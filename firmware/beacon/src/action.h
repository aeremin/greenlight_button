#ifndef ACTION_H_
#define ACTION_H_

#include <zephyr/kernel.h>

enum action {
	/* GoPro records video, FlySight logs a GPS track */
	ACTION_START,
	/* Both stop recording and go to sleep */
	ACTION_STOP,
	/* Bond with devices that are in pairing mode */
	ACTION_PAIR,
};

/* How long a start or stop waits for a device to show up */
#define CONNECT_TIMEOUT K_SECONDS(30)
/* How long pairing mode lasts */
#define PAIRING_TIMEOUT K_SECONDS(60)

/* Each returns 0 on success, or -ENOENT if the device was never paired. */
int gopro_run(enum action action);
int flysight_run(enum action action);

#endif /* ACTION_H_ */
