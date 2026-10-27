#include "link.h"

#include <errno.h>
#include <string.h>

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(link, LOG_LEVEL_INF);

#define SETTINGS_ROOT "greenlight"
#define NOTIFY_MAX    20
#define NOTIFY_QUEUE  8
#define POLL_MS       100

struct notification {
	uint8_t len;
	uint8_t data[NOTIFY_MAX];
};

struct link {
	const char *name;
	const char *key;
	/* Identity address of the bonded device, or BT_ADDR_LE_ANY */
	bt_addr_le_t addr;
	/* Set when pairing replaced the bonded device, until link_connect() saves the change */
	bool addr_changed;
	bt_addr_le_t replaced;

	/* While link_connect() waits for the peer */
	bool connecting;
	bool pair;
	link_adv_check_t check;

	struct bt_conn *conn;
	/* Completion of the connection or of the GATT procedure in flight, with err */
	struct k_sem done;
	int err;
	/* Given when conn is released */
	struct k_sem gone;

	struct bt_gatt_discover_params discover;
	uint16_t found;
	struct bt_gatt_read_params read;
	uint8_t *read_buf;
	uint16_t read_size;
	struct bt_gatt_write_params write;

	struct bt_gatt_subscribe_params sub;
	struct bt_gatt_discover_params sub_discover;
	struct k_msgq rx;
	char rx_buf[NOTIFY_QUEUE * sizeof(struct notification)];
};

static struct link links[PEER_COUNT] = {
	[PEER_GOPRO] = {.name = "GoPro", .key = "gopro"},
	[PEER_FLYSIGHT] = {.name = "FlySight", .key = "flysight"},
};

/* Guards the connection state shared with the Bluetooth callbacks */
static K_MUTEX_DEFINE(lock);
static bool scanning;
static bool initiating;
static atomic_t aborted;

static const struct bt_le_scan_param scan_param = BT_LE_SCAN_PARAM_INIT(
	BT_LE_SCAN_TYPE_PASSIVE, BT_LE_SCAN_OPT_NONE, BT_GAP_SCAN_FAST_INTERVAL,
	BT_GAP_SCAN_FAST_INTERVAL);

static bool paired(const struct link *l)
{
	return !bt_addr_le_eq(&l->addr, BT_ADDR_LE_ANY);
}

static struct link *find_link(const struct bt_conn *conn)
{
	ARRAY_FOR_EACH_PTR(links, l) {
		if (l->conn == conn) {
			return l;
		}
	}
	return NULL;
}

static struct bt_conn *get_conn(struct link *l)
{
	struct bt_conn *conn = NULL;

	k_mutex_lock(&lock, K_FOREVER);
	if (l->conn) {
		conn = bt_conn_ref(l->conn);
	}
	k_mutex_unlock(&lock);
	return conn;
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
			 struct net_buf_simple *ad);

/* Scan while a peer is wanted and no connection is being created. Call with lock held. */
static void scan_update(void)
{
	bool want = false;
	int err;

	if (!initiating) {
		ARRAY_FOR_EACH_PTR(links, l) {
			want |= l->connecting && !l->conn;
		}
	}

	if (want == scanning) {
		return;
	}

	if (want) {
		err = bt_le_scan_start(&scan_param, device_found);
	} else {
		err = bt_le_scan_stop();
	}
	if (err) {
		LOG_ERR("Scan %s failed (%d)", want ? "start" : "stop", err);
		return;
	}
	scanning = want;
}

/* End link_connect() with err. Call with lock held. */
static void finish(struct link *l, int err)
{
	l->err = err;
	l->connecting = false;
	k_sem_give(&l->done);
	scan_update();
}

static void connect(struct link *l, const bt_addr_le_t *addr)
{
	char str[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(addr, str, sizeof(str));
	LOG_INF("%s: connecting to %s", l->name, str);

	/* Stop scanning first, as the controller may not scan and initiate at once */
	initiating = true;
	scan_update();

	err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &l->conn);
	if (err) {
		LOG_WRN("%s: connection failed to start (%d)", l->name, err);
		initiating = false;
		scan_update();
	}
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
			 struct net_buf_simple *ad)
{
	if (adv_type != BT_GAP_ADV_TYPE_ADV_IND && adv_type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
		return;
	}

