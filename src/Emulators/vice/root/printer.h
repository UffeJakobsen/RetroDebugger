/*
 * printer.h - Common external printer interface.
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

#ifndef VICE_PRINTER_H
#define VICE_PRINTER_H

#include "vicetypes.h"

/* Generic interface.  */
int printer_resources_init(void);
int printer_userport_resources_init(void);
void printer_resources_shutdown(void);
int printer_cmdline_options_init(void);
int printer_userport_cmdline_options_init(void);
void printer_init(void);
void printer_reset(void);
void printer_formfeed(unsigned int unit);
void printer_shutdown(void);

/* Serial interface.  */
#define PRINTER_IEC_NUM 2

#define PRINTER_DEVICE_NONE 0
#define PRINTER_DEVICE_FS   1
#define PRINTER_DEVICE_REAL 2

int printer_serial_init_resources(void);
int printer_serial_init_cmdline_options(void);
void printer_serial_init(void);
int printer_serial_close(unsigned int unit);
int printer_serial_late_init(void);
void printer_serial_shutdown(void);

/* Userport interface.  */
int printer_userport_init_resources(void);
int printer_userport_init_cmdline_options(void);

#endif
