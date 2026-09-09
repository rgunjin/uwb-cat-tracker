#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <zephyr/shell/shell.h>

#include "storage.h"

static int parse_u16(const struct shell *sh, const char *arg, uint16_t *out)
{
	char *end;
	unsigned long value = strtoul(arg, &end, 0);

	if (*end != '\0' || value > UINT16_MAX) {
		shell_error(sh, "invalid value: %s", arg);
		return -EINVAL;
	}

	*out = (uint16_t)value;

	return 0;
}

static int cmd_addr(const struct shell *sh, size_t argc, char **argv)
{
	uint16_t value;
	int rc;

	if (argc == 1) {
		shell_print(sh, "address: 0x%04X", storage_get_addr());
		return 0;
	}

	rc = parse_u16(sh, argv[1], &value);
	if (rc != 0) {
		return rc;
	}

	rc = storage_set_addr(value);
	if (rc != 0) {
		shell_error(sh, "storage_set_addr failed: %d", rc);
		return rc;
	}

	shell_print(sh, "address stored: 0x%04X", value);
	shell_print(sh, "takes effect after reboot (address is read once at startup)");

	return 0;
}

static int cmd_antdly(const struct shell *sh, size_t argc, char **argv)
{
	uint16_t value;
	int rc;

	if (argc == 1) {
		shell_print(sh, "antenna delay: %u", storage_get_ant_dly());
		return 0;
	}

	rc = parse_u16(sh, argv[1], &value);
	if (rc != 0) {
		return rc;
	}

	rc = storage_set_ant_dly(value);
	if (rc != 0) {
		shell_error(sh, "storage_set_ant_dly failed: %d", rc);
		return rc;
	}

	shell_print(sh, "antenna delay stored: %u", value);

	return 0;
}

SHELL_CMD_ARG_REGISTER(addr, NULL,
	"Get/set the node's short address (decimal or 0x hex)",
	cmd_addr, 1, 1);

SHELL_CMD_ARG_REGISTER(antdly, NULL,
	"Get/set the antenna delay (decimal or 0x hex)",
	cmd_antdly, 1, 1);
