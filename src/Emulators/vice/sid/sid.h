
/*! \file sid/sid.h */

/*
 * sid.h - MOS6581 (SID) emulation, hooks to actual implementation.
 *
 * Written by
 *  Dag Lem <resid@nimrod.no>
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

#ifndef VICE_SID_ENGINE_H
#define VICE_SID_ENGINE_H

#include "vicetypes.h"
#include "sound.h"

#define SID_SETTINGS_DIALOG

struct sound_s;
struct sid_snapshot_state_s;

#define SID_ENGINE_FASTSID        0
#define SID_ENGINE_RESID          1
#define SID_ENGINE_CATWEASELMKIII 2
#define SID_ENGINE_HARDSID        3
#define SID_ENGINE_PARSID         4
#define SID_ENGINE_SSI2001        5
#define SID_ENGINE_USBSID         7
#define SID_ENGINE_RESID_FP       8
#define SID_ENGINE_DEFAULT       99

/* Maximum number of supported SIDs for each engine (from VICE 3.10) */
#define SID_ENGINE_FASTSID_NUM_SIDS         4
#define SID_ENGINE_RESID_NUM_SIDS           4
#define SID_ENGINE_CATWEASELMKIII_NUM_SIDS  2
#define SID_ENGINE_HARDSID_NUM_SIDS         2
#define SID_ENGINE_PARSID_NUM_SIDS          1
#define SID_ENGINE_USBSID_NUM_SIDS         4

/* Sampling methods (from VICE 3.10) */
#define SID_RESID_SAMPLING_FAST             0
#define SID_RESID_SAMPLING_INTERPOLATION    1
#define SID_RESID_SAMPLING_RESAMPLING       2
#define SID_RESID_SAMPLING_FAST_RESAMPLING  3

#define SID_MODEL_6581           0
#define SID_MODEL_8580           1
#define SID_MODEL_8580D          2
#define SID_MODEL_6581R4         3
#define SID_MODEL_DTVSID         4
/* c64d: reSID-FP specific chip models */
#define SID_MODEL_6581R3_4885    8
#define SID_MODEL_6581R3_0486S   9
#define SID_MODEL_6581R3_3984   10
#define SID_MODEL_6581R4AR_3789 11
#define SID_MODEL_6581R3_4485   12
#define SID_MODEL_6581R4_1986S  13
#define SID_MODEL_8580R5_3691   16
#define SID_MODEL_8580R5_3691D  17
#define SID_MODEL_8580R5_1489   18
#define SID_MODEL_8580R5_1489D  19
#define SID_MODEL_DEFAULT       99

/* these definitions are the only valid combinations of
   software SID engines and model, and are used in the
   UI and command line code. */
#define SID_FASTSID_6581          ((SID_ENGINE_FASTSID << 8) | SID_MODEL_6581)
#define SID_FASTSID_8580          ((SID_ENGINE_FASTSID << 8) | SID_MODEL_8580)
#define SID_RESID_6581            ((SID_ENGINE_RESID << 8) | SID_MODEL_6581)
#define SID_RESID_8580            ((SID_ENGINE_RESID << 8) | SID_MODEL_8580)
#define SID_RESID_8580D           ((SID_ENGINE_RESID << 8) | SID_MODEL_8580D)
#define SID_RESID_DTVSID          ((SID_ENGINE_RESID << 8) | SID_MODEL_DTVSID)
#define SID_CATWEASELMKIII        (SID_ENGINE_CATWEASELMKIII << 8)
#define SID_HARDSID               (SID_ENGINE_HARDSID << 8)
#define SID_PARSID                (SID_ENGINE_PARSID << 8)
#define SID_SSI2001               (SID_ENGINE_SSI2001 << 8)
#define SID_USBSID               (SID_ENGINE_USBSID << 8)
/* c64d: reSID-FP engine+model combinations */
#define SID_RESIDFP_6581R3_4885   ((SID_ENGINE_RESID_FP << 8) | SID_MODEL_6581R3_4885)
#define SID_RESIDFP_6581R3_0486S  ((SID_ENGINE_RESID_FP << 8) | SID_MODEL_6581R3_0486S)
#define SID_RESIDFP_6581R3_3984   ((SID_ENGINE_RESID_FP << 8) | SID_MODEL_6581R3_3984)
#define SID_RESIDFP_6581R4AR_3789 ((SID_ENGINE_RESID_FP << 8) | SID_MODEL_6581R4AR_3789)
#define SID_RESIDFP_6581R3_4485   ((SID_ENGINE_RESID_FP << 8) | SID_MODEL_6581R3_4485)
#define SID_RESIDFP_6581R4_1986S  ((SID_ENGINE_RESID_FP << 8) | SID_MODEL_6581R4_1986S)
#define SID_RESIDFP_8580R5_3691   ((SID_ENGINE_RESID_FP << 8) | SID_MODEL_8580R5_3691)
#define SID_RESIDFP_8580R5_3691D  ((SID_ENGINE_RESID_FP << 8) | SID_MODEL_8580R5_3691D)
#define SID_RESIDFP_8580R5_1489   ((SID_ENGINE_RESID_FP << 8) | SID_MODEL_8580R5_1489)
#define SID_RESIDFP_8580R5_1489D  ((SID_ENGINE_RESID_FP << 8) | SID_MODEL_8580R5_1489D)

