#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "deca_device_api.h"
#include "deca_port.h"
#include "deca_regs.h"
#include "dw1000_config.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static void dump_device_info(void) {
    uint32 otp[5];
    uint8 xtalt;

    dwt_otpread(0x006, &otp[0], 1);   /* PART_ID   */
	dwt_otpread(0x007, &otp[1], 1);   /* LOT_ID    */
	dwt_otpread(0x019, &otp[2], 1);   /* CH5 TX power, PRF 64 */
	dwt_otpread(0x01C, &otp[3], 1);   /* antenna delay */
	dwt_otpread(0x01E, &otp[4], 1);   /* XTAL_TRIM */

	dwt_readfromdevice(0x2B, 0x0E, 1, &xtalt);

	LOG_INF("PART_ID    0x%08X", otp[0]);
	LOG_INF("LOT_ID     0x%08X", otp[1]);
	LOG_INF("XTAL_TRIM  %u  (FS_XTALT 0x%02X)", otp[4] & 0x1F, xtalt);
	LOG_INF("ANT_DELAY  %u", otp[3] >> 16);
	LOG_INF("TX_POWER   0x%08X", otp[2]);
}


static void run_tx(void) {
    uint8 tx_msg[] = { 'H', 'E', 'L', 'L', 'O' };
    uint32 count = 0;
    uint32 status;
    
    while (1) {
        /* Clear leftover TX flags. SYS_STATUS bits are write-1-to-clear:
         * writing a 1 clears that bit, writing a 0 leaves it alone. Passing
         * the mask therefore clears every TX flag and touches nothing else.
         * Without this the poll below would see a flag from the previous
         * frame and return immediately. */
        dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_TX);

        dwt_writetxdata(sizeof(tx_msg) + 2, tx_msg, 0);
        dwt_writetxfctrl(sizeof(tx_msg) + 2, 0, 0);
        dwt_starttx(DWT_START_TX_IMMEDIATE);

        int timeout = 10000;
        do {
	        status = dwt_read32bitreg(SYS_STATUS_ID);
	        if (--timeout == 0) {
		        LOG_ERR("TX timeout, SYS_STATUS: 0x%08X", status);
                break;
	        }
        } while (!(status & SYS_STATUS_TXFRS));

        LOG_INF("frame %u sent: ", ++count);
        k_msleep(500);
    }
}

static void run_rx(void) {
    uint8 rx_buf[32];
    uint32 status;
    uint32 count = 0;

    /* No timeout: wait indefinitely for a frame */
    dwt_setrxtimeout(0);

    while (1) {
        /* Same write-1-to-clear as in run_tx(), for the RX flags. */
        dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_GOOD | SYS_STATUS_ALL_RX_ERR);

        dwt_rxenable(DWT_START_RX_IMMEDIATE);

        uint32_t start = k_uptime_get_32();

        do {
            status = dwt_read32bitreg(SYS_STATUS_ID);

            if (k_uptime_get_32() - start > 2000) {
                LOG_WRN("no frame in 2 s, SYS_STATUS 0x%08X  "
					"PRD:%d SFDD:%d PHE:%d FCE:%d SFDTO:%d",
					status,
					!!(status & SYS_STATUS_RXPRD),
					!!(status & SYS_STATUS_RXSFDD),
					!!(status & SYS_STATUS_RXPHE),
					!!(status & SYS_STATUS_RXFCE),
					!!(status & SYS_STATUS_RXSFDTO));
				break;
            }
        } while (!(status & (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_ERR)));

        if (status & SYS_STATUS_RXFCG) {
            uint32 finfo = dwt_read32bitreg(RX_FINFO_ID);
            uint16 len = finfo & RX_FINFO_RXFLEN_MASK;

            if (len > sizeof(rx_buf)) {
                len = sizeof(rx_buf);
            }

            dwt_readrxdata(rx_buf, len, 0);
            LOG_INF("frame: %u, %u bytes: %.*s", ++count, len, len - 2, rx_buf);
        } else if (status & SYS_STATUS_ALL_RX_ERR) {
            LOG_WRN("RX error, SYS_STATUS 0x%08X", status);
            dwt_rxreset();
        }
    }
}

static int dw1000_setup(void) {
	if (deca_port_init() != 0) {
		return -ENODEV;
	}

	reset_DW1000();
	port_set_dw1000_slowrate();

	if (dwt_initialise(DWT_LOADUCODE) == DWT_ERROR) {
		LOG_ERR("dwt_initialise failed");
		return -EIO;
	}

    /* OTP must be read at the slow SPI rate. At 8 MHz the reads
	 * return 0xFF and leave the OTP interface hung, after which
	 * even a plain DEV_ID read fails. */
#if defined (CONFIG_UWB_DUMP_INFO)
    dump_device_info();
#endif

	port_set_dw1000_fastrate();
    LOG_INF("DEV_ID after fastrate: 0x%08X", dwt_readdevid());
    
    dwt_configure((dwt_config_t *)&dw1000_config);
    dwt_settxantennadelay(DW1000_ANT_DELAY);
    dwt_setrxantennadelay(DW1000_ANT_DELAY);

    LOG_INF("DW1000 ready, DEV_ID 0x%08X, ch%u",
		dwt_readdevid(), dw1000_config.chan);

    return  0;
}


int main(void) {
    if (dw1000_setup() != 0) {
        return  -EIO;
    }

#if defined (CONFIG_UWB_ROLE_TX)
    run_tx();
#elif defined (CONFIG_UWB_ROLE_RX)
    run_rx();
#else
#error "No role selected"
#endif

    return 0;
}
