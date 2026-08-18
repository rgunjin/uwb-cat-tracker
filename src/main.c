#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "deca_device_api.h"
#include "deca_port.h"
#include "deca_regs.h"

#include "dw1000_config.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
	if (deca_port_init() != 0) {
		return -ENODEV;
	}

	reset_DW1000();
	port_set_dw1000_slowrate();

	if (dwt_initialise(DWT_LOADUCODE) == DWT_ERROR) {
		LOG_ERR("dwt_initialise failed");
		return -EIO;
	}

    uint32 otp[5];

    dwt_otpread(0x006, &otp[0], 1);   /* PART_ID   */
    dwt_otpread(0x007, &otp[1], 1);   /* LOT_ID    */
    dwt_otpread(0x019, &otp[2], 1);   /* CH5 TX power PRF64 */
    dwt_otpread(0x01C, &otp[3], 1);   /* antenna delay */
    dwt_otpread(0x01E, &otp[4], 1);   /* XTAL_TRIM */

    LOG_INF("PART_ID: 0x%08X  LOT_ID: 0x%08X", otp[0], otp[1]);
    LOG_INF("XTAL_TRIM: %u", otp[4] & 0x1F);
    LOG_INF("ANT_DELAY PRF64: %u", otp[3] >> 16);
    LOG_INF("CH5 TX_POWER: 0x%08X", otp[2]);

	port_set_dw1000_fastrate();

	LOG_INF("DW1000 initialised, DEV_ID: 0x%08X", dwt_readdevid());

    uint8 xtalt;
    dwt_readfromdevice(0x2B, 0x0E, 1, &xtalt);
    LOG_INF("FS_XTALT: 0x%02X (trim %u)", xtalt, xtalt & 0x1F);

    dwt_configure((dwt_config_t *)&dw1000_config);

    dwt_settxantennadelay(DW1000_ANT_DELAY);
    dwt_setrxantennadelay(DW1000_ANT_DELAY);

    LOG_INF("configured: ch%u, PRF64, 6.8M, PLEN128, code 9",
	    dw1000_config.chan);

    uint8 cc_raw[4];
    dwt_readfromdevice(0x1F, 0, 4, cc_raw);
    uint32 cc = cc_raw[0] | (cc_raw[1] << 8) |
	    (cc_raw[2] << 16) | (cc_raw[3] << 24);

    LOG_INF("CHAN_CTRL: 0x%08X  (TX ch %u, RX ch %u)",
	    cc, cc & 0x0F, (cc >> 4) & 0x0F);

    uint8 tx_msg[] = { 'H', 'E', 'L', 'L', 'O' };

    dwt_writetxdata(sizeof(tx_msg) + 2, tx_msg, 0);
    dwt_writetxfctrl(sizeof(tx_msg) + 2, 0, 0);

    uint32 status;

    /* Clear all stale TX flags before starting. */
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_TX);
    dwt_starttx(DWT_START_TX_IMMEDIATE);

    int timeout = 10000;
    do {
	    status = dwt_read32bitreg(SYS_STATUS_ID);
	    if (--timeout == 0) {
		    LOG_ERR("TX timeout, SYS_STATUS: 0x%08X", status);
		    return -ETIMEDOUT;
	    }
    } while (!(status & SYS_STATUS_TXFRS));

    LOG_INF("TXFRS set, SYS_STATUS: 0x%08X", status);

    /* Clear TXFRS (write-1-to-clear), otherwise the next transmission
     * would see a stale flag. */
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS);

	return 0;
}
