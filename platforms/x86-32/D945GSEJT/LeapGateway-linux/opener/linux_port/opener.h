/*******************************************************************************
 * OpENer Linux integration API (same surface as RTEMS opener.h).
 ******************************************************************************/

#ifndef OPENER_LINUX_OPENER_H_
#define OPENER_LINUX_OPENER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void opener_init(const char *ifname);
void opener_cyclic(void);
void opener_shutdown(void);
int opener_get_status(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENER_LINUX_OPENER_H_ */
