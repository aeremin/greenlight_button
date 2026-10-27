/* Debug commands over the RTT shell, built only with debug.conf. */

#include <stdlib.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>

#include "action.h"
#include "link.h"

static const char *const peer_names[] = {
	[PEER_GOPRO] = "gopro",
	[PEER_FLYSIGHT] = "flysight",
};

static int cmd_press(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t held_ms = argc > 1 ? strtoul(argv[1], NULL, 10) : 100;

	button_press(held_ms);
	return 0;
}

static int cmd_long(const struct shell *sh, size_t argc, char **argv)
{
	button_press(2000);
	return 0;
}

static int cmd_pair(const struct shell *sh, size_t argc, char **argv)
{
	button_press(10000);
	return 0;
}

static int cmd_status(const struct shell *sh, size_t argc, char **argv)
{
	for (int i = 0; i < PEER_COUNT; i++) {
		shell_print(sh, "%s: %s", peer_names[i],
			    link_is_paired(i) ? "paired" : "not paired");
	}
	return 0;
}

static int cmd_unpair(const struct shell *sh, size_t argc, char **argv)
{
	for (int i = 0; i < PEER_COUNT; i++) {
		if (!strcmp(argv[1], "all") || !strcmp(argv[1], peer_names[i])) {
			link_unpair(i);
			shell_print(sh, "%s: unpaired", peer_names[i]);
		}
	}
	return 0;
}

#define SCAN_MAX 24

static bt_addr_le_t seen[SCAN_MAX];
static size_t seen_count;
static const struct shell *scan_shell;

struct ad_summary {
	char name[20];
	int company;
	bool gopro_uuid;
};

static bool summarize(struct bt_data *data, void *user_data)
{
	struct ad_summary *sum = user_data;

	switch (data->type) {
	case BT_DATA_NAME_COMPLETE:
	case BT_DATA_NAME_SHORTENED:
		memcpy(sum->name, data->data, MIN(data->data_len, sizeof(sum->name) - 1));
		break;
	case BT_DATA_MANUFACTURER_DATA:
		if (data->data_len >= 2) {
			sum->company = sys_get_le16(data->data);
		}
		break;
	case BT_DATA_UUID16_SOME:
	case BT_DATA_UUID16_ALL:
		for (size_t i = 0; i + 1 < data->data_len; i += 2) {
			sum->gopro_uuid |= sys_get_le16(&data->data[i]) == 0xfea6;
		}
		break;
	}
	return true;
}

static void scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{
	struct ad_summary sum = {.company = -1};
	char addr[BT_ADDR_LE_STR_LEN];

	if (!scan_shell) {
		return;
	}
	for (size_t i = 0; i < seen_count; i++) {
		if (bt_addr_le_eq(&seen[i], info->addr)) {
			return;
		}
	}
	if (seen_count < SCAN_MAX) {
		bt_addr_le_copy(&seen[seen_count++], info->addr);
	}

	bt_data_parse(buf, summarize, &sum);
	bt_addr_le_to_str(info->addr, addr, sizeof(addr));
	shell_print(scan_shell, "%s rssi %d type %u props 0x%02x company %04x%s %s", addr,
		    info->rssi, info->adv_type, info->adv_props, sum.company & 0xffff,
		    sum.gopro_uuid ? " FEA6" : "", sum.name);
}

static struct bt_le_scan_cb scan_callbacks = {
	.recv = scan_recv,
};

static int cmd_scan(const struct shell *sh, size_t argc, char **argv)
{
	static bool registered;
	uint32_t seconds = argc > 1 ? strtoul(argv[1], NULL, 10) : 10;
	int err;

	if (!registered) {
		bt_le_scan_cb_register(&scan_callbacks);
		registered = true;
	}

	seen_count = 0;
	scan_shell = sh;
	err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, NULL);
	if (err) {
		shell_error(sh, "Scan failed to start (%d)", err);
		scan_shell = NULL;
		return err;
	}

	k_sleep(K_SECONDS(seconds));
	bt_le_scan_stop();
	scan_shell = NULL;
	shell_print(sh, "%u devices", seen_count);
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(gl_cmds,
	SHELL_CMD_ARG(scan, NULL, "[seconds] List advertisers; only while idle", cmd_scan, 1, 1),
	SHELL_CMD_ARG(press, NULL, "Short press, or [held_ms] for any length", cmd_press, 1, 1),
	SHELL_CMD(long, NULL, "Long press: stop", cmd_long),
	SHELL_CMD(pair, NULL, "10 s hold: pairing mode", cmd_pair),
	SHELL_CMD(status, NULL, "Show which devices are paired", cmd_status),
	SHELL_CMD_ARG(unpair, NULL, "<gopro|flysight|all>", cmd_unpair, 2, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(gl, &gl_cmds, "Greenlight button", NULL);
