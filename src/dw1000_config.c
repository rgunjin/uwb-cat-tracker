#include "dw1000_config.h"

const dwt_config_t dw1000_config = {
        .chan           = 5,
        .prf            = DWT_PRF_64M,
        .txPreambLength = DWT_PLEN_128,
        .rxPAC          = DWT_PAC8,
        .txCode         = 9,
        .rxCode         = 9,
        .nsSFD          = 0,
        .dataRate       = DWT_BR_6M8,
        .phrMode        = DWT_PHRMODE_STD,
        .sfdTO          = 128 + 8 + 1 - 8,
};
