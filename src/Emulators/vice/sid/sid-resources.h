/*
 * sid-resources.h - SID resources.
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *  Marco van den Heuvel <blackystardust68@yahoo.com>
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

#ifndef VICE_SID_RESOURCES_H
#define VICE_SID_RESOURCES_H

/* Sampling methods (also defined in sid.h) */
#define SID_RESID_SAMPLING_FAST                 0
#define SID_RESID_SAMPLING_INTERPOLATION        1
#define SID_RESID_SAMPLING_RESAMPLING           2
#define SID_RESID_SAMPLING_FAST_RESAMPLING      3

int sid_resources_init(void);
int sid_common_resources_init(void);

/* c64d: non-static for direct engine/model setting */
int set_sid_engine(int set_engine, void *param);
int set_sid_model(int val, void *param);

int sid_set_sid_stereo_address(int val, void *param);
int sid_set_sid_triple_address(int val, void *param);
int sid_set_sid4_address(int val, void *param);
int sid_set_sid5_address(int val, void *param);
int sid_set_sid6_address(int val, void *param);
int sid_set_sid7_address(int val, void *param);
int sid_set_sid8_address(int val, void *param);

extern int sid_stereo;
extern int checking_sid_stereo;
extern unsigned int sid_stereo_address_start;
extern unsigned int sid_stereo_address_end;
extern unsigned int sid_triple_address_start;
extern unsigned int sid_triple_address_end;
extern unsigned int sid4_address_start;
extern unsigned int sid4_address_end;
extern unsigned int sid5_address_start;
extern unsigned int sid5_address_end;
extern unsigned int sid6_address_start;
extern unsigned int sid6_address_end;
extern unsigned int sid7_address_start;
extern unsigned int sid7_address_end;
extern unsigned int sid8_address_start;
extern unsigned int sid8_address_end;

/* c64d */
void c64d_sid_set_engine_model_direct(int engine, int model);
void c64d_sid_set_sampling_method(int samplingMethod);
void c64d_sid_set_emulate_filters(int emulateFilters);
void c64d_sid_set_passband(int passband);
void c64d_sid_set_filter_bias(int filterBias);
void c64d_sid_set_stereo(int stereoMode);
void c64d_sid_set_stereo_address(int sidAddress);
void c64d_sid_set_triple_address(int sidAddress);

#endif