	k_mutex_lock(&lock, K_FOREVER);

	/* Reports can trail a scan stop */
	if (!scanning || initiating) {
		goto out;
	}

	ARRAY_FOR_EACH_PTR(links, l) {
		struct net_buf_simple_state state;
		int res = 0;

		if (!l->connecting || l->conn) {
			continue;
		}

		if (l->pair) {
			if (!l->check) {
				continue;
			}
		} else if (!bt_addr_le_eq(addr, &l->addr)) {
			continue;
		}

		if (l->check) {
			net_buf_simple_save(ad, &state);
			res = l->check(adv_type, ad);
			net_buf_simple_restore(ad, &state);
		}

		if (res < 0) {
			finish(l, res);
		} else if (res == 0) {
			connect(l, addr);
		} else {
			continue;
		}
		break;
	}

out:
	k_mutex_unlock(&lock);
}

static void release(struct link *l)
{
	bt_conn_unref(l->conn);
	l->conn = NULL;
	k_sem_give(&l->gone);
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	struct link *l;
	int err;

	k_mutex_lock(&lock, K_FOREVER);

	l = find_link(conn);
	if (!l) {
		goto out;
	}

	initiating = false;

	if (conn_err) {
		LOG_WRN("%s: connection failed (0x%02x)", l->name, conn_err);
		release(l);
		scan_update();
		goto out;
	}

	LOG_INF("%s: connected", l->name);

	/* Encrypts with the bond keys, or pairs when there are none */
	err = bt_conn_set_security(conn, BT_SECURITY_L2);
	if (err) {
		LOG_WRN("%s: security request failed (%d)", l->name, err);
		bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
	}

	scan_update();

out:
	k_mutex_unlock(&lock);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	struct link *l;

	k_mutex_lock(&lock, K_FOREVER);

	l = find_link(conn);
	if (!l) {
		goto out;
	}

	LOG_INF("%s: disconnected (0x%02x)", l->name, reason);
	release(l);

	if (l->connecting) {
		/* Not up yet; try again on the next advertisement */
		scan_update();
	} else {
		/* Wake whatever GATT procedure is waiting */
		l->err = -ENOTCONN;
		k_sem_give(&l->done);
	}

out:
	k_mutex_unlock(&lock);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	struct link *l;

	k_mutex_lock(&lock, K_FOREVER);

	l = find_link(conn);
	if (!l || !l->connecting) {
		goto out;
	}

	if (err == BT_SECURITY_ERR_PIN_OR_KEY_MISSING && l->pair) {
		/* The device forgot the bond. Drop it and pair afresh on the next connection. */
		LOG_INF("%s: bond rejected, pairing again", l->name);
		if (bt_addr_le_eq(bt_conn_get_dst(conn), &l->addr)) {
			bt_addr_le_copy(&l->replaced, &l->addr);
			bt_addr_le_copy(&l->addr, BT_ADDR_LE_ANY);
			l->addr_changed = true;
		}
		bt_unpair(BT_ID_DEFAULT, bt_conn_get_dst(conn));
		goto out;
	}

	if (err) {
		LOG_WRN("%s: security failed (%d)", l->name, err);
		finish(l, -EACCES);
		bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
		goto out;
	}

	/* A new bond is only complete once pairing_complete() has the keys */
	if (!l->pair || bt_addr_le_eq(bt_conn_get_dst(conn), &l->addr)) {
		LOG_INF("%s: encrypted", l->name);
		finish(l, 0);
	}

out:
	k_mutex_unlock(&lock);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
};

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	struct link *l;

	k_mutex_lock(&lock, K_FOREVER);

	l = find_link(conn);
	if (!l || !l->connecting || !l->pair) {
		goto out;
	}

	if (!bonded) {
		LOG_WRN("%s: paired without bonding", l->name);
		finish(l, -EACCES);
		bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
		goto out;
	}

	/* bt_conn_get_dst() is the identity address once the keys are exchanged */
	if (!bt_addr_le_eq(bt_conn_get_dst(conn), &l->addr)) {
		if (!l->addr_changed) {
			bt_addr_le_copy(&l->replaced, &l->addr);
		}
		bt_addr_le_copy(&l->addr, bt_conn_get_dst(conn));
		l->addr_changed = true;
	}

	LOG_INF("%s: bonded", l->name);
	finish(l, 0);

