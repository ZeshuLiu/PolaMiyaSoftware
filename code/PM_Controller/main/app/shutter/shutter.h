#ifndef PM_SHUTTER_H
#define PM_SHUTTER_H

#include <stdint.h>

/* Call once after flash_trigger_init(); enables falling-edge flash triggering. */
void shutter_init(void);

/* Take accepted falling edges recorded since the previous call, in task context. */
uint64_t shutter_take_count(void);

#endif
