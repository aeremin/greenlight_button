#ifndef LINK_H_
#define LINK_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>

/*
 * Connections to the remote devices. Each peer has at most one connection, so the blocking
 * calls below name the peer rather than the connection. Every peer has its own thread.
 */

enum peer {
	PEER_GOPRO,
	PEER_FLYSIGHT,
	PEER_COUNT,
};

/*
 * Looks at an advertisement from a candidate. Returns 0 to connect, a positive value to skip
 * this report and keep scanning, or a negative error to give up with that error.
 */
typedef int (*link_adv_check_t)(uint8_t adv_type, struct net_buf_simple *ad);

/* Load the peer roles saved by earlier pairings. Call after settings_load(). */
int link_init(void);

bool link_is_paired(enum peer peer);

/*
 * Scan until the peer advertises, connect, and encrypt the link with the bond keys. With pair
 * set, connect instead to any device that check accepts and bond with it as this peer. check
 * may be NULL when not pairing.
 */
int link_connect(enum peer peer, bool pair, link_adv_check_t check, k_timeout_t timeout);

void link_disconnect(enum peer peer);

/* Abandon waits for a connection and link_sleep() in every peer thread. */
void link_abort(void);

/* Re-arm the peer threads after link_abort(). */
void link_clear_abort(void);

/* Sleep that link_abort() cuts short with -ECANCELED. */
int link_sleep(uint32_t ms);

int link_find(enum peer peer, const struct bt_uuid *uuid, uint16_t *handle);
int link_write(enum peer peer, uint16_t handle, const void *data, uint16_t len);

/* Returns the number of bytes read. */
int link_read(enum peer peer, uint16_t handle, void *buf, uint16_t size);

/* Queue notifications from this characteristic for link_recv(). */
int link_subscribe(enum peer peer, uint16_t handle);

/* Drop queued notifications. */
void link_flush(enum peer peer);

/* Returns the length of the next queued notification, truncated to size. */
int link_recv(enum peer peer, void *buf, size_t size, k_timeout_t timeout);

/*
 * Find manufacturer specific data for a company in an advertisement. Returns the length of the
 * data after the company ID, copied to buf up to size, or -ENOENT.
 */
int link_mfg_data(struct net_buf_simple *ad, uint16_t company, uint8_t *buf, size_t size);

#endif /* LINK_H_ */
