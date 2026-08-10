#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/spi.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define DW1000_NODE     DT_NODELABEL(ieee802154)

static const struct spi_dt_spec dw_spi = SPI_DT_SPEC_GET(
        DW1000_NODE,
        SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER,
        0);

int main(void) {
    LOG_INF("uwb-cat-tracker starting");

    uint8_t tx_buf[5] = { 0 };
    uint8_t rx_buf[5] = { 0 };

    const struct spi_buf tx = { .buf = tx_buf, .len = sizeof(tx_buf) };
    const struct spi_buf rx = { .buf = rx_buf, .len = sizeof(rx_buf) };
    const struct spi_buf_set tx_set = { .buffers = &tx, .count = 1 };
    const struct spi_buf_set rx_set = { .buffers = &rx, .count = 1 };

    if (!spi_is_ready_dt(&dw_spi)) {
        LOG_ERR("SPI device not ready");
        return -ENODEV;
    }

    k_sleep(K_MSEC(10));

    int err = spi_transceive_dt(&dw_spi, &tx_set, &rx_set);
    if (err) {
        LOG_ERR("spi_transceive_dt is failed: %d", err);
        return err;
    }

    uint32_t dev_id = rx_buf[1] | (rx_buf[2] << 8) |
                     (rx_buf[3] << 16) | (rx_buf[4] << 24);

    LOG_INF("raw: %02x %02x %02x %02x %02x",
            rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3], rx_buf[4]);
    LOG_INF("DEV_ID: 0x%08X", dev_id);

    if (dev_id == 0xDECA0130) {
		LOG_INF("DW1000 detected");
	} else {
		LOG_ERR("unexpected DEV_ID");
	}

    return 0;
}
