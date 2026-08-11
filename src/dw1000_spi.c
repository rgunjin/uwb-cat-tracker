#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include "dw1000_spi.h"

LOG_MODULE_REGISTER(dw1000_spi, LOG_LEVEL_INF);

#define DW1000_NODE     DT_NODELABEL(ieee802154)

static const struct spi_dt_spec dw_spi = SPI_DT_SPEC_GET(
        DW1000_NODE,
        SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER);

static size_t dw_header(uint8_t *hdr, uint8_t reg, uint16_t off, bool write)
{
    hdr[0] = reg & 0x3F;
    
    if (write) {
        hdr[0] |= 0x80;
    }

    if (off == 0) {
        return 1;
    }
    hdr[0] |= 0x40;

    if (off <= 0x7F) {
        hdr[1] = off & 0x7F;
        return 2;
    }

    hdr[1] = 0x80 | (off & 0x7F);
    hdr[2] = (off >> 7) & 0xFF;
    return 3;
}

int dw1000_spi_init(void)
{
    if (!spi_is_ready_dt(&dw_spi)) {
        LOG_ERR("SPI device not ready");
        return -ENODEV;
    }
    return 0;
}

int dw1000_read(uint8_t reg, uint16_t off, uint8_t *data, size_t len)
{
    uint8_t hdr[3];
    size_t hlen = dw_header(hdr, reg, off, false);

    const struct spi_buf tx_bufs[] = {
        { .buf = hdr, .len = hlen },
    };
    const struct spi_buf rx_bufs[] = {
        { .buf = NULL, .len = hlen },
        { .buf = data, .len = len },
    };
    const struct spi_buf_set tx = { .buffers = tx_bufs, .count = 1 };
    const struct spi_buf_set rx = { .buffers = rx_bufs, .count = 2 };

    return spi_transceive_dt(&dw_spi, &tx, &rx);
}

int dw1000_write(uint8_t reg, uint16_t off, const uint8_t *data, size_t len)
{
    uint8_t hdr[3];
    size_t hlen = dw_header(hdr, reg, off, true);

    const struct spi_buf tx_bufs[] = {
        { .buf = hdr,          .len = hlen },
        { .buf = (void *)data, .len = len },
    };
    const struct spi_buf_set tx = { .buffers = tx_bufs, .count = 2 };

    return spi_write_dt(&dw_spi, &tx);
}