/* SID type identifiers (from VICE 3.10) */
#define SIDTYPE_SID       0
#define SIDTYPE_SIDDTV    1
#define SIDTYPE_SIDCART   2

/* Machine max SIDs (from VICE 3.10) */
#define SID_MACHINE_MAX_SID_C64     8
#define SID_MACHINE_MAX_SID_C64DTV  1
#define SID_MACHINE_MAX_SID_C128    8
#define SID_MACHINE_MAX_SID_VIC20   1
#define SID_MACHINE_MAX_SID_PLUS4   1
#define SID_MACHINE_MAX_SID_CBM5x0  1
#define SID_MACHINE_MAX_SID_CBM6x0  0
#define SID_MACHINE_MAX_SID_PET     1
#define SID_MACHINE_MAX_SID_VSID    3

/* ReSID filter parameters (from VICE 3.10) */
#define RESID_6581_PASSBAND_MIN             0
#define RESID_6581_PASSBAND_MAX             90
#define RESID_6581_PASSBAND_ONE             1
#define RESID_6581_PASSBAND_DEFAULT         90

#define RESID_6581_FILTER_GAIN_MIN          90
#define RESID_6581_FILTER_GAIN_MAX          100
#define RESID_6581_FILTER_GAIN_ONE          1
#define RESID_6581_FILTER_GAIN_DEFAULT      97

#define RESID_6581_FILTER_BIAS_MIN          -5000
#define RESID_6581_FILTER_BIAS_MAX          5000
#define RESID_6581_FILTER_BIAS_ONE          1000
#define RESID_6581_FILTER_BIAS_DEFAULT      500

#define RESID_8580_PASSBAND_MIN             0
#define RESID_8580_PASSBAND_MAX             90
#define RESID_8580_PASSBAND_ONE             1
#define RESID_8580_PASSBAND_DEFAULT         90

#define RESID_8580_FILTER_GAIN_MIN          90
#define RESID_8580_FILTER_GAIN_MAX          100
#define RESID_8580_FILTER_GAIN_ONE          1
#define RESID_8580_FILTER_GAIN_DEFAULT      97

#define RESID_8580_FILTER_BIAS_MIN          -5000
#define RESID_8580_FILTER_BIAS_MAX          5000
#define RESID_8580_FILTER_BIAS_ONE          1000
#define RESID_8580_FILTER_BIAS_DEFAULT      0

void machine_sid2_enable(int val);

uint8_t sid_read(uint16_t address);
uint8_t sid_peek(uint16_t address);
uint8_t sid2_read(uint16_t address);
uint8_t sid3_read(uint16_t address);
uint8_t sid4_read(uint16_t address);
uint8_t sid5_read(uint16_t address);
uint8_t sid6_read(uint16_t address);
uint8_t sid7_read(uint16_t address);
uint8_t sid8_read(uint16_t address);

