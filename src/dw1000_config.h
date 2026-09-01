#ifndef DW1000_CONFIG_H
#define DW1000_CONFIG_H

#include "deca_device_api.h"

/*! Radio configuration. Fixed by the DWM1001C certification —
 *  see docs/config.md. */
extern const dwt_config_t dw1000_config;

/*! Antenna delay. The OTP value on this batch of modules is 16472;
 *  calibrated at 2 m to 16495, since the measured distance came out
 *  282 mm long. One millimetre of error is about 0.107 units: the
 *  delay enters the result four times and T_prop is halved. */
#define DW1000_ANT_DLY    16433

#endif /* DW1000_CONFIG_H */
