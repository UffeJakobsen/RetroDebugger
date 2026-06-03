/*
 * c64-resources.h
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#ifndef VICE_C64_RESOURCES_H
#define VICE_C64_RESOURCES_H

int c64_resources_init(void);
void c64_resources_shutdown(void);

void c64_resources_update_cia_models(int model);

extern int acia_de_enabled;

/* Kernal revision constants — canonical definitions in c64rom.h.
   Duplicated here for backward compat with callers that only include this header. */
#define C64_KERNAL_UNKNOWN -1

#define C64_KERNAL_JAP      0       /* 906145-02 */
#define C64_KERNAL_REV1     1       /* 901227-01 */
#define C64_KERNAL_REV2     2       /* 901227-02 */
#define C64_KERNAL_REV3     3       /* 901227-03 */
#define C64_KERNAL_GS64     39      /* 390852-01 */
#define C64_KERNAL_SX64     67      /* 251104-04 */
#define C64_KERNAL_4064     100     /* 901246-01 */
#define C64_KERNAL_REV3SWE  13      /* FIXME */
#define C64_KERNAL_NONE     -2      /* MAX machine, no kernal */
#define C64_KERNAL_MAX      C64_KERNAL_NONE

extern int kernal_revision;

extern int cia1_model;
extern int cia2_model;

#endif
