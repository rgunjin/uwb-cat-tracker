#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <deca_device_api.h>

#include "uwb_radio.h"
#include "uwb_msg.h"
#include "initiator.h"
#include "storage.h"

LOG_MODULE_REGISTER(initiator, LOG_LEVEL_INF);

/* Gap between exchanges */
#define POLL_INTERVAL_MS    500

/* How many exchanges between summary lines. Read by the HIL gate;
 * see uwb-protocol-slots.md, "Формат лога — контракт с гейтом". */
#define STATS_WINDOW        10

/* Anchors the tag can hear from, in slot order. */
static const uint16_t anchor_addr[3] = {
    UWB_ADDR_A1, UWB_ADDR_A2, UWB_ADDR_A3,
};

static int anchor_index(uint16_t addr)
{
    for (int i = 0; i < 3; i++) {
        if (anchor_addr[i] == addr) {
            return i;
        }
    }

    return -1;
}

void run_initiator(void)
{
    uint8_t buf[64];
    uint16_t len;
    uint8_t seq = 0;
    uint32_t n = 0;
    uint32_t ok = 0;
    uint32_t lost = 0;

    uint16_t ant_dly = storage_get_ant_dly();

    LOG_INF("initiator started, addr 0x%04X", uwb_my_addr);

    while (1) {
        struct uwb_msg poll = {
            .hdr = {
                .fc = { UWB_FC0, UWB_FC1 },
                .seq = seq,
                .pan = UWB_PAN,
                .dst = UWB_ADDR_BCAST,
                .src = uwb_my_addr,
            },
            .type = MSG_POLL,
        };

        if (uwb_send((uint8_t *)&poll, sizeof(poll)) != 0) {
            LOG_WRN("poll %u not sent", seq);
            seq++;
            k_msleep(POLL_INTERVAL_MS);
            continue;
        }

        /* Final's own delayed-TX schedule and its poll_tx_ts field
         * both need the full 40 bits, read right after TXFRS so the
         * antenna delay adjustment in dwt_readtxtimestamp() is not
         * lost the way a 32-bit reading would. */
        uint64_t poll_tx_ts = uwb_tx_timestamp();

        bool answered[3] = { false, false, false };
        uint64_t resp_rx_ts[3] = { 0 };
        int n_answered = 0;

         /* One window per anchor slot. dwt_setrxtimeout() takes a
         * duration from the moment the receiver is enabled, not a
         * point on the schedule, so these are the gaps between
         * slots: the first covers Poll to A1's slot, the rest one
         * slot each. */
        static const uint32_t slot_window_uus[3] = {
            2 * SLOT_UUS,   /* 1200: room for A1 to answer */
            SLOT_UUS,       /* 600:  A2 */
            SLOT_UUS,       /* 600:  A3 */
        };

        for (int slot = 0; slot < 3 && n_answered < 3; slot++) {
            int err = uwb_receive(buf, sizeof(buf), &len, slot_window_uus[slot]);

            if (err == -ETIMEDOUT) {
                /* Nobody answered in this window. The next anchor
                 * may still come, so keep going. */
                continue;
            }

            if (err != 0) {
                /* A garbled frame; the window may still hold a
                 * genuine response from another anchor. */
                continue;
            }

            if (len < sizeof(struct uwb_msg)) {
                continue;
            }

            struct uwb_msg *rx = (struct uwb_msg *)buf;

            if (rx->type != MSG_RESPONSE ||
                rx->hdr.seq != seq ||
                rx->hdr.dst != uwb_my_addr) {
                continue;
            }

            int idx = anchor_index(rx->hdr.src);

            if (idx < 0) {
                LOG_WRN("response from unknown addr 0x%04X", rx->hdr.src);
                continue;
            }

            if (answered[idx]) {
                continue;
            }

            /* Latched by the chip at reception; stable until the next
             * dwt_rxenable(), which does not happen before this is read. */
            resp_rx_ts[idx] = uwb_rx_timestamp();
            answered[idx] = true;
            n_answered++;
            LOG_DBG("poll %u: response from 0x%04X (A%d)",
                    seq, rx->hdr.src, idx + 1);
        }

        for (int i = 0; i < 3; i++) {
            if (!answered[i]) {
                LOG_DBG("poll %u: no response from A%d", seq, i + 1);
            }
        }

        /* Final's slot is counted from the tag's own poll_tx_ts, not
         * from anything an anchor sent — this node has that timestamp
         * already, so the delay is computed the same way responder.c
         * derives its response's transmit time from poll_rx_ts:
         * schedule first, then rebuild the 40-bit timestamp from the
         * scheduled value instead of reading it back after the fact. */
        uint32_t final_tx_time =
            (uint32_t)((poll_tx_ts + ((uint64_t)SLOT_FINAL * UUS_TO_DWT_TIME)) >> 8);

        dwt_setdelayedtrxtime(final_tx_time);

        /* Work out the timestamp the Final will carry.
         *
         * The tag cannot read its own transmit timestamp here — the
         * frame has not gone out yet, and by the time it has, the
         * timestamp would have to be inside it already. Delayed
         * transmission breaks that circle: the moment is scheduled in
         * advance, so it can be computed rather than measured. Same
         * reasoning as resp_tx_ts in responder.c.
         *
         * Three corrections turn the scheduled time into the timestamp
         * the chip will actually record:
         *
         *   & 0xFFFFFFFE  DX_TIME ignores the low 9 bits of the 40-bit
         *                 time. Eight of them were dropped by the >> 8
         *                 above; this clears the ninth. Skip it and the
         *                 timestamp is off by up to 256 ticks, about
         *                 1.2 m of range.
         *
         *   << 8          back to the full 40-bit scale. The cast to
         *                 uint64_t comes first: in 32-bit arithmetic
         *                 the shift would throw away the top byte.
         *
         *   + ant_dly     DX_TIME specifies the RMARKER without the
         *                 antenna delay (UM 3.3), but a timestamp read
         *                 from the chip includes it. Computing one by
         *                 hand means adding it back.
         *
         * This value leaves the node and is used off-board to compute
         * a range, so an error here is silent: the frame is well
         * formed, the arithmetic downstream succeeds, and the answer
         * is simply wrong. */
        uint64_t final_tx_ts = ((uint64_t)(final_tx_time & 0xFFFFFFFEUL) << 8) + ant_dly;

        struct uwb_final_msg final = {
            .msg = {
                .hdr = {
                    .fc = { UWB_FC0, UWB_FC1 },
                    .seq = seq,
                    .pan = UWB_PAN,
                    .dst = UWB_ADDR_BCAST,
                    .src = uwb_my_addr,
                },
                .type = MSG_FINAL,
            },
        };

        uwb_ts40_pack(final.poll_tx_ts, poll_tx_ts);
        uwb_ts40_pack(final.final_tx_ts, final_tx_ts);

        for (int i = 0; i < 3; i++) {
            if (answered[i]) {
                final.anchors[i].addr = anchor_addr[i];
                uwb_ts40_pack(final.anchors[i].resp_rx_ts, resp_rx_ts[i]);
            }
            /* Left zeroed otherwise: addr 0x0000 marks a missed slot. */

            LOG_DBG("final %u: A%d addr 0x%04X resp_rx_ts 0x%010" PRIx64,
                    seq, i + 1, final.anchors[i].addr, resp_rx_ts[i]);
        }

        LOG_DBG("final %u: poll_tx_ts 0x%010" PRIx64 ", final_tx_ts 0x%010" PRIx64,
                seq, poll_tx_ts, final_tx_ts);

        if (uwb_send_delayed((uint8_t *)&final, sizeof(final)) != 0) {
            LOG_WRN("final %u not sent", seq);
        }

        n++;
        if (n_answered > 0) {
            ok++;
        } else {
            lost++;
        }

        if ((n % STATS_WINDOW) == 0) {
            LOG_INF("n=%u ok %u lost %u", n, ok, lost);
        }

        seq++;
        k_msleep(POLL_INTERVAL_MS);
    }
}
