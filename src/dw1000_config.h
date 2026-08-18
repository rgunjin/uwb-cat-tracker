#ifndef DW1000_CONFIG_H
#define DW1000_CONFIG_H

#include "deca_device_api.h"

/*! Radio configuration. Fixed by the DWM1001C certification —
 *  see docs/config.md. */
extern const dwt_config_t dw1000_config;

/*! Antenna delay, read from OTP on this batch of modules. */
#define DW1000_ANT_DELAY    16472

#endif /* DW1000_CONFIG_H */
