#ifndef UWB_MSG_H
#define UWB_MSG_H

#include <stdint.h>

#define MSG_POLL        0x01
#define MSG_RESPONSE    0x02

/*! Frame layout: type byte, then a sequence number the responder
 *  echoes back, so a reply can be matched to its poll.
 *
 *  __packed forbids the compiler from inserting padding between
 *  fields. Without it the in-memory layout can differ from the byte
 *  order on air: a struct with a uint8_t followed by a uint32_t
 *  would get three padding bytes to keep the 32-bit field aligned,
 *  and the receiver would read the wrong offsets. Harmless for two
 *  bytes, but the rule has to hold once timestamps go in. */
struct uwb_msg {
    uint8_t type;
    uint8_t seq;
} __packed;

#endif /* UWB_MSG_H */
