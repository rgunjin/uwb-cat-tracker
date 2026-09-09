#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>

/*! @brief Mount the NVS partition. Call once at startup.
 *  @return 0 on success, negative errno otherwise */
int storage_init(void);

/*! @brief Antenna delay for this module, in device time units.
 *  Falls back to the OTP default if nothing is stored. */
uint16_t storage_get_ant_dly(void);

int storage_set_ant_dly(uint16_t value);

/*! Short address for this node. Falls back to the per-role default
 *  (UWB_ADDR_T1 / UWB_ADDR_A1) if nothing is stored. */
uint16_t storage_get_addr(void);

int storage_set_addr(uint16_t value);

#endif /* STORAGE_H */
