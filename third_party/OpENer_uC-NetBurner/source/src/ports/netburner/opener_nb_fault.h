#ifndef OPENER_NB_FAULT_H_
#define OPENER_NB_FAULT_H_

/*******************************************************************************
 * OpENer_uC-NetBurner — ACD fault hook
 *
 * Called from opener_nb_acd.cpp when a duplicate address forces interface IP to
 * zero. Closes all I/O connections so scanners see a clean fault state.
 ******************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Close all CIP connections after ACD fault (duplicate IPv4). */
void OpenerNbAcdOnFault(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENER_NB_FAULT_H_ */
