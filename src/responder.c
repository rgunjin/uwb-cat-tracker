#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "uwb_radio.h"
#include "uwb_msg.h"
#include "responder.h"

LOG_MODULE_REGISTER(responder, LOG_LEVEL_INF);

void run_responder(void)
{
    uint8_t buf[32];
    uint16_t len;
    uint32_t count = 0;

    LOG_INF("responder started");

    while (1) {
        /* No timeout: the responder has nothing else to do. */
        if (uwb_receive(buf, sizeof(buf), &len, 0) != 0) {
            continue;
        }

        if (len < sizeof(struct uwb_msg)) {
            continue;
        }

        struct uwb_msg *rx = (struct uwb_msg *)buf;

        if (rx->type != MSG_POLL) {
            continue;
        }

        struct uwb_msg reply = {
            .type = MSG_RESPONSE,
            .seq = rx->seq,
        };

        if (uwb_send((uint8_t *)&reply, sizeof(reply)) == 0) {
            LOG_INF("replied to poll %u (%u total)", rx->seq, ++count);
        }
    }
}
