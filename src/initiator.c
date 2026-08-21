#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "uwb_radio.h"
#include "uwb_msg.h"
#include "initiator.h"

LOG_MODULE_REGISTER(initiator, LOG_LEVEL_INF);

/* How long to wait for a reply. Generous for now — the responder
 * needs to notice the poll, build a reply and get it on air. Worth
 * tightening once the exchange is reliable. */
#define REPLY_TIMEOUT_MS    100

/* Gap between exchanges */
#define POLL_INTERVAL_MS    500

void run_initiator(void)
{
    uint8_t buf[32];
    uint16_t len;
    uint8_t seq = 0;

    uint32_t ok = 0;
    uint32_t lost = 0;

    LOG_INF("initiator started");

    while (1) {
        struct uwb_msg poll = {
            .type = MSG_POLL,
            .seq = seq,
        };

        if (uwb_send((uint8_t *)&poll, sizeof(poll)) != 0) {
            LOG_WRN("poll %u not sent", seq);
            seq++;
            k_msleep(POLL_INTERVAL_MS);
            continue;
        }

        int err = uwb_receive(buf, sizeof(buf), &len, REPLY_TIMEOUT_MS);

        if (err == 0 && len >= sizeof(struct uwb_msg)) {
            struct uwb_msg *rx = (struct uwb_msg *)buf;

            if (rx->type == MSG_RESPONSE && rx->seq == seq) {
                ok++;
            } else {
                /* Wrong type, or a reply to an earlier poll
				 * that arrived late. */
                lost++;
                LOG_WRN("unexpected reply: type %u seq %u, "
                        "expected %u", rx->type,rx->seq, seq);
            }
        } else {
            lost++;
        }

        if ((seq % 20) == 0) {
            LOG_INF("exchanges: %u ok, %u lost", ok, lost);
        }

        seq++;
        k_msleep(POLL_INTERVAL_MS);
    }
}
