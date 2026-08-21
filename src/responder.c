#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <deca_device_api.h>

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
            uint32_t poll_rx_ts = dwt_readrxtimestamplo32();
            uint32_t resp_tx_ts = dwt_readtxtimestamplo32();

            LOG_INF("poll %u T_reply %u", rx->seq, resp_tx_ts - poll_rx_ts);
            count++;
        }
    }
}
