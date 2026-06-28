/** Firmware entry points — copy with opener.c; drive from app/nbeclipse/main.cpp. */
#ifndef OPENER_NB_APP_OPENER_H_
#define OPENER_NB_APP_OPENER_H_

#include "opener_app_preinclude.h"
#include "opener_hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Set on shutdown or fatal init/cyclic error; polled by HAL wait paths. */
extern volatile int g_opener_abort;

/** One-time bring-up. @p netif is 1-based NNDK index (NULL = default). */
void opener_init(OpenerNetIfHandle netif);

/** One stack tick; call every kOpenerTimerTickInMilliSeconds. */
void opener_process(void);

/** Tear down handler, wire modules, and CIP stack. */
void opener_shutdown(void);

/** Non-zero when the stack stopped (use to exit the Opener task loop). */
int opener_get_status(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENER_NB_APP_OPENER_H_ */
