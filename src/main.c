#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "dw1000_spi.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void) {
    if (dw1000_spi_init() != 0) {
		return -ENODEV;
	}

	uint8_t id[4];
	dw1000_read(0x00, 0, id, 4);
    LOG_INF("DEV_ID: %02x %02x %02x %02x", id[3], id[2], id[1], id[0]);

    return 0;
}
