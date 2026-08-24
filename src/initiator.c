#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <deca_device_api.h>

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

/* in air, not vacuum */
#define SPEED_OF_LIGHT  299702547    

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

        if (err != 0) {
            lost++;
            goto next;
        }

        if (len < sizeof(struct uwb_resp_msg)) {
            lost++;
            goto next;
        }


        struct uwb_resp_msg *rx = (struct uwb_resp_msg *)buf;

        if (rx->hdr.type != MSG_RESPONSE || rx->hdr.seq != seq) {
            LOG_WRN("unexpected reply: type %u seq %u, expected %u",
                    rx->hdr.type, rx->hdr.seq, seq);
            lost++;
            goto next;
        }

        uint32_t poll_tx_ts = dwt_readtxtimestamplo32();
        uint32_t resp_rx_ts = dwt_readrxtimestamplo32();

        /* T_round: measured by the initiator, own clock. */
        uint32_t t_round = resp_rx_ts - poll_tx_ts;

        /* T_reply: measured by the responder, its clock, carried in the frame. */
        uint32_t t_reply = rx->resp_tx_ts - rx->poll_rx_ts;

        double tof = (t_round - t_reply) / 2.0 * DWT_TIME_UNITS;
        double distance = tof * SPEED_OF_LIGHT;

        LOG_INF("seq %u: T_round %u  T_reply %u  dist %d mm",
                seq, t_round, t_reply, (int)(distance * 1000));

        ok++;

next:
        if ((seq % 20) == 0) {
            LOG_INF("exchanges: %u ok, %u lost", ok, lost);
        }
        seq++;
        k_msleep(POLL_INTERVAL_MS);
    }
}
