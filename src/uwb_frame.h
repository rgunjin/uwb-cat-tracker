#ifndef UWB_FRAME_H
#define UWB_FRAME_H

#include <stdint.h>

/*!
 * @brief Transmit a frame and wait for it to leave the radio.
 *
 * The two CRC bytes the chip appends are accounted for here; the
 * caller passes the payload length only.
 *
 * @param data payload
 * @param len  payload length in bytes
 * @return 0 on success, -EIO if TXFRS never came up
 */
int uwb_send(const uint8_t *data, uint16_t len);

/*!
 * @brief Enable the receiver and wait for one frame.
 *
 * @param buf        where to put the payload
 * @param buf_size   size of buf in bytes
 * @param len        receives the payload length, CRC excluded
 * @param timeout_ms how long to wait, 0 for no limit
 * @return 0 on success, -ETIMEDOUT on timeout, -EIO on a receive error
 */
int uwb_receive(uint8_t *buf, uint16_t buf_size, uint16_t *len,
		uint32_t timeout_ms);

#endif /* UWB_FRAME_H */
