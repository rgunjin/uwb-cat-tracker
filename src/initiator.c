#include <stdbool.h>
#include <stdint.h>
#include <sys/errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

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

        bool answered[3] = { false, false, false };
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

            answered[idx] = true;
            n_answered++;
            LOG_INF("poll %u: response from 0x%04X (A%d)",
                    seq, rx->hdr.src, idx + 1);
        }

        for (int i = 0; i < 3; i++) {
            if (!answered[i]) {
                LOG_INF("poll %u: no response from A%d", seq, i + 1);
            }
        }

        seq++;
        k_msleep(POLL_INTERVAL_MS);
    }
}
