#ifndef UWB_MSG_H
#define UWB_MSG_H

#include <stdint.h>

/*! IEEE 802.15.4 frame control, low byte then high byte.
 *
 *  0x41 = data frame (type 001) with PAN ID compression set, so the
 *  PAN appears once instead of twice.
 *  0x88 = both destination and source addressing modes are 10,
 *  short 16-bit addresses.
 *
 *  This is the same value the Decawave examples use, and it is what
 *  makes the header nine bytes: control, sequence, PAN, dst, src.
 */
#define UWB_FC0         0x41
#define UWB_FC1         0x88

#define UWB_PAN         0xCA7

/*! Short addresses. 0xFFFF is the broadcast address defined by the
 *  standard; the poll goes to every anchor at once. */
#define UWB_ADDR_BCAST  0xFFFF
#define UWB_ADDR_T1     0x0001
#define UWB_ADDR_A1     0x0011
#define UWB_ADDR_A2     0x0012
#define UWB_ADDR_A3     0x0013

/*! Function codes, carried after the MAC header. Values follow the
 *  Decawave examples so a frame dump reads the same way. */
#define MSG_POLL        0xE0
#define MSG_RESPONSE    0xE1

/*! IEEE 802.15.4 MAC header, nine bytes on air.
 *
 *  __packed forbids padding between fields. Without it the compiler
 *  would align the 16-bit fields and the layout in memory would stop
 *  matching the bytes on air — the frame filter in the chip reads
 *  fixed offsets and would see garbage.
 */
struct ieee_hdr {
	uint8_t  fc[2];
	uint8_t  seq;
	uint16_t pan;
	uint16_t dst;
	uint16_t src;
} __packed;

/*! Poll: header plus the function code. */
struct uwb_msg {
	struct ieee_hdr hdr;
	uint8_t type;
} __packed;

/*! Response: the poll frame plus the two timestamps the initiator
 *  needs. Raw device time units — the subtraction happens on the
 *  initiator. */
struct uwb_resp_msg {
	struct uwb_msg msg;
	uint32_t poll_rx_ts;
	uint32_t resp_tx_ts;
} __packed;

#endif /* UWB_MSG_H */
