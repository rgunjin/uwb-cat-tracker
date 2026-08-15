#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "dw1000_spi.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* OTP interface, register 0x2D */
#define OTP_IF          0x2D
#define OTP_ADDR        0x04    /* 2 bytes: address to read */
#define OTP_CTRL        0x06    /* 1 byte: control bits */
#define OTP_RDAT        0x0A    /* 4 bytes: read data */

#define OTP_CTRL_OTPRDEN    0x01
#define OTP_CTRL_OTPREAD    0x02

/* OTP addresses, DWM1001 datasheet 2.1.4 */
#define OTP_PART_ID     0x006
#define OTP_LOT_ID      0x007
#define OTP_CH5_PWR64   0x019
#define OTP_ANT_DELAY   0x01C
#define OTP_XTAL_TRIM   0x01E

static int otp_read(uint16_t addr, uint32_t *out) {
    uint8_t a[2] = { addr & 0xFF, (addr >> 8) & 0xFF };
    uint8_t ctrl;
    uint8_t rd[4];
    int err;

    err = dw1000_write(OTP_IF, OTP_ADDR, a, sizeof(a));
    if (err) {
        return err;
    }

    ctrl = OTP_CTRL_OTPRDEN | OTP_CTRL_OTPREAD;
    err = dw1000_write(OTP_IF, OTP_CTRL, &ctrl, 1);
    if (err) {
        return err;
    }

    /* OTPREAD self-clears, OTPRDEN does not */
    ctrl = 0x00;
    err = dw1000_write(OTP_IF, OTP_CTRL, &ctrl, 1);
    if (err) {
        return err;
    }

    err = dw1000_read(OTP_IF, OTP_RDAT, rd, sizeof(rd));
    if (err) {
        return err;
    }

    *out = rd[0] |(rd[1] << 8) | (rd[2] << 16) | (rd[3] << 24);
    return 0;
}

int main(void) {
    uint8_t dev_id[4];
    uint32_t part, lot, xtal, adly, pwr;

    if (dw1000_spi_init() != 0) {
		return -ENODEV;
	}

	if (dw1000_read(0x00, 0, dev_id, sizeof(dev_id)) != 0) {
        LOG_ERR("DEV_ID read failed");
        return -EIO;
    }
    LOG_INF("DEV_ID: %02x %02x %02x %02x", dev_id[3], dev_id[2], dev_id[1], dev_id[0]);

    if (otp_read(OTP_PART_ID, &part) || otp_read(OTP_LOT_ID, &lot) ||
        otp_read(OTP_XTAL_TRIM, &xtal) || otp_read(OTP_ANT_DELAY, &adly) ||
        otp_read(OTP_CH5_PWR64, &pwr)) {
        LOG_ERR("OTP read failed");
        return -EIO;
    }
    LOG_INF("PART_ID: 0x%08X   LOT_ID: 0x%08X", part, lot);
    LOG_INF("XTAL_TRIM: %u   (OTP rev %u)", xtal & 0x1F, (xtal >> 8) & 0xFF);
    LOG_INF("ANT_DELAY PRF64: %u   PRF16: %u", adly >> 16, adly & 0xFFFF);
    LOG_INF("CH5 TX_POWER PRF64: 0x%08X", pwr);

    return 0;
}