uint8_t sid2_peek(uint16_t address);
uint8_t sid3_peek(uint16_t address);
uint8_t sid4_peek(uint16_t address);
uint8_t sid5_peek(uint16_t address);
uint8_t sid6_peek(uint16_t address);
uint8_t sid7_peek(uint16_t address);
uint8_t sid8_peek(uint16_t address);

void sid_store(uint16_t address, uint8_t byte);
void sid2_store(uint16_t address, uint8_t byte);
void sid3_store(uint16_t address, uint8_t byte);
void sid4_store(uint16_t address, uint8_t byte);
void sid5_store(uint16_t address, uint8_t byte);
void sid6_store(uint16_t address, uint8_t byte);
void sid7_store(uint16_t address, uint8_t byte);
void sid8_store(uint16_t address, uint8_t byte);

int sid_dump(void);
int sid2_dump(void);
int sid3_dump(void);
int sid4_dump(void);
int sid5_dump(void);
int sid6_dump(void);
int sid7_dump(void);
int sid8_dump(void);

void sid_reset(void);

void sid_set_machine_parameter(long clock_rate);
uint8_t *sid_get_siddata(unsigned int channel);
int sid_engine_set(int engine);
void sid_state_read(unsigned int channel,
                           struct sid_snapshot_state_s *sid_state);
void sid_state_write(unsigned int channel,
                            struct sid_snapshot_state_s *sid_state);

struct sid_engine_s {
    struct sound_s *(*open)(uint8_t *sidstate, int chipNo); /* c64d: chipNo kept for channel routing */
    int (*init)(struct sound_s *psid, int speed, int cycles_per_sec, int factor);
    void (*close)(struct sound_s *psid);
    uint8_t (*read)(struct sound_s *psid, uint16_t addr);
    void (*store)(struct sound_s *psid, uint16_t addr, uint8_t val);
    void (*reset)(struct sound_s *psid, CLOCK cpu_clk);
    int (*calculate_samples)(struct sound_s *psid, int16_t *pbuf, int nr,
                             int interleave, int *delta_t);
    char *(*dump_state)(struct sound_s *psid);
    void (*state_read)(struct sound_s *psid,
                       struct sid_snapshot_state_s *sid_state);
    void (*state_write)(struct sound_s *psid,
                        struct sid_snapshot_state_s *sid_state);

    // c64d
    void (*set_voice_mask)(struct sound_s *psid, uint8_t voiceMask);
};
typedef struct sid_engine_s sid_engine_t;

struct sid_engine_model_s {
    char *name;
    int value;
};
typedef struct sid_engine_model_s sid_engine_model_t;

sound_t *sid_sound_machine_open(int chipno);
int sid_sound_machine_init_vbr(sound_t *psid, int speed, int cycles_per_sec, int factor);
int sid_sound_machine_init(sound_t *psid, int speed, int cycles_per_sec);
void sid_sound_machine_close(sound_t *psid);
uint8_t sid_sound_machine_read(sound_t *psid, uint16_t addr);
void sid_sound_machine_store(sound_t *psid, uint16_t addr, uint8_t byte);
void sid_sound_machine_reset(sound_t *psid, CLOCK cpu_clk);
int sid_sound_machine_calculate_samples(sound_t **psid, int16_t *pbuf, int nr, int sound_output_channels, int sound_chip_channels, int *delta_t);
char *sid_sound_machine_dump_state(sound_t *psid);
int sid_sound_machine_cycle_based(void);
int sid_sound_machine_channels(void);
void sid_sound_machine_enable(int enable);
sid_engine_model_t **sid_get_engine_model_list(void);
int sid_set_engine_model(int engine, int model);
void sid_sound_chip_init(void);

void sid_set_enable(int value);
int sid_sound_machine_set_engine_hooks(void);

int sid_engine_get_max_sids(int engine);
int sid_machine_get_max_sids(void);
int sid_machine_engine_get_max_sids(int engine);
int sid_machine_can_have_multiple_sids(void);

// c64d
void sid_sound_machine_set_voice_mask(sound_t *psid, uint8_t voiceMask);

#endif
