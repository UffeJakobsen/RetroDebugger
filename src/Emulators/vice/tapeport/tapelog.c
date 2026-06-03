/*
 * tapelog.c - Generic tapeport diagnostic tool / logger emulation.
 *
 * Written by
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

/* tapelog:

This device is attached to the tape port and has a passthrough port
to which any other device can be attached. The device logs any
data that goes through.

TAPEPORT | TAPELOG
--------------------------------
  MOTOR  | MOTOR IN -> MOTOR OUT
  SENSE  | SENSE OUT <- SENSE IN
  WRITE  | WRITE IN -> WRITE OUT
  READ   | READ OUT <- READ IN
*/

#include "vice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmdline.h"
#include "lib.h"
#include "log.h"
#include "machine.h"
#include "maincpu.h"
#include "resources.h"
#include "snapshot.h"
#include "tapeport.h"
#include "util.h"

/* Device enabled */
static int tapelog_enabled = 0;

/* log to specified file (1) or generic logfile (0) */
static int tapelog_destination = 0;

/* log filename */
static char *tapelog_filename = NULL;

/* keep track of lines */
static uint8_t tapelog_motor_in = 2;
static uint8_t tapelog_motor_out = 2;
static uint8_t tapelog_sense_in = 2;
static uint8_t tapelog_sense_out = 2;
static uint8_t tapelog_write_in = 2;
static uint8_t tapelog_write_out = 2;
static unsigned int tapelog_read_in = 0;
static uint8_t tapelog_read_out = 2;

/* ------------------------------------------------------------------------- */

/* Some prototypes are needed */
static int tapelog_enable(int port, int val);
static void tapelog_set_motor(int port, int flag);
static void tapelog_toggle_write_bit(int port, int write_bit);
static void tapelog_set_sense_out(int port, int sense);
static void tapelog_set_read_out(int port, int val);
static int tapelog_write_snapshot(int port, struct snapshot_s *s, int write_image);
static int tapelog_read_snapshot(int port, struct snapshot_s *s);

/* TAPEPORT_DEVICE_TAPE_LOG was removed in VICE 3.10 along with tapelog.c itself.
   Since no device ID exists for tape log in 3.10 and the file is compiled as a
   legacy stub, register as TAPEPORT_DEVICE_NONE with a unique init path. */
#define TAPELOG_DEVICE_ID   TAPEPORT_DEVICE_NONE

static tapeport_device_t tapelog_device = {
    "Tape Log",                 /* device name */
    TAPEPORT_DEVICE_TYPE_NONE,  /* device type */
    VICE_MACHINE_ALL,           /* machine mask */
    TAPEPORT_PORT_ALL_MASK,     /* port mask */
    tapelog_enable,             /* enable function */
    NULL,                       /* NO powerup function */
    NULL,                       /* NO shutdown function */
    tapelog_set_motor,          /* set motor line */
    tapelog_toggle_write_bit,   /* toggle write bit */
    tapelog_set_sense_out,      /* set sense out */
    tapelog_set_read_out,       /* set read out */
    tapelog_write_snapshot,     /* snapshot write */
    tapelog_read_snapshot       /* snapshot read */
};

static log_t log_tapelog;

static FILE *tapelog_out = NULL;

/* ------------------------------------------------------------------------- */

static int enable_tapelog(void)
{
    if (tapelog_destination) {
        tapelog_out = fopen(tapelog_filename, "w+");
        if (tapelog_out == NULL) {
            return -1;
        }
        fprintf(tapelog_out, "\n-------------------------------------------------------------------------\n\n");
    } else {
        log_tapelog = log_open("Tape Log");
    }
    return 0;
}

static void disable_tapelog(void)
{
    if (tapelog_destination) {
        fclose(tapelog_out);
        tapelog_out = NULL;
    } else {
        log_close(log_tapelog);
    }
}

static void tapelog_initial_set(char *line, uint8_t val)
{
    if (tapelog_destination) {
        fprintf(tapelog_out, "Initial set of %s to %d at %X\n", line, val, (unsigned int)maincpu_clk);
    } else {
        log_message(log_tapelog, "Initial set of %s to %d at %X", line, val, (unsigned int)maincpu_clk);
    }
}

