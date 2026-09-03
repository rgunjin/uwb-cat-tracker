#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "deca_device_api.h"
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

/* How many measurements to accumulate before printing a summary. */
#define STATS_WINDOW      50

void run_initiator(void)
{
    uint8_t buf[32];
    uint16_t len;
    uint8_t seq = 0;
    uint32_t ok = 0;
    uint32_t lost = 0;

    int32_t sum = 0;
    uint32_t n = 0;
    int32_t min = INT32_MAX;
    int32_t max = INT32_MIN;

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

        /* All three relate to the frame just received; read them together
         * before anything can re-enable the receiver. */
        uint32_t poll_tx_ts = dwt_readtxtimestamplo32();
        uint32_t resp_rx_ts = dwt_readrxtimestamplo32();
        int32_t integrator  = dwt_readcarrierintegrator();

        dwt_rxdiag_t diag;
        dwt_readdiagnostics(&diag);

        /* RX_POWER - FP_POWER reduces to a ratio: N^2 and the PRF
	     * constant cancel, so no logarithm is needed. Thresholds are
	     * 6 dB (ratio 3.98) for line of sight and 10 dB (ratio 10)
	     * for non-line-of-sight, UM 4.7. */
	    uint64_t total = (uint64_t)diag.maxGrowthCIR << 17;
	    uint64_t fp    = (uint64_t)diag.firstPathAmp1 * diag.firstPathAmp1
	                + (uint64_t)diag.firstPathAmp2 * diag.firstPathAmp2
	                + (uint64_t)diag.firstPathAmp3 * diag.firstPathAmp3;

	    uint32_t ratio_x100 = (fp > 0) ? (uint32_t)(total * 100 / fp) : 0;

        /* Sign convention: a positive ratio means the responder's clock runs
         * slower than ours (the multiplier flips the sign of the raw value). */
        double clock_offset = integrator *
            (FREQ_OFFSET_MULTIPLIER * HERTZ_TO_PPM_MULTIPLIER_CHAN_5 / 1.0e6);

        /* T_round: measured by the initiator, own clock. */
        uint32_t t_round = resp_rx_ts - poll_tx_ts;

        /* T_reply: measured by the responder, its clock, carried in the frame. */
        uint32_t t_reply = rx->resp_tx_ts - rx->poll_rx_ts;

        /* T_reply was measured by the responder's clock, so it is scaled
         * into ours before the subtraction. */
        double tof = (t_round - t_reply) / 2.0 * DWT_TIME_UNITS;
        int32_t d = (int32_t)(tof * SPEED_OF_LIGHT * 1000);

        sum += d;
        n++;

        if (d < min) min = d;
        if (d > max) max = d;

        if (n == STATS_WINDOW) {
            LOG_INF("n=%u  avg %d mm  spread %d  off %d ppb | "
			        "cir %u  pacc %u  noise %u  ratio %u | "
			        "f1 %u  f2 %u  f3 %u",
			        n, sum / (int32_t)n, max - min,
			        (int32_t)(clock_offset * 1e9),
			        diag.maxGrowthCIR, diag.rxPreamCount,
			        diag.stdNoise, ratio_x100,
			        diag.firstPathAmp1, diag.firstPathAmp2,
			        diag.firstPathAmp3);
            sum = 0;
            n = 0;
            min = INT32_MAX;
            max = INT32_MIN;
        }

        ok++;

next:
        seq++;
        k_msleep(POLL_INTERVAL_MS);
    }
}