out:
	k_mutex_unlock(&lock);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	struct link *l;

	k_mutex_lock(&lock, K_FOREVER);

	l = find_link(conn);
	if (l && l->connecting && l->pair) {
		LOG_WRN("%s: pairing failed (%d)", l->name, reason);
		finish(l, -EACCES);
		bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
	}

	k_mutex_unlock(&lock);
}

static struct bt_conn_auth_info_cb auth_info_callbacks = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed,
};

static int settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	ARRAY_FOR_EACH_PTR(links, l) {
		if (!settings_name_steq(name, l->key, NULL)) {
			continue;
		}
		if (len != sizeof(l->addr)) {
			return -EINVAL;
		}
		return MIN(read_cb(cb_arg, &l->addr, sizeof(l->addr)), 0);
	}
	return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(greenlight, SETTINGS_ROOT, NULL, settings_set, NULL, NULL);

static void save_addr(struct link *l)
{
	char key[32];
	int err;

	snprintk(key, sizeof(key), SETTINGS_ROOT "/%s", l->key);
	if (paired(l)) {
		err = settings_save_one(key, &l->addr, sizeof(l->addr));
	} else {
		err = settings_delete(key);
	}
	if (err) {
		LOG_ERR("%s: saving the bonded address failed (%d)", l->name, err);
	}
}

struct stale_bonds {
	bt_addr_le_t addr[CONFIG_BT_MAX_PAIRED];
	size_t count;
};

static void find_stale_bond(const struct bt_bond_info *info, void *user_data)
{
	struct stale_bonds *stale = user_data;

	ARRAY_FOR_EACH_PTR(links, l) {
		if (bt_addr_le_eq(&info->addr, &l->addr)) {
			return;
		}
	}
	if (stale->count < ARRAY_SIZE(stale->addr)) {
		bt_addr_le_copy(&stale->addr[stale->count++], &info->addr);
	}
}

int link_init(void)
{
	struct stale_bonds stale = {0};
	int err;

	ARRAY_FOR_EACH_PTR(links, l) {
		k_sem_init(&l->done, 0, 1);
		k_sem_init(&l->gone, 0, 1);
		k_msgq_init(&l->rx, l->rx_buf, sizeof(struct notification), NOTIFY_QUEUE);

		/* A role whose keys are gone, e.g. after a failed save, can't connect */
		if (paired(l) && !bt_le_bond_exists(BT_ID_DEFAULT, &l->addr)) {
			LOG_WRN("%s: bond missing, forgetting it", l->name);
			bt_addr_le_copy(&l->addr, BT_ADDR_LE_ANY);
			save_addr(l);
		}
	}

	/* Bonds without a role only take up room */
	bt_foreach_bond(BT_ID_DEFAULT, find_stale_bond, &stale);
	for (size_t i = 0; i < stale.count; i++) {
		bt_unpair(BT_ID_DEFAULT, &stale.addr[i]);
	}

	err = bt_conn_auth_info_cb_register(&auth_info_callbacks);
	if (err) {
		return err;
	}

	return 0;
}

bool link_is_paired(enum peer peer)
{
	return paired(&links[peer]);
}

int link_connect(enum peer peer, bool pair, link_adv_check_t check, k_timeout_t timeout)
{
	struct link *l = &links[peer];
	k_timepoint_t end = sys_timepoint_calc(timeout);
	int err;

	if (!pair && !paired(l)) {
		return -ENOENT;
	}

	k_mutex_lock(&lock, K_FOREVER);
	k_sem_reset(&l->done);
	l->pair = pair;
	l->check = check;
	l->connecting = true;
	scan_update();
	k_mutex_unlock(&lock);

	while (k_sem_take(&l->done, K_MSEC(POLL_MS)) != 0) {
		if (!atomic_get(&aborted) && !sys_timepoint_expired(end)) {
			continue;
		}

		k_mutex_lock(&lock, K_FOREVER);
		if (!l->connecting) {
			/* Finished just now */
			k_mutex_unlock(&lock);
			break;
		}
		l->connecting = false;
		scan_update();
		k_mutex_unlock(&lock);

		link_disconnect(peer);
		err = atomic_get(&aborted) ? -ECANCELED : -ETIMEDOUT;
		goto out;
	}

	err = l->err;

out:
	k_mutex_lock(&lock, K_FOREVER);
	if (l->addr_changed) {
		l->addr_changed = false;
		save_addr(l);
		if (!bt_addr_le_eq(&l->replaced, BT_ADDR_LE_ANY) &&
		    !bt_addr_le_eq(&l->replaced, &l->addr)) {
			bt_unpair(BT_ID_DEFAULT, &l->replaced);
		}
	}
	k_mutex_unlock(&lock);

	return err;
}

