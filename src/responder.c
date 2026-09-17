#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <deca_device_api.h>

#include "storage.h"
#include "uwb_radio.h"
#include "uwb_msg.h"
#include "responder.h"

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
 *  The three anchors share one firmware image and tell their slot
 *  apart by their own short address (SLOT_RESP_A1 / A2 / A3, see
 *  uwb_msg.h), so the delay is looked up once at startup rather than
 *  fixed at compile time. */

LOG_MODULE_REGISTER(responder, LOG_LEVEL_INF);

void run_responder(void)
{
    uint8_t buf[64];
    uint16_t len;
    uint32_t count = 0;
    uint32_t late = 0;
    uint32_t bad = 0;
    uint32_t other = 0;
    uint32_t finals = 0;
    uint32_t ts_zero = 0;
    uint32_t resp_dly_uus;

    uint16_t ant_dly = storage_get_ant_dly();

    /* State of the cycle in progress, kept across iterations: the
     * Poll arrives in one pass through the loop and the Final in a
     * later one, so these cannot be locals inside it. cycle_seq is
     * the Poll's sequence number and marks the state valid — a
     * Final carrying a different one belongs to a cycle this anchor
     * missed, and its timestamps must not be paired with ours. */
    bool     cycle_open  = false;
    uint8_t  cycle_seq   = 0;
    uint64_t poll_rx_ts  = 0;
    uint64_t resp_tx_ts  = 0;

    switch (uwb_my_addr) {
    case UWB_ADDR_A1:
        resp_dly_uus = SLOT_RESP_A1;
        break;
    case UWB_ADDR_A2:
        resp_dly_uus = SLOT_RESP_A2;
        break;
    case UWB_ADDR_A3:
        resp_dly_uus = SLOT_RESP_A3;
        break;
    default:
        LOG_ERR("addr 0x%04X matches no anchor slot (A1 0x%04X, A2 0x%04X, A3 0x%04X); "
                "responder not starting",
                uwb_my_addr, UWB_ADDR_A1, UWB_ADDR_A2, UWB_ADDR_A3);
        return;
    }

    LOG_INF("responder started, addr 0x%04X, resp slot delay %u UUS",
            uwb_my_addr, resp_dly_uus);

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


        if (rx->hdr.dst != uwb_my_addr &&
            rx->hdr.dst != UWB_ADDR_BCAST) {
            other++;
            goto next;
        }

        /* ---- Final: closes the cycle this anchor answered ---- */
        if (rx->type == MSG_FINAL) {
            if (len < sizeof(struct uwb_final_msg)) {
                bad++;
                goto  next;
            }

            if (!cycle_open || rx->hdr.seq != cycle_seq) {
                /* A Final for a cycle whose Poll we never saw, or
                 * whose Response never went out. Its timestamps
                 * cannot be paired with ours. */
                other++;
                goto next;
            }

            uint64_t final_rx_ts = uwb_rx_timestamp();

            if (final_rx_ts == 0) {
                ts_zero++;
            }

            finals++;
            cycle_open = false;
            
            /* Four of the six timestamps a range needs are now in
             * hand; the tag's three travel in the frame. Nothing is
             * computed here yet — that is the next step. */
            LOG_DBG("final seq %u: poll_rx %llu resp_tx %llu final_rx %llu",
                    rx->hdr.seq, poll_rx_ts, resp_tx_ts, final_rx_ts);

            goto next;
        }

        /* ---- Poll: opens a cycle ---- */
        if (rx->type != MSG_POLL) {
            bad++;
            goto next;
        }
        /* Full 40 bits: the addition below would overflow in 32,
		 * and DX_TIME needs bits 8..39 after the shift. */
        poll_rx_ts = uwb_rx_timestamp();

        uint32_t tx_time = (poll_rx_ts + (resp_dly_uus * UUS_TO_DWT_TIME)) >> 8;

        dwt_setdelayedtrxtime(tx_time);

        /* Same three corrections as final_tx_ts in initiator.c: mask
         * the ninth ignored bit, back to the 40-bit scale, then add
         * the antenna delay DX_TIME leaves out (UM 3.3). */
        resp_tx_ts = ((uint64_t)(tx_time & 0xFFFFFFFEUL) << 8) + ant_dly;

        struct uwb_msg reply = {
            .hdr = {
                .fc = { UWB_FC0, UWB_FC1 },
                .seq = rx->hdr.seq,
                .pan = UWB_PAN,
                .dst = rx->hdr.src,
                .src = uwb_my_addr,
            },
            .type = MSG_RESPONSE,
        };

        if (uwb_send_delayed((uint8_t *)&reply, sizeof(reply)) != 0) {
            late++;
            LOG_WRN("delayed TX failed, poll %u (%u late)",
                    rx->hdr.seq, late);
            goto next;
        }

        cycle_seq  = rx->hdr.seq;
        cycle_open = true;
        count++;

next:
        if (((count + bad + late) % 20) == 0 && count > 0) {
            LOG_INF("replies: %u, late %u, bad %u", count, late, bad);
            LOG_INF("finals: %u, ts_zero %u", finals, ts_zero);
        }

        /* Give the log thread a slot. Safe here: the reply is
         * already on air, so nothing time-critical is pending. */
        k_msleep(1);
    }
}
