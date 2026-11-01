#ifndef ERGODOX_CONFIGURATOR_MODE_H
#define ERGODOX_CONFIGURATOR_MODE_H

#include <stdbool.h>

/* Returns zero and the boot decision, or a negative GPIO error. */
int configurator_mode_requested(bool *requested);
/* Start the configurator services and wait until the device is rebooted. */
void configurator_mode_run(void);

#endif