void link_disconnect(enum peer peer)
{
	struct link *l = &links[peer];
	struct bt_conn *conn;

	k_mutex_lock(&lock, K_FOREVER);
	if (!l->conn) {
		k_mutex_unlock(&lock);
		return;
	}
	k_sem_reset(&l->gone);
	conn = bt_conn_ref(l->conn);
	k_mutex_unlock(&lock);

	/* Also cancels a connection still being created */
	bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	bt_conn_unref(conn);

	if (k_sem_take(&l->gone, K_SECONDS(10)) != 0) {
		LOG_ERR("%s: disconnect timed out", l->name);
	}
}

void link_abort(void)
{
	atomic_set(&aborted, 1);
}

void link_clear_abort(void)
{
	atomic_set(&aborted, 0);
}

int link_sleep(uint32_t ms)
{
	while (ms > 0 && !atomic_get(&aborted)) {
		uint32_t step = MIN(ms, POLL_MS);

		k_msleep(step);
		ms -= step;
	}
	return atomic_get(&aborted) ? -ECANCELED : 0;
}

/*
 * GATT procedures complete through their callback, an ATT timeout or a disconnect, so the
 * waits below need no timeout of their own.
 */
static int wait_done(struct link *l, int err)
{
	if (err) {
		return err;
	}
	k_sem_take(&l->done, K_FOREVER);
	return l->err;
}

static uint8_t discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			   struct bt_gatt_discover_params *params)
{
	struct link *l = CONTAINER_OF(params, struct link, discover);

	if (attr) {
		l->found = ((const struct bt_gatt_chrc *)attr->user_data)->value_handle;
		l->err = 0;
	} else {
		l->err = -ENOENT;
	}
	k_sem_give(&l->done);
	return BT_GATT_ITER_STOP;
}

int link_find(enum peer peer, const struct bt_uuid *uuid, uint16_t *handle)
{
	struct link *l = &links[peer];
	struct bt_conn *conn = get_conn(l);
	int err;

	if (!conn) {
		return -ENOTCONN;
	}

	l->discover = (struct bt_gatt_discover_params){
		.uuid = uuid,
		.func = discover_cb,
		.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE,
		.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE,
		.type = BT_GATT_DISCOVER_CHARACTERISTIC,
	};
	k_sem_reset(&l->done);
	err = wait_done(l, bt_gatt_discover(conn, &l->discover));
	bt_conn_unref(conn);

	if (!err) {
		*handle = l->found;
	}
	return err;
}

static void write_cb(struct bt_conn *conn, uint8_t att_err, struct bt_gatt_write_params *params)
{
	struct link *l = CONTAINER_OF(params, struct link, write);

	l->err = att_err ? -EIO : 0;
	k_sem_give(&l->done);
}

int link_write(enum peer peer, uint16_t handle, const void *data, uint16_t len)
{
	struct link *l = &links[peer];
	struct bt_conn *conn = get_conn(l);
	int err;

	if (!conn) {
		return -ENOTCONN;
	}

	l->write = (struct bt_gatt_write_params){
		.func = write_cb,
		.handle = handle,
		.data = data,
		.length = len,
	};
	k_sem_reset(&l->done);
	err = wait_done(l, bt_gatt_write(conn, &l->write));
	bt_conn_unref(conn);
	return err;
}

