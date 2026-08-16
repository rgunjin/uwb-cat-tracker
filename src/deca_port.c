#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "deca_device_api.h"

LOG_MODULE_REGISTER(deca_port, LOG_LEVEL_INF);

#define DW1000_NODE DT_NODELABEL(ieee802154)
#define DW_SPI_OP (SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER)

/* 8 MHz, straight from the devicetree */
static const struct spi_dt_spec dw_spi_fast =
	SPI_DT_SPEC_GET(DW1000_NODE, DW_SPI_OP);

/* Same node, clocked down. The DW1000 needs a slow SPI clock until
 * the PLL has locked and after wake-up. */
static const struct spi_dt_spec dw_spi_slow = {
	.bus = DEVICE_DT_GET(DT_BUS(DW1000_NODE)),
	.config = SPI_CONFIG_DT(DW1000_NODE, DW_SPI_OP),
	.config.frequency = 2000000,
};

/* The rate currently in use. Starts slow — dwt_initialise() runs
 * before the PLL is up. */
static const struct spi_dt_spec *dw_spi = &dw_spi_slow;

static const struct gpio_dt_spec dw_rst =
	GPIO_DT_SPEC_GET(DW1000_NODE, reset_gpios);

/*
 * Hooks called by the vendor driver.
 */

int readfromspi(uint16 headerLength, const uint8 *headerBuffer,
                uint32 readlength, uint8 *readBuffer)
{
    const struct spi_buf tx_bufs[] = {
        { .buf = (void *)headerBuffer, .len = headerLength },
    };
    const struct spi_buf rx_bufs[] = {
        { .buf = NULL,       .len = headerLength },
        { .buf = readBuffer, .len = readlength   },
    };
    const struct spi_buf_set tx = { .buffers = tx_bufs, .count = 1 };
    const struct spi_buf_set rx = { .buffers = rx_bufs, .count = 2 };

    return spi_transceive_dt(dw_spi, &tx, &rx);
}

int writetospi(uint16 headerLength, const uint8 *headerBuffer,
               uint32 bodylength, const uint8 *bodyBuffer)
{
    const struct spi_buf tx_bufs[] = {
        { .buf = (void *)headerBuffer, .len = headerLength },
        { .buf = (void *)bodyBuffer,   .len = bodylength   },
    };
    const struct spi_buf_set tx = { .buffers = tx_bufs, .count = 2 };

    return spi_write_dt(dw_spi, &tx);
}

void deca_sleep(unsigned int time_ms)
{
    k_msleep(time_ms);
}

decaIrqStatus_t decamutexon(void)
{
    return 0;
}

void decamutexoff(decaIrqStatus_t s)
{
    ARG_UNUSED(s);
}

/*
 * Called from the application.
 */

void port_set_dw1000_slowrate(void)
{
    dw_spi = &dw_spi_slow;
}

void port_set_dw1000_fastrate(void)
{
    dw_spi = &dw_spi_fast;
}

/* RSTn is open-drain: pull low to reset, then release to
 * high-impedance. It must never be driven high. */
void reset_DW1000(void)
{
	gpio_pin_configure_dt(&dw_rst, GPIO_OUTPUT_ACTIVE);
	k_msleep(2);
	gpio_pin_configure_dt(&dw_rst, GPIO_INPUT);
	k_msleep(2);
}

int deca_port_init(void)
{
	if (!spi_is_ready_dt(&dw_spi_fast)) {
		LOG_ERR("SPI bus not ready");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&dw_rst)) {
		LOG_ERR("reset GPIO not ready");
		return -ENODEV;
	}

	return 0;
}
