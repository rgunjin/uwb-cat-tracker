#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <deca_device_api.h>

#include "uwb_radio.h"
#include "uwb_msg.h"
#include "responder.h"

/*! One UWB microsecond (UUS) in device time units.
 *
 *  A UUS is 512 / 499.2 MHz, roughly 1.0256 us — the same unit
 *  dwt_setrxtimeout() takes. A device time unit (DTU) is
 *  1 / (499.2 MHz * 128), roughly 15.65 ps. One UUS therefore holds
 *  65536 DTU. Both derive from the 499.2 MHz base frequency of the
 *  IEEE 802.15.4 UWB standard. */
#define UUS_TO_DWT_TIME             65536

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
    uint8_t buf[32];
    uint16_t len;
    uint32_t count = 0;
    uint32_t late = 0;
    uint32_t bad = 0;
    uint32_t resp_dly_uus;

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
        /* Адрес не входит в {A1, A2, A3} — какой слот занимать в
         * этом случае, решением человека не закрыто (см. отчёт по
         * T011). Здесь только безопасная остановка, не решение: не
         * подставляем случайный слот и не отвечаем вслепую. */
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

        if (rx->type != MSG_POLL) {
            bad++;
            goto next;
        }

        if (rx->hdr.dst != uwb_my_addr &&
            rx->hdr.dst != UWB_ADDR_BCAST) {
            bad++;
            goto next;
        }

        /* Full 40 bits: the addition below would overflow in 32,
		 * and DX_TIME needs bits 8..39 after the shift. */
        uint64_t poll_rx_ts = uwb_rx_timestamp();

        uint32_t tx_time = (poll_rx_ts + (resp_dly_uus * UUS_TO_DWT_TIME)) >> 8;

        dwt_setdelayedtrxtime(tx_time);

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

        count++;

next:
        if (((count + bad + late) % 20) == 0 && count > 0) {
            LOG_INF("replies: %u, late %u, bad %u",
                    count, late, bad);
        }

        /* Give the log thread a slot. Safe here: the reply is
         * already on air, so nothing time-critical is pending. */
        k_msleep(1);
    }
}
