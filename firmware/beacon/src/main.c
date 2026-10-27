#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include "action.h"
#include "led.h"
#include "link.h"

LOG_MODULE_REGISTER(beacon, LOG_LEVEL_INF);

/* Shorter presses start, longer ones stop */
#define LONG_PRESS_MS    1000
/* Holding this long enters pairing mode without waiting for the release */
#define PAIRING_PRESS_MS 10000

#define PEER_STACK_SIZE 2048
#define PEER_PRIORITY   K_PRIO_PREEMPT(5)

static const char *const action_names[] = {
	[ACTION_START] = "Start",
	[ACTION_STOP] = "Stop",
	[ACTION_PAIR] = "Pairing",
};

static int (*const runners[PEER_COUNT])(enum action action) = {
	[PEER_GOPRO] = gopro_run,
	[PEER_FLYSIGHT] = flysight_run,
};

/* Holds only the latest request */
K_MSGQ_DEFINE(requests, sizeof(enum action), 1, 4);

K_THREAD_STACK_ARRAY_DEFINE(peer_stacks, PEER_COUNT, PEER_STACK_SIZE);
static struct k_thread peer_threads[PEER_COUNT];
static int results[PEER_COUNT];

static int64_t pressed_at;
static bool press_used;
static atomic_t pairing;

static void request(enum action action)
{
	/* The newest press wins over anything queued or running. Abort first, so the new action
	 * isn't cut short if main picks it up straight away.
	 */
	link_abort();
	k_msgq_purge(&requests);
	k_msgq_put(&requests, &action, K_NO_WAIT);
}

static void hold_handler(struct k_work *work)
{
	int64_t held = k_uptime_get() - pressed_at;

	if (held < PAIRING_PRESS_MS) {
		/* Releasing from here on is a long press */
		led_set(LED_OFF);
		k_work_reschedule(k_work_delayable_from_work(work),
				  K_MSEC(PAIRING_PRESS_MS - held));
		return;
	}

	press_used = true;
	request(ACTION_PAIR);
}

static K_WORK_DELAYABLE_DEFINE(hold_work, hold_handler);

static void on_input(struct input_event *evt, void *user_data)
{
	/* The jig button works like the on-board one */
	if (evt->type != INPUT_EV_KEY || (evt->code != INPUT_KEY_0 && evt->code != INPUT_KEY_1)) {
		return;
	}

	LOG_INF("Button %d %s", evt->code == INPUT_KEY_0 ? 0 : 1,
		evt->value ? "pressed" : "released");

	if (evt->value) {
		pressed_at = k_uptime_get();
		press_used = false;
		led_set(LED_ON);
		k_work_reschedule(&hold_work, K_MSEC(LONG_PRESS_MS));
		return;
	}

	k_work_cancel_delayable(&hold_work);
	if (!press_used) {
		button_press(k_uptime_get() - pressed_at);
	}
}

INPUT_CALLBACK_DEFINE(NULL, on_input, NULL);

void button_press(uint32_t held_ms)
{
	LOG_INF("Press held %u ms", held_ms);

	if (held_ms >= PAIRING_PRESS_MS) {
		request(ACTION_PAIR);
	} else if (atomic_get(&pairing)) {
		/* A press only ends pairing mode */
		link_abort();
	} else {
		request(held_ms < LONG_PRESS_MS ? ACTION_START : ACTION_STOP);
	}
}

static void peer_main(void *peer, void *action, void *unused)
{
	int err = runners[(uintptr_t)peer]((enum action)(uintptr_t)action);

	results[(uintptr_t)peer] = err;

	/* Each device pairs on its own, so confirm each one as it happens */
	if ((uintptr_t)action == ACTION_PAIR && !err) {
		led_flash(LED_FLASH_OK);
	}
}

static void run(enum action action)
{
	int ok = 0, failed = 0;

	LOG_INF("%s", action_names[action]);

	link_clear_abort();
	atomic_set(&pairing, action == ACTION_PAIR);
	led_set(action == ACTION_PAIR ? LED_PAIRING : LED_BUSY);

	/* Both devices at once: each takes seconds to wake */
	for (int i = 0; i < PEER_COUNT; i++) {
		k_thread_create(&peer_threads[i], peer_stacks[i], K_THREAD_STACK_SIZEOF(peer_stacks[i]),
				peer_main, (void *)(uintptr_t)i, (void *)(uintptr_t)action, NULL,
				PEER_PRIORITY, 0, K_NO_WAIT);
	}
	for (int i = 0; i < PEER_COUNT; i++) {
		k_thread_join(&peer_threads[i], K_FOREVER);
	}

	atomic_set(&pairing, 0);
	led_set(LED_OFF);

	/* A newer press cut this one short and takes over the LED */
	if (k_msgq_num_used_get(&requests) > 0) {
		return;
	}

	for (int i = 0; i < PEER_COUNT; i++) {
		if (results[i] == 0) {
			ok++;
		} else if (results[i] != -ENOENT) {
			failed++;
		}
	}

	LOG_INF("%s done: %d ok, %d failed", action_names[action], ok, failed);

	if (action == ACTION_PAIR) {
		if (ok == 0) {
			led_flash(LED_FLASH_FAIL);
		}
	} else {
		led_flash(ok > 0 && failed == 0 ? LED_FLASH_OK : LED_FLASH_FAIL);
	}
}

int main(void)
{
	enum action action;
	int err;

	err = led_init();
	if (err) {
		LOG_ERR("LED init failed (%d)", err);
	}

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (%d)", err);
		return 0;
	}

	err = settings_load();
	if (err) {
		LOG_ERR("Loading settings failed (%d)", err);
	}

	err = link_init();
	if (err) {
		LOG_ERR("Link init failed (%d)", err);
		return 0;
	}

	LOG_INF("Ready. GoPro %s, FlySight %s",
		link_is_paired(PEER_GOPRO) ? "paired" : "not paired",
		link_is_paired(PEER_FLYSIGHT) ? "paired" : "not paired");

	/* One flash to show the firmware is alive */
	led_flash(LED_FLASH_OK);

	for (;;) {
		/* Fails with -ENOMSG when request() purges the queue under us */
		if (k_msgq_get(&requests, &action, K_FOREVER) == 0) {
			run(action);
		}
	}

	return 0;
}
