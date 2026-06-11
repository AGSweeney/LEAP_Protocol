/*
 * leapos_ich7_sata.h — ICH7 SATA0 legacy IDE prep for D945GSEJT CF on SATA.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAPOS_ICH7_SATA_H
#define LEAPOS_ICH7_SATA_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 0 when ICH7 SATA port 0 legacy mapping was programmed. */
int leapos_ich7_sata0_prep(void);

#ifdef __cplusplus
}
#endif

#endif /* LEAPOS_ICH7_SATA_H */