static void tapelog_transition(char *line, uint8_t val)
{
    if (tapelog_destination) {
        fprintf(tapelog_out, "%s: %d -> %d at %X\n", line, !val, val, (unsigned int)maincpu_clk);
    } else {
        log_message(log_tapelog, "%s: %d -> %d at %X", line, !val, val, (unsigned int)maincpu_clk);
    }
}

/* ------------------------------------------------------------------------- */

static int tapelog_enable(int port, int val)
{
    int newval = val ? 1 : 0;

    if (tapelog_enabled == newval) {
        return 0;
    }

    if (newval) {
        if (enable_tapelog() < 0) {
            return -1;
        }
    } else {
        disable_tapelog();
    }

    tapelog_enabled = newval;
    return 0;
}

static int set_tapelog_enabled(int value, void *param)
{
    return tapelog_enable(0, value);
}

static int set_tapelog_destination(int value, void *param)
{
    int val = value ? 1 : 0;

    if (tapelog_destination == val) {
        return 0;
    }

    if (tapelog_enabled) {
        disable_tapelog();
    }

    tapelog_destination = val;

    if (tapelog_enabled) {
        if (enable_tapelog() < 0) {
            return -1;
        }
    }

    return 0;
}

static int set_tapelog_filename(const char *name, void *param)
{
    if (tapelog_filename != NULL && name != NULL && strcmp(name, tapelog_filename) == 0) {
        return 0;
    }

    if (name != NULL && *name != '\0') {
        if (util_check_filename_access(name) < 0) {
            return -1;
        }
    }

    if (tapelog_enabled && tapelog_destination) {
        disable_tapelog();
        util_string_set(&tapelog_filename, name);
        if (enable_tapelog() < 0) {
            return -1;
        }
    } else {
        util_string_set(&tapelog_filename, name);
    }

    return 0;
}

/* ------------------------------------------------------------------------- */

static const resource_int_t resources_int[] = {
    { "TapeLog", 0, RES_EVENT_STRICT, (resource_value_t)0,
      &tapelog_enabled, set_tapelog_enabled, NULL },
    { "TapeLogDestination", 0, RES_EVENT_STRICT, (resource_value_t)0,
      &tapelog_destination, set_tapelog_destination, NULL },
    RESOURCE_INT_LIST_END
};

static const resource_string_t resources_string[] = {
    { "TapeLogfilename", "", RES_EVENT_NO, NULL,
      &tapelog_filename, set_tapelog_filename, NULL },
    RESOURCE_STRING_LIST_END
};

int tapelog_resources_init(void)
{
    if (resources_register_string(resources_string) < 0) {
        return -1;
    }

    return resources_register_int(resources_int);
}


void tapelog_resources_shutdown(void)
{
    if (tapelog_filename != NULL) {
        lib_free(tapelog_filename);
    }
}


/* ------------------------------------------------------------------------- */

static const cmdline_option_t cmdline_options[] =
{
    { "-tapelog", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "TapeLog", (resource_value_t)1,
      NULL, "Enable the tape log device" },
    { "+tapelog", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "TapeLog", (resource_value_t)0,
      NULL, "Disable the tape log device" },
    { "-tapelogtofile", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "TapeLogDestination", (resource_value_t)1,
      NULL, "Enable logging to a file" },
    { "-tapelogtolog", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "TapeLogDestination", (resource_value_t)0,
      NULL, "Enable logging to the emulator log file" },
    { "-tapelogimage", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "TapeLogfilename", NULL,
      "<Name>", "Specify tape log file name" },
    CMDLINE_LIST_END
};

int tapelog_cmdline_options_init(void)
{
    return cmdline_register_options(cmdline_options);
}

/* ---------------------------------------------------------------------*/

static void tapelog_set_motor(int port, int flag)
{
    uint8_t val = flag ? 1 : 0;

    if (tapelog_motor_out == val) {
        return;
    }

    if (tapelog_motor_out == 2) {
        tapelog_initial_set("motor", val);
    } else {
        tapelog_transition("motor", val);
    }

    tapelog_motor_out = val;
}

static void tapelog_toggle_write_bit(int port, int write_bit)
{
    uint8_t val = write_bit ? 1 : 0;

    if (tapelog_write_out == val) {
        return;
    }

    if (tapelog_write_out == 2) {
        tapelog_initial_set("write", val);
    } else {
        tapelog_transition("write", val);
    }

    tapelog_write_out = val;
}

