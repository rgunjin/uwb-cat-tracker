#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <deca_device_api.h>

#include "uwb_radio.h"
#include "uwb_msg.h"
#include "responder.h"
#include "dw1000_config.h"

/*! One UWB microsecond (UUS) in device time units.
 *
 *  A UUS is 512 / 499.2 MHz, roughly 1.0256 us — the same unit
 *  dwt_setrxtimeout() takes. A device time unit (DTU) is
 *  1 / (499.2 MHz * 128), roughly 15.65 ps. One UUS therefore holds
 *  65536 DTU. Both derive from the 499.2 MHz base frequency of the
 *  IEEE 802.15.4 UWB standard. */
#define UUS_TO_DWT_TIME             65536

/*! Delay from receiving the poll to transmitting the response, in UUS.
 *
 *  This is not a free choice. The responder must have time to read the
 *  RX timestamp, compute the transmit time, build the frame and load
 *  it into the TX buffer before the scheduled moment arrives. Too
 *  short and dwt_starttx() returns DWT_ERROR with HPDWARN set, and no
 *  reply goes out at all.
 *
 *  Too long is not free either: the SS-TWR clock error scales with
 *  T_reply, so every microsecond here shows up in the range estimate
 *  (UM Table 65).
 *
 *  1100 is the value Decawave ship for the nRF52; their comment says
 *  800 might work but was never tested. Worth tightening once the
 *  exchange is reliable, watching for DWT_ERROR at each step. */
#define POLL_RX_TO_RESP_TX_DLY_UUS  1100

LOG_MODULE_REGISTER(responder, LOG_LEVEL_INF);

void run_responder(void)
{
    uint8_t buf[32];
    uint16_t len;
    uint32_t count = 0;
    uint32_t late = 0;
    uint32_t bad = 0;

    LOG_INF("responder started");

    while (1) {
        /* No timeout: the responder has nothing else to do. */
        if (uwb_receive(buf, sizeof(buf), &len, 0) != 0) {
            bad++;
            goto next;
        }

        if (len < sizeof(struct uwb_msg)) {
            bad++;
            goto next;
        }

        struct uwb_msg *rx = (struct uwb_msg *)buf;

        if (rx->type != MSG_POLL) {
            bad++;
            goto next;
        }

        /* Full 40 bits: the addition below would overflow in 32,
		 * and DX_TIME needs bits 8..39 after the shift. */
        uint64_t poll_rx_ts = uwb_rx_timestamp();

        uint32_t tx_time = (poll_rx_ts + (POLL_RX_TO_RESP_TX_DLY_UUS * UUS_TO_DWT_TIME)) >> 8;

        dwt_setdelayedtrxtime(tx_time);

        /* DX_TIME specifies the RMARKER without the antenna delay
		 * (UM 3.3), and its low 9 bits are ignored — hence the mask
		 * and the addition. */
		uint32_t resp_tx_ts =
			((tx_time & 0xFFFFFFFEUL) << 8) + DW1000_ANT_DELAY;

        struct uwb_resp_msg reply = {
            .hdr = {
                .type = MSG_RESPONSE,
                .seq = rx->seq,
            },
            .poll_rx_ts = (uint32_t)poll_rx_ts,
            .resp_tx_ts = resp_tx_ts,
        };

        if (uwb_send_delayed((uint8_t *)&reply, sizeof(reply)) != 0) {
            late++;
            LOG_WRN("delayed TX failed, poll %u (%u late)",
                    rx->seq, late);
            goto next;
        }

        count++;

next:
        if ((count % 20) == 0 && count > 0) {
            LOG_INF("replies: %u, late %u, bad %u",
                    count, late, bad);
        }

         /* Give the log thread a slot. Safe here: the reply is
         * already on air, so nothing time-critical is pending. */
        k_msleep(1);
    }
}
