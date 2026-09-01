#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/kvss/nvs.h>
#include <zephyr/logging/log.h>

#include "storage.h"
#include "dw1000_config.h"

LOG_MODULE_REGISTER(storage, LOG_LEVEL_INF);

/* The partition comes from the board devicetree: 24 KB at 0x7A000. */
#define NVS_PARTITION         storage_partition
#define NVS_PARTITION_DEVICE  PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET  PARTITION_OFFSET(NVS_PARTITION)

/* Keys. Never reuse a number for a different meaning — old records
 * with that id may still be on the flash. */
#define ID_ANT_DLY  1

static struct nvs_fs fs;

int storage_init(void)
{
    struct flash_pages_info info;
    int rc;

    fs.flash_device = NVS_PARTITION_DEVICE;

    if (!device_is_ready(fs.flash_device)) {
        LOG_ERR("flash device not ready");
        return -ENODEV;
    }

    fs.offset = NVS_PARTITION_OFFSET;

    rc = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
    if (rc) {
        LOG_ERR("cannot read flash page info: %d", rc);
        return rc;
    }

    fs.sector_size = info.size;
    fs.sector_count = 3U;

    rc = nvs_mount(&fs);
    if (rc) {
        LOG_ERR("nvs_mount failed: %d", rc);
        return  rc;
    }

    LOG_INF("NVS mounted, %u sectors of %u bytes", fs.sector_count, fs.sector_size);

    return 0;
}

uint16_t storage_get_ant_dly(void)
{
    uint16_t value;
    int rc;

    rc = nvs_read(&fs, ID_ANT_DLY, &value, sizeof(value));

    /* nvs_read returns the number of bytes read, or a negative
	 * errno; -ENOENT means the key has never been written. */
    if (rc == sizeof(value)) {
        return  value;
    }

    LOG_WRN("no stored antenna delay, using default %u", DW1000_ANT_DLY);

    return DW1000_ANT_DLY;
}

int storage_set_ant_dly(uint16_t value)
{
    int rc = nvs_write(&fs, ID_ANT_DLY, &value, sizeof(value));

    /* nvs_write returns the number of bytes written, 0 if the value
	 * was already there, or a negative errno. */
    if (rc < 0) {
        LOG_ERR("cannot stored antenna delay: %d", rc);
        return rc;
    }

    LOG_INF("antenna delay stored: %u", value);

    return 0;
}
