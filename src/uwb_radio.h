#ifndef UWB_RADIO_H
#define UWB_RADIO_H

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

/*! @brief Read the RX timestamp of the last received frame, all 40 bits.
 *
 *  The chip keeps timestamps as get_rx_timestamp_u6440-bit counts of device time units,
 *  one unit being 1 / (499.2 MHz * 128), about 15.65 ps.
 *
 *  dwt_readrxtimestamplo32() returns only the low 32 bits, which is
 *  enough when the value is used in a difference: the intervals in a
 *  ranging exchange are far shorter than the 67 ms it takes a 32-bit
 *  count to wrap, and unsigned subtraction handles a wrap correctly.
 *
 *  Scheduling a delayed transmission is different. There the timestamp
 *  is added to a delay and shifted right by 8, since DX_TIME takes the
 *  high 32 bits of the 40-bit time — so bits 8..39 are needed, and the
 *  top byte is missing from the low 32. The addition could also carry
 *  past bit 31, which 32-bit arithmetic would silently drop.
 *
 *  @return the timestamp, 40 significant bits
 */
uint64_t uwb_rx_timestamp(void);

/*!
 * @brief Transmit a frame at the time set by dwt_setdelayedtrxtime(),
 *        and wait for it to leave the radio.
 *
 * The transmit time must be programmed before calling this. The chip
 * works backwards from it to decide when to start the preamble, so
 * that the RMARKER lands on the scheduled moment (UM 3.3).
 *
 * @param data payload
 * @param len  payload length in bytes
 * @return 0 on success, -ETIME if the scheduled time had already
 *         passed, -EIO if TXFRS never came up
 */
int uwb_send_delayed(const uint8_t *data, uint16_t len);

#endif /* UWB_RADIO_H */
