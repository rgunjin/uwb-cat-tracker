#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "deca_device_api.h"
#include "deca_port.h"
#include "dw1000_config.h"
#include "initiator.h"
#include "responder.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static void dump_device_info(void) {
    uint32 otp[5];
    uint8 xtalt;

    dwt_otpread(0x006, &otp[0], 1);   /* PART_ID   */
	dwt_otpread(0x007, &otp[1], 1);   /* LOT_ID    */
	dwt_otpread(0x019, &otp[2], 1);   /* CH5 TX power, PRF 64 */
	dwt_otpread(0x01C, &otp[3], 1);   /* antenna delay */
	dwt_otpread(0x01E, &otp[4], 1);   /* XTAL_TRIM */

	dwt_readfromdevice(0x2B, 0x0E, 1, &xtalt);

	LOG_INF("PART_ID    0x%08X", otp[0]);
	LOG_INF("LOT_ID     0x%08X", otp[1]);
	LOG_INF("XTAL_TRIM  %u  (FS_XTALT 0x%02X)", otp[4] & 0x1F, xtalt);
	LOG_INF("ANT_DELAY  %u", otp[3] >> 16);
	LOG_INF("TX_POWER   0x%08X", otp[2]);
}

static int dw1000_setup(void) {
	if (deca_port_init() != 0) {
		return -ENODEV;
	}

	reset_DW1000();
	port_set_dw1000_slowrate();

	if (dwt_initialise(DWT_LOADUCODE) == DWT_ERROR) {
		LOG_ERR("dwt_initialise failed");
		return -EIO;
	}

    /* OTP must be read at the slow SPI rate. At 8 MHz the reads
	 * return 0xFF and leave the OTP interface hung, after which
	 * even a plain DEV_ID read fails. */
#if defined (CONFIG_UWB_DUMP_INFO)
    dump_device_info();
#endif

	port_set_dw1000_fastrate();
    
    dwt_configure((dwt_config_t *)&dw1000_config);
    dwt_settxantennadelay(DW1000_ANT_DELAY);
    dwt_setrxantennadelay(DW1000_ANT_DELAY);

    return  0;
}


int main(void) {
    if (dw1000_setup() != 0) {
        return  -EIO;
    }

#if defined (CONFIG_UWB_ROLE_INITIATOR)
    run_initiator();
#elif defined (CONFIG_UWB_ROLE_RESPONDER)
    run_responder();
#else
#error "No role selected"
#endif

    return 0;
}
