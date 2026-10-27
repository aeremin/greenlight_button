#ifndef LED_H_
#define LED_H_

enum led_mode {
	LED_OFF,
	LED_ON,
	/* Brief blink each second while a start or stop runs */
	LED_BUSY,
	/* Double blink each second in pairing mode */
	LED_PAIRING,
};

enum led_flash {
	/* One long flash */
	LED_FLASH_OK,
	/* Five quick flashes */
	LED_FLASH_FAIL,
};

int led_init(void);

/* The pattern to show when no flash is playing. */
void led_set(enum led_mode mode);

/* Play a flash once, then return to the mode. */
void led_flash(enum led_flash flash);

#endif /* LED_H_ */
