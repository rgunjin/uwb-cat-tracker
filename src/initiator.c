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

LOG_MODULE_REGISTER(initiator, LOG_LEVEL_INF);

/* How long the tag stays listening for Response frames after the Poll,
 * covering all three response slots. Not yet derived from SLOT_UUS /
 * SLOT_FINAL: uwb_receive() only takes a millisecond-resolution
 * timeout, far coarser than the ~3 ms the real schedule spans, so this
 * is a generous placeholder rather than a synced window. Tightening
 * it to the real schedule is follow-up work once the tag starts
 * scheduling its own delayed transmissions (Final, T013). */
#define RESPONSE_WINDOW_MS  100

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
    uint8_t buf[32];
    uint16_t len;
    uint8_t seq = 0;
    uint32_t n = 0;
    uint32_t ok = 0;
    uint32_t lost = 0;

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
        uint32_t win_start = k_uptime_get_32();

        while (n_answered < 3) {
            uint32_t elapsed = k_uptime_get_32() - win_start;

            if (elapsed >= RESPONSE_WINDOW_MS) {
                break;
            }

            int err = uwb_receive(buf, sizeof(buf), &len,
                                   RESPONSE_WINDOW_MS - elapsed);

            if (err == -ETIMEDOUT) {
                /* Nothing more is coming in this window. */
                break;
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
             * dwt_rxenable(), which does not happen before this is
             * read. */
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

        uint64_t final_tx_ts = ((uint64_t)final_tx_time) << 8;

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
