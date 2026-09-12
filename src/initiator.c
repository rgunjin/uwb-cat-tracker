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
            .hdr = {
                .fc = { UWB_FC0, UWB_FC1 },
                .seq = seq,
                .pan = UWB_PAN,
                .dst = UWB_ADDR_A1,
                .src = UWB_ADDR_T1,
            },
            .type = MSG_POLL,
        };

        if (uwb_send((uint8_t *)&poll, sizeof(poll)) != 0) {
            LOG_WRN("poll %u not sent", seq);
            seq++;
            k_msleep(POLL_INTERVAL_MS);
            continue;
        }

        int err = uwb_receive(buf, sizeof(buf), &len, REPLY_TIMEOUT_MS);

        if (err != 0) {
            lost++;
            goto next;
        }

        if (len < sizeof(struct uwb_msg)) {
            lost++;
            goto next;
        }

        struct uwb_msg *rx = (struct uwb_msg *)buf;

        if (rx->type != MSG_RESPONSE ||
            rx->hdr.seq != seq ||
            rx->hdr.dst != UWB_ADDR_T1) {
            LOG_WRN("unexpected reply: type %u seq %u, expected %u",
                    rx->type, rx->hdr.seq, seq);
            lost++;
            goto next;
        }

        ok++;
        LOG_INF("response %u ok (%u ok, %u lost)", seq, ok, lost);

next:
        seq++;
        k_msleep(POLL_INTERVAL_MS);
    }
}