static uint8_t read_cb(struct bt_conn *conn, uint8_t att_err, struct bt_gatt_read_params *params,
		       const void *data, uint16_t length)
{
	struct link *l = CONTAINER_OF(params, struct link, read);

	if (att_err) {
		l->err = -EIO;
	} else if (data) {
		l->err = MIN(length, l->read_size);
		memcpy(l->read_buf, data, l->err);
	} else {
		l->err = 0;
	}
	k_sem_give(&l->done);
	return BT_GATT_ITER_STOP;
}

int link_read(enum peer peer, uint16_t handle, void *buf, uint16_t size)
{
	struct link *l = &links[peer];
	struct bt_conn *conn = get_conn(l);
	int err;

	if (!conn) {
		return -ENOTCONN;
	}

	l->read_buf = buf;
	l->read_size = size;
	l->read = (struct bt_gatt_read_params){
		.func = read_cb,
		.handle_count = 1,
		.single.handle = handle,
	};
	k_sem_reset(&l->done);
	err = wait_done(l, bt_gatt_read(conn, &l->read));
	bt_conn_unref(conn);
	return err;
}

static uint8_t notify_cb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
			 const void *data, uint16_t length)
{
	struct link *l = CONTAINER_OF(params, struct link, sub);
	struct notification n;

	if (!data) {
		/* Unsubscribed, e.g. by the disconnect */
		return BT_GATT_ITER_STOP;
	}

	n.len = MIN(length, sizeof(n.data));
	memcpy(n.data, data, n.len);
	if (k_msgq_put(&l->rx, &n, K_NO_WAIT) != 0) {
		LOG_WRN("%s: notification dropped", l->name);
	}
	return BT_GATT_ITER_CONTINUE;
}

static void subscribe_cb(struct bt_conn *conn, uint8_t att_err,
			 struct bt_gatt_subscribe_params *params)
{
	struct link *l = CONTAINER_OF(params, struct link, sub);

	l->err = att_err ? -EIO : 0;
	k_sem_give(&l->done);
}

int link_subscribe(enum peer peer, uint16_t handle)
{
	struct link *l = &links[peer];
	struct bt_conn *conn = get_conn(l);
	int err;

	if (!conn) {
		return -ENOTCONN;
	}

	l->sub = (struct bt_gatt_subscribe_params){
		.notify = notify_cb,
		.subscribe = subscribe_cb,
		.value = BT_GATT_CCC_NOTIFY,
		.value_handle = handle,
		.ccc_handle = BT_GATT_AUTO_DISCOVER_CCC_HANDLE,
		.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE,
		.disc_params = &l->sub_discover,
	};
	/* Peers don't keep subscriptions across connections, so neither do we */
	atomic_set_bit(l->sub.flags, BT_GATT_SUBSCRIBE_FLAG_VOLATILE);

	k_sem_reset(&l->done);
	err = wait_done(l, bt_gatt_subscribe(conn, &l->sub));
	bt_conn_unref(conn);
	return err;
}

void link_flush(enum peer peer)
{
	k_msgq_purge(&links[peer].rx);
}

int link_recv(enum peer peer, void *buf, size_t size, k_timeout_t timeout)
{
	struct notification n;
	int err;

	err = k_msgq_get(&links[peer].rx, &n, timeout);
	if (err) {
		return err;
	}

	memcpy(buf, n.data, MIN(n.len, size));
	return MIN(n.len, size);
}

struct mfg_search {
	uint16_t company;
	uint8_t *buf;
	size_t size;
	int len;
};

static bool mfg_data_cb(struct bt_data *data, void *user_data)
{
	struct mfg_search *search = user_data;

	if (data->type != BT_DATA_MANUFACTURER_DATA || data->data_len < 2 ||
	    sys_get_le16(data->data) != search->company) {
		return true;
	}

	search->len = data->data_len - 2;
	memcpy(search->buf, &data->data[2], MIN(search->len, search->size));
	return false;
}

int link_mfg_data(struct net_buf_simple *ad, uint16_t company, uint8_t *buf, size_t size)
{
	struct mfg_search search = {
		.company = company,
		.buf = buf,
		.size = size,
		.len = -ENOENT,
	};

	bt_data_parse(ad, mfg_data_cb, &search);
	return search.len;
}
