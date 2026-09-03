#include <stdint.h>
#include <sys/errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "deca_device_api.h"
#include "deca_port.h"
#include "dw1000_config.h"
#include "initiator.h"
#include "responder.h"
#include "deca_regs.h"
#include "storage.h"

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
	LOG_INF("ANT_DELAY  %u (OTP)", otp[3] >> 16);
	LOG_INF("TX_POWER   0x%08X (OTP)", otp[2]);
	LOG_INF("TX_POWER   0x%08X (register)", dwt_read32bitreg(TX_POWER_ID));
}

static int dw1000_setup(void)
{
    uint16_t ant_dly;

    if (storage_init() != 0) {
        return  -EIO;
    }

    storage_set_ant_dly(16430);   /* temporary: verifying the model */

    ant_dly = storage_get_ant_dly();

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
	 * return 0xFF and leave the OTP interface hung. */
	uint32 tx_power;
	dwt_otpread(0x019, &tx_power, 1);   /* channel 5, PRF 64 */

#if defined(CONFIG_UWB_DUMP_INFO)
    dump_device_info();
#endif

	port_set_dw1000_fastrate();
    
    dwt_configure((dwt_config_t *)&dw1000_config);

    /* dwt_configure() does not touch TX_POWER; without this the
	 * chip keeps its reset default instead of the factory
	 * calibration held in OTP. */
	dwt_txconfig_t tx_cfg = {
		.PGdly = TC_PGDELAY_CH5,
		.power = tx_power,
	};
	dwt_configuretxrf(&tx_cfg);

    dwt_settxantennadelay(ant_dly);
    dwt_setrxantennadelay(ant_dly);

    LOG_INF("DW1000 ready, DEV_ID 0x%08X, ch%u, ant delay %u, tx power 0x%08X",
	    dwt_readdevid(), dw1000_config.chan, ant_dly,
	    dwt_read32bitreg(TX_POWER_ID));

    /* Let the log thread drain before the role loop takes over —
     * the startup output should not depend on when the first frame
     * arrives. */
	k_msleep(10);

    return  0;
}


int main(void) {
    if (dw1000_setup() != 0) {
        return  -EIO;
    }

#if defined(CONFIG_UWB_ROLE_INITIATOR)
    run_initiator();
#elif defined(CONFIG_UWB_ROLE_RESPONDER)
    run_responder();
#else
#error "No role selected"
#endif

    return 0;
}
