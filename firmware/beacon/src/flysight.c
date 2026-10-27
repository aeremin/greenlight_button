/*
 * FlySight 2 through its Device State service, which needs firmware v2026.05.13.develop or
 * later. A paired FlySight advertises slowly while it sleeps and accepts mode requests then.
 */

#include <errno.h>

#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#include "action.h"
#include "link.h"

LOG_MODULE_REGISTER(flysight, LOG_LEVEL_INF);

#define FLYSIGHT_UUID(n) BT_UUID_128_ENCODE(n, 0x8e22, 0x4541, 0x9d4c, 0x21edae82ed19)

static const struct bt_uuid_128 mode_uuid = BT_UUID_INIT_128(FLYSIGHT_UUID(0x00000005));
static const struct bt_uuid_128 control_uuid = BT_UUID_INIT_128(FLYSIGHT_UUID(0x00000007));

/* Manufacturer data: company, then 0x01 while in pairing request mode */
#define COMPANY_ID 0x09db

#define MODE_SLEEP  0x00
#define MODE_ACTIVE 0x01
#define MODE_START  0x05

#define CMD_REQUEST_SLEEP  0x10
#define CMD_REQUEST_ACTIVE 0x11

#define SETTLE_TRIES 15
#define SETTLE_MS    200

static int check_pairing(uint8_t adv_type, struct net_buf_simple *ad)
{
	uint8_t status;

	return (link_mfg_data(ad, COMPANY_ID, &status, 1) >= 1 && status == 0x01) ? 0 : 1;
}

static int read_mode(uint16_t handle)
{
	uint8_t mode;
	int n = link_read(PEER_FLYSIGHT, handle, &mode, 1);

	if (n < 0) {
		return n;
	}
	return n == 1 ? mode : -EIO;
}

static int set_mode(enum action action)
{
	uint16_t mode_handle, control_handle;
	uint8_t target, cmd;
	int mode, err;

	err = link_find(PEER_FLYSIGHT, &mode_uuid.uuid, &mode_handle);
	if (!err) {
		err = link_find(PEER_FLYSIGHT, &control_uuid.uuid, &control_handle);
	}
	if (err) {
		LOG_ERR("Device State service not found (%d); update the FlySight firmware", err);
		return err;
	}

	mode = read_mode(mode_handle);
	if (mode < 0) {
		return mode;
	}

	if (action == ACTION_START) {
		/* Start mode logs too, for the starter pistol */
		if (mode == MODE_ACTIVE || mode == MODE_START) {
			LOG_INF("Logging already");
			return 0;
		}
		/* Only sleep may move to active, so don't override config, pairing or USB */
		if (mode != MODE_SLEEP) {
			LOG_WRN("Busy in mode %d", mode);
			return -EBUSY;
		}
		target = MODE_ACTIVE;
		cmd = CMD_REQUEST_ACTIVE;
	} else {
		if (mode == MODE_SLEEP) {
			LOG_INF("Asleep already");
			return 0;
		}
		target = MODE_SLEEP;
		cmd = CMD_REQUEST_SLEEP;
	}

	/* Without indications enabled the FlySight sends no response, so poll the mode instead */
	err = link_write(PEER_FLYSIGHT, control_handle, &cmd, sizeof(cmd));
	if (err) {
		return err;
	}

	for (int i = 0; i < SETTLE_TRIES; i++) {
		err = link_sleep(SETTLE_MS);
		if (err) {
			return err;
		}
		mode = read_mode(mode_handle);
		if (mode == target) {
			LOG_INF("%s", target == MODE_ACTIVE ? "Logging" : "Asleep");
			return 0;
		}
		if (mode < 0) {
			return mode;
		}
	}

	LOG_WRN("Mode stuck at %d", mode);
	return -ETIMEDOUT;
}

int flysight_run(enum action action)
{
	bool pair = action == ACTION_PAIR;
	int err;

	err = link_connect(PEER_FLYSIGHT, pair, pair ? check_pairing : NULL,
			   pair ? PAIRING_TIMEOUT : CONNECT_TIMEOUT);
	if (err) {
		return err;
	}

	if (pair) {
		LOG_INF("Paired");
	} else {
		err = set_mode(action);
	}

	link_disconnect(PEER_FLYSIGHT);
	return err;
}
