#ifndef UWB_MSG_H
#define UWB_MSG_H

#include <stdint.h>
#include <zephyr/sys/util.h>

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
#define UWB_ADDR_C      0x0002
#define UWB_ADDR_A1     0x0011
#define UWB_ADDR_A2     0x0012
#define UWB_ADDR_A3     0x0013

/*! This node's short address, read from NVS once at startup (see
 *  storage_get_addr()). Needed in main.c to program the chip's
 *  address filter, and in the role's own .c file to fill in and
 *  check frame headers. */
extern uint16_t uwb_my_addr;

/*! Function codes, carried after the MAC header. Values follow the
 *  Decawave examples so a frame dump reads the same way. */
#define MSG_POLL        0xE0
#define MSG_RESPONSE    0xE1
#define MSG_FINAL       0xE2
#define MSG_REPORT      0xE3

/*! Slot schedule, UM 12.3.4.3 (Figure 39) extended with anchor
 *  reports to the center node. All values in UWB microseconds,
 *  counted from the tag's poll_tx_ts. One cycle is 5.6 ms, polled
 *  every 500 ms; none of that timing is enforced here, this header
 *  only names the constants the schedule is built from. */
#define SLOT_UUS        600
#define SLOT_POLL       (0 * SLOT_UUS)
#define SLOT_RESP_A1    (1 * SLOT_UUS)
#define SLOT_RESP_A2    (2 * SLOT_UUS)
#define SLOT_RESP_A3    (3 * SLOT_UUS)
#define SLOT_FINAL      (5 * SLOT_UUS)
#define SLOT_RESERVED   (6 * SLOT_UUS)
#define SLOT_REPORT_A1  (7 * SLOT_UUS)
#define SLOT_REPORT_A2  (8 * SLOT_UUS)
#define SLOT_REPORT_A3  (9 * SLOT_UUS)

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

/*! Raw 40-bit device timestamp, byte order as dwt_readtxtimestamp()
 *  / dwt_readrxtimestamp() write it (least significant byte first).
 *  Kept as bytes, not a scalar type: the 32-bit counter wraps every
 *  ~67 ms and truncating to it would fail silently, so the only safe
 *  choice is to carry all 5 bytes untouched and let the Pi do the
 *  arithmetic. */
typedef uint8_t uwb_ts40_t[5];

/*! One anchor's slot in the Final frame: which anchor answered (0x0000
 *  if it missed its slot) and the rx timestamp of its Response. */
struct uwb_final_anchor {
	uint16_t    addr;
	uwb_ts40_t  resp_rx_ts;
} __packed;

/*! Final: broadcast by the tag once all three response slots have
 *  passed. Three anchor entries are always present, regardless of how
 *  many anchors actually answered — the window is fixed by the
 *  schedule, not by how many Responses came in. */
struct uwb_final_msg {
	struct uwb_msg           msg;
	uwb_ts40_t                poll_tx_ts;
	struct uwb_final_anchor   anchors[3];
	uwb_ts40_t                final_tx_ts;
} __packed;

BUILD_ASSERT(sizeof(struct uwb_final_msg) == 41,
	     "uwb_final_msg must match the 41-byte Final layout");

/*! Report: one anchor's view of a cycle, sent to the center after its
 *  Report slot. tag_seq echoes the Poll's sequence number so the
 *  center can match this Report back to the tag's Final. */
struct uwb_report_msg {
	struct uwb_msg  msg;
	uint8_t         tag_seq;
	uwb_ts40_t      poll_rx_ts;
	uwb_ts40_t      resp_tx_ts;
	uwb_ts40_t      final_rx_ts;
	uint32_t        ratio_x100;
} __packed;

BUILD_ASSERT(sizeof(struct uwb_report_msg) == 30,
	     "uwb_report_msg must match the 30-byte Report layout");

#endif /* UWB_MSG_H */
