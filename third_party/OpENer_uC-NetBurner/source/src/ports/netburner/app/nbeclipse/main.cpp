/* NBEclipse entry — Opener RTOS task; see makefile.init.snippet for link/CPPFLAGS. */

#include <predef.h>
#include <stdio.h>
#include <init.h>
#include <nbrtos.h>
#include <hal.h>
#include <system.h>

extern "C" {
#include "opener_user_conf.h"
#include "opener.h"
}

#ifndef OPENER_TASK_PRIO
#define OPENER_TASK_PRIO 16
#endif

#ifndef OPENER_NB_IFNUM
#define OPENER_NB_IFNUM 1
#endif

static OS_TASK s_opener_task;

extern "C" void OpenerNbScheduleRebootNow(void) {
  ForceReboot(false);
}

extern "C" void OpenerNbScheduleFactoryResetNow(void) {
  EraseWholeConfigRecord();
  ForceReboot(false);
}

/** Opener RTOS task loop. */
static void OpenerTask(void *) {
  iprintf("OpENer: task start (ifnum=%d)\r\n", OPENER_NB_IFNUM);
  opener_init((OpenerNetIfHandle)(intptr_t)OPENER_NB_IFNUM);

  if(0 != opener_get_status()) {
    iprintf("OpENer: init failed; adapter not running\r\n");
  }

  while(0 == opener_get_status()) {
    opener_process();
    OSTimeDly(1);
  }

  iprintf("OpENer: task exit (status=%d)\r\n", opener_get_status());
  opener_shutdown();
}

/** NBEclipse UserMain — create Opener task after init(). */
void UserMain(void *) {
  init();
  iprintf("OpENer: UserMain initialized\r\n");

  OSTaskCreate(&s_opener_task,
               "Opener",
               OpenerTask,
               NULL,
               OPENER_TASK_PRIO,
               NULL,
               8192);
  iprintf("OpENer: task created\r\n");

  while(1) {
    OSTimeDly(TICKS_PER_SECOND);
  }
}
