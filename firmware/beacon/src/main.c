#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(beacon, LOG_LEVEL_INF);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

/* Placeholder payload. 0xFFFF is the company ID the Bluetooth SIG reserves for testing. */
static uint8_t mfg_data[] = {0xff, 0xff, 0x00};

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, sizeof(mfg_data)),
};

/* Non-connectable, roughly once a second to spare the coin cell. */
static const struct bt_le_adv_param adv_param = BT_LE_ADV_PARAM_INIT(
	BT_LE_ADV_OPT_NONE, BT_GAP_ADV_SLOW_INT_MIN, BT_GAP_ADV_SLOW_INT_MAX, NULL);

int main(void)
{
	int err;

	if (gpio_is_ready_dt(&led)) {
		gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	}

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (%d)", err);
		return 0;
	}

	err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Advertising failed to start (%d)", err);
		return 0;
	}

	LOG_INF("Advertising as \"%s\"", CONFIG_BT_DEVICE_NAME);

	/* One short blink to show the firmware is alive. */
	gpio_pin_set_dt(&led, 1);
	k_msleep(100);
	gpio_pin_set_dt(&led, 0);

	return 0;
}
