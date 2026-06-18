/*******************************************************************************
 * Deferred gateway reboot for CIP Identity Reset (C/C++ boundary).
 ******************************************************************************/

#ifndef NB_REBOOT_H_
#define NB_REBOOT_H_

#ifdef __cplusplus
extern "C" {
#endif

void nb_schedule_reboot(void);

#ifdef __cplusplus
}
#endif

#endif /* NB_REBOOT_H_ */
