/*
 * GoPro over the Open GoPro BLE API. A paired camera keeps advertising for 8 hours after it
 * goes to sleep, and connecting to it in that time wakes it up.
 */

#include <errno.h>

#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#include "action.h"
#include "link.h"

LOG_MODULE_REGISTER(gopro, LOG_LEVEL_INF);

#define GOPRO_UUID(n) BT_UUID_128_ENCODE(0xb5f90000 | (n), 0xaa8d, 0x11e3, 0x9046, 0x0002a5d5c51b)

static const struct bt_uuid_128 command_uuid = BT_UUID_INIT_128(GOPRO_UUID(0x0072));
static const struct bt_uuid_128 response_uuid = BT_UUID_INIT_128(GOPRO_UUID(0x0073));
static const struct bt_uuid_128 network_uuid = BT_UUID_INIT_128(GOPRO_UUID(0x0091));

/* Manufacturer data: company, schema version, camera status */
#define COMPANY_ID     0xf202
#define STATUS_AWAKE   BIT(0)
#define STATUS_PAIRING BIT(2)

/* Commands carry a one-byte length header, the command ID, then length-value parameters */
static const uint8_t cmd_hw_info[] = {0x01, 0x3c};
static const uint8_t cmd_shutter_on[] = {0x03, 0x01, 0x01, 0x01};
static const uint8_t cmd_shutter_off[] = {0x03, 0x01, 0x01, 0x00};
static const uint8_t cmd_sleep[] = {0x01, 0x05};

/*
 * Network management feature 0x03, action 0x01 (set pairing state) with a RequestPairingFinish
 * protobuf: result SUCCESS and a client name. It clears the pairing screen on the camera.
 */
static const uint8_t cmd_pairing_finish[] = {
	0x10, 0x03, 0x01, 0x08, 0x00, 0x12, 0x0a,
	'G', 'r', 'e', 'e', 'n', 'l', 'i', 'g', 'h', 't',
};

#define RESPONSE_TIMEOUT K_SECONDS(2)
/* Covers booting from sleep and closing a video file */
#define COMMAND_TIMEOUT  K_SECONDS(20)
#define RETRY_MS         500

static uint16_t command_handle;

static int camera_status(struct net_buf_simple *ad)
{
	uint8_t data[2];

	if (link_mfg_data(ad, COMPANY_ID, data, sizeof(data)) < (int)sizeof(data)) {
		return -ENOENT;
	}
	return data[1];
}

static int check_pairing(uint8_t adv_type, struct net_buf_simple *ad)
{
	int status = camera_status(ad);

	return (status >= 0 && (status & STATUS_PAIRING)) ? 0 : 1;
}

static int check_awake(uint8_t adv_type, struct net_buf_simple *ad)
{
	int status = camera_status(ad);

	/* Connecting would only wake it up to put it back to sleep */
	return (status >= 0 && !(status & STATUS_AWAKE)) ? -EALREADY : 0;
}

/* Returns the command status from the response, or a negative error. */
static int command(const uint8_t *req, uint16_t len)
{
	k_timepoint_t end = sys_timepoint_calc(RESPONSE_TIMEOUT);
	uint8_t rsp[20];
	int err;

	link_flush(PEER_GOPRO);
	err = link_write(PEER_GOPRO, command_handle, req, len);
	if (err) {
		return err;
	}

	for (;;) {
		int n = link_recv(PEER_GOPRO, rsp, sizeof(rsp), sys_timepoint_timeout(end));
		int at;

		if (n < 0) {
			return n;
		}

		/* Bit 7 marks a continuation packet. Bits 6-5 pick a 5, 13 or 16-bit length. */
		if (rsp[0] & BIT(7)) {
			continue;
		}
		at = 1 + (rsp[0] >> 5);
		if (at > 3 || n < at + 2 || rsp[at] != req[1]) {
			continue;
		}
		return rsp[at + 1];
	}
}

/* The camera turns commands down while it boots or is busy, so keep asking. */
static int command_retry(const uint8_t *req, uint16_t len, const char *what)
{
	k_timepoint_t end = sys_timepoint_calc(COMMAND_TIMEOUT);
	int status;

	for (;;) {
		status = command(req, len);
		if (status == 0 || status == -ENOTCONN) {
			break;
		}
		if (sys_timepoint_expired(end) || link_sleep(RETRY_MS) == -ECANCELED) {
			break;
		}
	}

	if (status) {
		LOG_WRN("%s failed (%d)", what, status);
	}
	return status > 0 ? -EIO : status;
}

int gopro_run(enum action action)
{
	link_adv_check_t check = NULL;
	uint16_t response_handle, network_handle;
	int err;

	if (action == ACTION_PAIR) {
		check = check_pairing;
	} else if (action == ACTION_STOP) {
		check = check_awake;
	}

	err = link_connect(PEER_GOPRO, action == ACTION_PAIR, check,
			   action == ACTION_PAIR ? PAIRING_TIMEOUT : CONNECT_TIMEOUT);
	if (err == -EALREADY) {
		LOG_INF("Asleep already");
		return 0;
	}
	if (err) {
		return err;
	}

	err = link_find(PEER_GOPRO, &command_uuid.uuid, &command_handle);
	if (!err) {
		err = link_find(PEER_GOPRO, &response_uuid.uuid, &response_handle);
	}
	if (!err) {
		err = link_subscribe(PEER_GOPRO, response_handle);
	}
	if (err) {
		LOG_ERR("Command characteristics not found (%d)", err);
		goto out;
	}

	/* Hardware info answers once the camera is ready for commands */
	err = command_retry(cmd_hw_info, sizeof(cmd_hw_info), "Waiting for camera");
	if (err) {
		goto out;
	}

	switch (action) {
	case ACTION_START:
		err = command_retry(cmd_shutter_on, sizeof(cmd_shutter_on), "Shutter on");
		if (!err) {
			LOG_INF("Recording");
		}
		break;

	case ACTION_STOP:
		err = command_retry(cmd_shutter_off, sizeof(cmd_shutter_off), "Shutter off");
		if (err) {
			break;
		}
		err = command_retry(cmd_sleep, sizeof(cmd_sleep), "Sleep");
		/* The camera may drop the link before it answers */
		if (err == -ENOTCONN) {
			err = 0;
		}
		if (!err) {
			LOG_INF("Stopped and asleep");
		}
		break;

	case ACTION_PAIR:
		err = link_find(PEER_GOPRO, &network_uuid.uuid, &network_handle);
		if (!err) {
			err = link_write(PEER_GOPRO, network_handle, cmd_pairing_finish,
					 sizeof(cmd_pairing_finish));
		}
		if (err) {
			LOG_ERR("Finishing pairing failed (%d)", err);
		} else {
			LOG_INF("Paired");
		}
		break;
	}

out:
	link_disconnect(PEER_GOPRO);
	return err;
}
