/*
 * fakesid.c - Minimal fallback SID engine that produces silence.
 *
 * From VICE 3.10 — ensures sound subsystem doesn't crash when no real
 * SID engine (FASTSID/RESID/hardware) is available.
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 */

#include "vice.h"

#include <string.h>

#include "lib.h"
#include "sid.h"
#include "sid-snapshot.h"
#include "vicetypes.h"

struct sound_s {
    uint8_t d[32];
};

static sound_t *fakesid_open(uint8_t *sidstate, int chipNo)
{
    sound_t *psid;
    (void)chipNo;

    psid = lib_calloc(1, sizeof(sound_t));
    if (sidstate) {
        memcpy(psid->d, sidstate, 32);
    }
    return psid;
}

static int fakesid_init(sound_t *psid, int speed, int cycles_per_sec, int factor)
{
    (void)psid; (void)speed; (void)cycles_per_sec; (void)factor;
    return 1;
}

static void fakesid_close(sound_t *psid)
{
    lib_free(psid);
}

static uint8_t fakesid_read(sound_t *psid, uint16_t addr)
{
    (void)psid; (void)addr;
    return 0;
}

static void fakesid_store(sound_t *psid, uint16_t addr, uint8_t val)
{
    if (psid && addr < 32) {
        psid->d[addr] = val;
    }
}

static void fakesid_reset(sound_t *psid, CLOCK cpu_clk)
{
    (void)psid; (void)cpu_clk;
}

static int fakesid_calculate_samples(sound_t *psid, int16_t *pbuf, int nr,
                                     int interleave, int *delta_t)
{
    (void)psid; (void)interleave;
    memset(pbuf, 0, nr * sizeof(int16_t));
    *delta_t = 0;
    return nr;
}

static char *fakesid_dump_state(sound_t *psid)
{
    (void)psid;
    return lib_strdup("");
}

static void fakesid_state_read(sound_t *psid, sid_snapshot_state_t *st)
{
    (void)psid; (void)st;
}

static void fakesid_state_write(sound_t *psid, sid_snapshot_state_t *st)
{
    (void)psid; (void)st;
}

static void fakesid_set_voice_mask(sound_t *psid, uint8_t mask)
{
    (void)psid; (void)mask;
}

sid_engine_t fakesid_hooks = {
    fakesid_open,
    fakesid_init,
    fakesid_close,
    fakesid_read,
    fakesid_store,
    fakesid_reset,
    fakesid_calculate_samples,
    fakesid_dump_state,
    fakesid_state_read,
    fakesid_state_write,
    fakesid_set_voice_mask
};
