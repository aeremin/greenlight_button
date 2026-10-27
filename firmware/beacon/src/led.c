#include "led.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

/* Step durations in ms, alternating on and off. Without steps the LED holds level. */
struct pattern {
	const uint16_t *steps;
	uint8_t len;
	bool repeat;
	bool level;
};

#define STEPS(...) .steps = (const uint16_t[]){__VA_ARGS__}, \
		   .len = sizeof((uint16_t[]){__VA_ARGS__}) / sizeof(uint16_t)

static const struct pattern modes[] = {
	[LED_OFF] = {.level = false},
	[LED_ON] = {.level = true},
	[LED_BUSY] = {STEPS(30, 970), .repeat = true},
	[LED_PAIRING] = {STEPS(60, 140, 60, 740), .repeat = true},
};

static const struct pattern flashes[] = {
	[LED_FLASH_OK] = {STEPS(1000, 300)},
	[LED_FLASH_FAIL] = {STEPS(100, 100, 100, 100, 100, 100, 100, 100, 100, 300)},
};

static struct k_spinlock lock;
static const struct pattern *base = &modes[LED_OFF];
static const struct pattern *flash;
static const struct pattern *current = &modes[LED_OFF];
static uint8_t step_index;

static void step(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(step_work, step);

static void step(struct k_work *work)
{
	k_spinlock_key_t key = k_spin_lock(&lock);
	uint16_t delay = 0;
	bool level;

	for (;;) {
		if (step_index < current->len) {
			level = step_index % 2 == 0;
			delay = current->steps[step_index++];
			break;
		}
		if (current == flash) {
			flash = NULL;
			current = base;
		} else if (current->len > 0 && current->repeat) {
			/* Start over below */
		} else {
			level = current->level;
			break;
		}
		step_index = 0;
	}

	k_spin_unlock(&lock, key);

	gpio_pin_set_dt(&led, level);
	if (delay) {
		k_work_reschedule(&step_work, K_MSEC(delay));
	}
}

static void play(const struct pattern *pattern)
{
	current = pattern;
	step_index = 0;
	k_work_reschedule(&step_work, K_NO_WAIT);
}

int led_init(void)
{
	if (!gpio_is_ready_dt(&led)) {
		return -ENODEV;
	}
	return gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
}

void led_set(enum led_mode mode)
{
	k_spinlock_key_t key = k_spin_lock(&lock);

	base = &modes[mode];
	if (!flash) {
		play(base);
	}

	k_spin_unlock(&lock, key);
}

void led_flash(enum led_flash which)
{
	k_spinlock_key_t key = k_spin_lock(&lock);

	flash = &flashes[which];
	play(flash);

	k_spin_unlock(&lock, key);
}
