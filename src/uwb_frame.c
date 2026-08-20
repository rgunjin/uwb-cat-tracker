#include <sys/errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "uwb_frame.h"
#include "deca_device_api.h"
#include "deca_regs.h"

LOG_MODULE_REGISTER(uwb_frame, LOG_LEVEL_INF);

int uwb_send(const uint8_t *data, uint16_t len)
{
    uint32 status;

    /* Clear leftover TX flags. SYS_STATUS bits are write-1-to-clear:
     * writing a 1 clears that bit, writing a 0 leaves it alone. Passing
     * the mask therefore clears every TX flag and touches nothing else.
     * Without this the poll below would see a flag from the previous
     * frame and return immediately. */
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_TX);
    
    /* The chip appends two CRC bytes itself, but they count towards
	 * the length in both calls below. */
	dwt_writetxdata(len + 2, (uint8 *)data, 0);
    dwt_writetxfctrl(len + 2, 0, 0);
    dwt_starttx(DWT_START_TX_IMMEDIATE);

    int timeout = 10000;
    do {
        status = dwt_read32bitreg(SYS_STATUS_ID);
        if (--timeout == 0) {
            LOG_ERR("TX timeout, SYS_STATUS 0x%08X", status);
            return -EIO;
        }
    } while (!(status & SYS_STATUS_TXFRS));

    return  0;
}

int uwb_receive(uint8_t *buf, uint16_t buf_size, uint16_t *len, uint32_t timeout_ms)
{
    uint32 status;
    uint32_t start;
     
    /* No timeout: wait indefinitely for a frame */
    dwt_setrxtimeout(0);
    
    /* Same write-1-to-clear as in run_tx(), for the RX flags. */
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_GOOD | SYS_STATUS_ALL_RX_ERR);

    dwt_rxenable(DWT_START_RX_IMMEDIATE);

    start = k_uptime_get_32();

    do {
        status = dwt_read32bitreg(SYS_STATUS_ID);

        if (timeout_ms != 0 && k_uptime_get_32() - start > timeout_ms) {
            dwt_forcetrxoff();
            return -ETIMEDOUT;
        }
    } while (!(status & (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_ERR)));

    if (status & SYS_STATUS_RXFCG) {
        uint32 finfo = dwt_read32bitreg(RX_FINFO_ID);
        uint16_t rx_len = finfo & RX_FINFO_RXFLEN_MASK;

        /* The length in RX_FINFO includes the two CRC bytes. */
        if (rx_len < 2) {
            return -EIO;
        }
        rx_len -= 2;

        if (rx_len > buf_size) {
            rx_len = buf_size;
        }

        dwt_readrxdata(buf, rx_len, 0);
        *len = rx_len;

        return 0;
    }

    LOG_WRN("RX error, SYS_STATUS 0x%08X  PRD:%d SFDD:%d "
		"PHE:%d FCE:%d SFDTO:%d",
		status,
		!!(status & SYS_STATUS_RXPRD),
		!!(status & SYS_STATUS_RXSFDD),
		!!(status & SYS_STATUS_RXPHE),
		!!(status & SYS_STATUS_RXFCE),
		!!(status & SYS_STATUS_RXSFDTO));

	dwt_rxreset();

	return -EIO;
}
