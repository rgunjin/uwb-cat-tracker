#include <stdint.h>
#include <sys/errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "uwb_radio.h"
#include "deca_device_api.h"
#include "deca_regs.h"
#include "deca_types.h"

LOG_MODULE_REGISTER(uwb_radio, LOG_LEVEL_INF);

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

int uwb_receive(uint8_t *buf, uint16_t buf_size, uint16_t *len, uint16_t timeout_uus)
{
    uint32 status;
     
    dwt_setrxtimeout(timeout_uus);
    
    /* Write-1-to-clear, same as in uwb_send(). The timeout flag goes
     * too: a leftover RXRFTO would make the next call return immediately. */
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_GOOD |
                                     SYS_STATUS_ALL_RX_ERR |
                                     SYS_STATUS_ALL_RX_TO);

    dwt_rxenable(DWT_START_RX_IMMEDIATE);

    uint32_t guard_start = k_uptime_get_32();

    do {
        status = dwt_read32bitreg(SYS_STATUS_ID);

        /* Hardware guard, not the timeout: the chip owns that now. This
         * only catches a chip that stopped answering at all. */
        if (timeout_uus != 0 && k_uptime_get_32() - guard_start > 1000) {
            dwt_forcetrxoff();
            return -ETIMEDOUT;
        }
    } while (!(status & (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_ERR | SYS_STATUS_ALL_RX_TO)));

    if (status & SYS_STATUS_RXFCG) {
        uint32 finfo = dwt_read32bitreg(RX_FINFO_ID);
        uint16_t rx_len = finfo & RX_FINFO_RXFLEN_MASK;

        /* The length in RX_FINFO includes the two CRC bytes. */
        if (rx_len < 2) {
            return -EIO;
        }
        rx_len -= 2;

        if (rx_len > buf_size) {
            LOG_ERR("frame %u bytes, buffer %u", rx_len, buf_size);
            return -EMSGSIZE;
        }

        dwt_readrxdata(buf, rx_len, 0);
        *len = rx_len;

        return 0;
    }

    if (status & SYS_STATUS_ALL_RX_TO) {
        dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_TO);
        return -ETIMEDOUT;
    }

    LOG_WRN("RX error, SYS_STATUS 0x%08X  PRD:%d SFDD:%d PHD:%d "
	"PHE:%d FCE:%d RFSL:%d SFDTO:%d LDEERR:%d",
	status,
	!!(status & SYS_STATUS_RXPRD),
	!!(status & SYS_STATUS_RXSFDD),
	!!(status & SYS_STATUS_RXPHD),
	!!(status & SYS_STATUS_RXPHE),
	!!(status & SYS_STATUS_RXFCE),
	!!(status & SYS_STATUS_RXRFSL),
	!!(status & SYS_STATUS_RXSFDTO),
	!!(status & SYS_STATUS_LDEERR));

	dwt_rxreset();

	return -EIO;
}

/* The chip returns timestamp bytes least significant first, so the loop
 * runs backwards: shift what we have up, then add the next byte down
 * from the top. Shared by uwb_rx_timestamp() and uwb_tx_timestamp(),
 * which differ only in which register the bytes come from. */
static uint64_t ts40_unpack(const uint8_t ts_tab[5])
{
    uint64_t ts = 0;

    for (int i = 4; i >= 0; i--) {
        ts <<= 8;
        ts |= ts_tab[i];
    }

    return ts;
}

uint64_t uwb_rx_timestamp(void)
{
    uint8_t ts_tab[5];

    dwt_readrxtimestamp(ts_tab);

    return ts40_unpack(ts_tab);
}

uint64_t uwb_tx_timestamp(void)
{
    uint8_t ts_tab[5];

    dwt_readtxtimestamp(ts_tab);

    return ts40_unpack(ts_tab);
}

int uwb_send_delayed(const uint8_t *data, uint16_t len)
{
    /* Same write-1-to-clear as in uwb_send(). */
	dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_TX);

    dwt_writetxdata(len + 2, (uint8 *)data, 0);
	dwt_writetxfctrl(len + 2, 0, 0);

    if (dwt_starttx(DWT_START_TX_DELAYED) != DWT_SUCCESS) {
        uint32 status;
        /* The scheduled moment has already passed. Nothing was sent,
		 * so there is no point waiting for TXFRS. */
		status = dwt_read32bitreg(SYS_STATUS_ID);
		LOG_WRN("delayed TX rejected, HPDWARN %d",
			!!(status & SYS_STATUS_HPDWARN));
		return -ETIME;
    }

    /* Scheduled, not sent. The chip stays in IDLE until the moment
     * arrives (UM 2.3) and then transmits on its own; there is
     * nothing here to wait for. Blocking on TXFRS would only hold
     * the CPU through a delay the hardware already handles — and
     * cancelling on timeout, as this used to do, aborted a
     * transmission that was still pending. Whether the frame
     * actually went out is answered by the peer receiving it. */
    return 0;
}