static void tapelog_set_sense_out(int port, int sense)
{
    uint8_t val = sense ? 1 : 0;

    if (tapelog_sense_out == val) {
        return;
    }

    if (tapelog_sense_out == 2) {
        tapelog_initial_set("sense out", val);
    } else {
        tapelog_transition("sense out", val);
    }

    tapelog_sense_out = val;
}

static void tapelog_set_read_out(int port, int value)
{
    uint8_t val = value ? 1 : 0;

    if (tapelog_read_out == val) {
        return;
    }

    if (tapelog_read_out == 2) {
        tapelog_initial_set("read out", val);
    } else {
        tapelog_transition("read out", val);
    }

    tapelog_read_out = val;
}

/* ------------------------------------------------------------------------- */

/* TP_TAPELOG snapshot module format:

   type  | name      | version | description
   -----------------------------------------
   uint8_t  | motor out | 0.0+    | motor out state
   uint8_t  | motor in  | 0.1     | motor in state
   uint8_t  | sense in  | 0.0+    | sense in state
   uint8_t  | sense out | 0.0+    | sense out state
   uint8_t  | write out | 0.0+    | write out state
   uint8_t  | write in  | 0.1     | write in state
   uint8_t  | read out  | 0.1     | read out state
   uint32_t | read in   | 0.0+    | read in state
 */

static const char snap_module_name[] = "TP_TAPELOG";
#define SNAP_MAJOR   0
#define SNAP_MINOR   1

static int tapelog_write_snapshot(int port, struct snapshot_s *s, int write_image)
{
    snapshot_module_t *m;

    m = snapshot_module_create(s, snap_module_name, SNAP_MAJOR, SNAP_MINOR);

    if (m == NULL) {
        return -1;
    }

    if (0
        || SMW_B(m, tapelog_motor_out) < 0
        || SMW_B(m, tapelog_motor_in) < 0
        || SMW_B(m, tapelog_sense_in) < 0
        || SMW_B(m, tapelog_sense_out) < 0
        || SMW_B(m, tapelog_write_out) < 0
        || SMW_B(m, tapelog_write_in) < 0
        || SMW_B(m, tapelog_read_out) < 0
        || SMW_DW(m, (uint32_t)tapelog_read_in) < 0) {
        snapshot_module_close(m);
        return -1;
    }
    return snapshot_module_close(m);
}

static int tapelog_read_snapshot(int port, struct snapshot_s *s)
{
    uint8_t major_version, minor_version;
    snapshot_module_t *m;

    /* enable device */
    tapelog_enable(0, 1);

    m = snapshot_module_open(s, snap_module_name, &major_version, &minor_version);

    if (m == NULL) {
        return -1;
    }

    /* Do not accept versions higher than current */
    if (major_version > SNAP_MAJOR || minor_version > SNAP_MINOR) {
        snapshot_set_error(SNAPSHOT_MODULE_HIGHER_VERSION);
        goto fail;
    }

    if (SMR_B(m, &tapelog_motor_out) < 0) {
        goto fail;
    }

    /* new in 0.1 */
    if (!snapshot_version_is_smaller(major_version, minor_version, 0, 1)) {
        if (SMR_B(m, &tapelog_motor_in) < 0) {
            goto fail;
        }
    } else {
        tapelog_motor_in = 2;
    }

    if (0
        || SMR_B(m, &tapelog_sense_in) < 0
        || SMR_B(m, &tapelog_sense_out) < 0
        || SMR_B(m, &tapelog_write_out) < 0) {
        goto fail;
    }

    /* new in 0.1 */
    if (!snapshot_version_is_smaller(major_version, minor_version, 0, 1)) {
        if (0
            || SMR_B(m, &tapelog_write_in) < 0
            || SMR_B(m, &tapelog_read_out) < 0) {
            goto fail;
        }
    } else {
        tapelog_write_in = 2;
        tapelog_read_out = 2;
    }

    if (SMR_DW_UINT(m, &tapelog_read_in) < 0) {
        goto fail;
    }

    return snapshot_module_close(m);

fail:
    snapshot_module_close(m);
    return -1;
}
