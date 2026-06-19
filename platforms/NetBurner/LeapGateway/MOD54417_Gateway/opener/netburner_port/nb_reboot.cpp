/*******************************************************************************
 * Schedule a deferred reboot so the CIP Reset response can be sent first.
 ******************************************************************************/

#include "nb_reboot.h"

#include <hal.h>
#include <nbrtos.h>
#include <predef.h>

static void EIPRebootTask(void *pd)
{
  (void)pd;
  OSTimeDly(TICKS_PER_SECOND / 2);
  ForceReboot();
}

extern "C" void nb_schedule_reboot(void)
{
  static bool s_reboot_pending = false;

  if (s_reboot_pending) {
    return;
  }
  s_reboot_pending = true;
  OSSimpleTaskCreatewName(EIPRebootTask, MAIN_PRIO - 1, "EIPReboot");
}
