/*
 * machine.c - Interface to machine-specific implementations.
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

#include "vice.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "alarm.h"
#include "archdep.h"
#include "attach.h"
#include "autostart.h"
#include "cmdline.h"
#include "console.h"
#include "diskimage.h"
#include "drive.h"
#include "vice-event.h"
#include "fliplist.h"
#include "fsdevice.h"
#include "gfxoutput.h"
#include "interrupt.h"
#include "kbdbuf.h"
#include "keyboard.h"
#include "lib.h"
#include "log.h"
#include "machine-video.h"
#include "machine.h"
#include "maincpu.h"
#include "mem.h"
#include "monitor.h"
#include "monitor_network.h"
#include "network.h"
#include "printer.h"
#include "resources.h"
#include "romset.h"
#include "screenshot.h"
#include "sound.h"
#include "sysfile.h"
#include "tape.h"
#include "traps.h"
#include "vicetypes.h"
#include "uiapi.h"
#include "util.h"
#include "video.h"
#include "vsync.h"
#include "zfile.h"

#include "DebuggerDefs.h"

#ifdef HAS_JOYSTICK
#include "joy.h"
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

static int machine_init_was_called = 0;
static bool is_jammed = false;
static char *jam_reason = NULL;
static int jam_action = MACHINE_JAM_ACTION_DIALOG;
int machine_keymap_index;
static char *ExitScreenshotName = NULL;

void c64d_set_debug_mode(int newMode);

/* NOTE: this function is very similar to drive_jam - in case the behavior
         changes, change drive_jam too */
unsigned int machine_jam(const char *format, ...)
{
    va_list ap;
    ui_jam_action_t ret = JAM_NONE;

    /* c64d: pause debugger on JAM */
    c64d_set_debug_mode(DEBUGGER_MODE_PAUSED);

    /* always ignore subsequent JAMs. reset would clear the flag again, not
     * setting it when going to the monitor would just repeatedly pop up the
     * jam dialog (until reset)
     */
    if (is_jammed) {
        return JAM_NONE;
    }

    is_jammed = true;

    va_start(ap, format);
    if (jam_reason) {
        lib_free(jam_reason);
        jam_reason = NULL;
    }
    jam_reason = lib_mvsprintf(format, ap);
    va_end(ap);

    log_message(LOG_DEFAULT, "*** %s", jam_reason);

    vsync_suspend_speed_eval();
    sound_suspend();

    if (jam_action == MACHINE_JAM_ACTION_DIALOG) {
        if (monitor_is_remote()) {
            ret = monitor_network_ui_jam_dialog("%s", jam_reason);
        } else if (!console_mode) {
            ret = ui_jam_dialog("%s", jam_reason);
        }
    } else if (jam_action == MACHINE_JAM_ACTION_QUIT) {
        exit(EXIT_SUCCESS);
    } else {
        int actions[4] = {
            -1, UI_JAM_MONITOR, UI_JAM_RESET, UI_JAM_HARD_RESET
        };
        ret = actions[jam_action - 1];
    }

    switch (ret) {
        case UI_JAM_RESET:
            return JAM_RESET_CPU;
        case UI_JAM_HARD_RESET:
            return JAM_POWER_CYCLE;
        case UI_JAM_MONITOR:
            return JAM_MONITOR;
        default:
            break;
    }
    return JAM_NONE;
}

bool machine_is_jammed(void)
{
    return is_jammed;
}

char *machine_jam_reason(void)
{
    return jam_reason;
}

void machine_powerup(void)
{
    /* HACK: using 0 as the initial compare value allows us to skip the multiple
       calls to this function that happen at startup, due to the default values
       for resources being initialized. The actual first reset, which also
       triggers a powerup call, will happen at clock value 6, so this is safe */
    static CLOCK powerup_clk = 0;

    machine_specific_powerup();

    /* some functions we can omit, if the cpu did not run since last call */
    if (maincpu_clk != powerup_clk) {
        mem_powerup();
    }

    powerup_clk = maincpu_clk;
}

static void machine_trigger_reset_internal(const unsigned int mode)
{
    is_jammed = false;

    if (jam_reason) {
        lib_free(jam_reason);
        jam_reason = NULL;
    }

    switch (mode) {
        case MACHINE_RESET_MODE_POWER_CYCLE:
            machine_powerup();
        /* Fall through.  */
        case MACHINE_RESET_MODE_RESET_CPU:
            maincpu_trigger_reset();
            break;
    }
}

void machine_trigger_reset(const unsigned int mode)
{
    if (event_playback_active()) {
        return;
    }

    if (network_connected()) {
        network_event_record(EVENT_RESETCPU, (void *)&mode, sizeof(mode));
    } else {
        event_record(EVENT_RESETCPU, (void *)&mode, sizeof(mode));
        machine_trigger_reset_internal(mode);
    }
}

void machine_reset_event_playback(CLOCK offset, void *data)
{
    machine_trigger_reset_internal(((unsigned int*)data)[0]);
}

/* called via cpu_reset() */
/* CAUTION: this function is only called when the CPU core is "clocked" (ie the
   emulated machine is running). In particular that means that multiple calls
   to the machine_trigger_reset() function will not result in multiple calls
   to this function (if the cpu core is not running). */
/* NOTE: To make sure things work "as expected", really only deal with "reset"
   in the function below - anything related to "powerup" should go into
   machine_powerup() instead */

extern log_t maincpu_log;   /* FIXME: where should this live? */

static bool is_first_reset = true;

void machine_reset(void)
{
    log_message(maincpu_log, "RESET.");

    is_jammed = false;

    if (jam_reason) {
        lib_free(jam_reason);
        jam_reason = NULL;
    }

    /* Do machine-specific initialization.  */
    machine_specific_reset();

    autostart_reset();

    mem_initialize_memory();

    event_reset_ack();

    vsync_suspend_speed_eval();

    /* Handle the first machine reset */
    if (is_first_reset) {
        is_first_reset = false;
        /* extra power-up initialization */
        machine_powerup();
    }
}

void machine_maincpu_init(void)
{
    maincpu_init();
    maincpu_monitor_interface = lib_calloc(1, sizeof(monitor_interface_t));
}

void machine_early_init(void)
{
    maincpu_alarm_context = alarm_context_new("MainCPU");
}

int machine_init(void)
{
    machine_init_was_called = 1;

    machine_video_init();

    fsdevice_init();
    file_system_init();
    mem_initialize_memory();

    return machine_specific_init();
}

void machine_maincpu_shutdown(void)
{
    if (maincpu_alarm_context != NULL) {
        alarm_context_destroy(maincpu_alarm_context);
    }

    lib_free(maincpu_monitor_interface);
    maincpu_shutdown();

    if (jam_reason) {
        lib_free(jam_reason);
        jam_reason = NULL;
    }
}

static void screenshot_at_exit(void)
{
    struct video_canvas_s *canvas;

    if ((ExitScreenshotName == NULL) || (ExitScreenshotName[0] == 0)) {
        return;
    }
    /* FIXME: this always uses the first canvas, for x128/VDC we will need extra handling */
    canvas = machine_video_canvas_get(0);
    /* FIXME: perhaps select driver based on the extension of the given name. for now PNG is good enough :) */
    screenshot_save("PNG", ExitScreenshotName, canvas);
}

void machine_shutdown(void)
{
    if (!machine_init_was_called) {
        /* happens at the -help command line command*/
        return;
    }

    /*
     * Avoid SoundRecordDeviceName being written to vicerc when save-on-exit
     * is enabled. If recording is/was active vicerc will contain some setting
     * for this resource and display an error.
     */
    sound_stop_recording();

    screenshot_at_exit();
    screenshot_shutdown();

    file_system_detach_disk_shutdown();

    machine_specific_shutdown();

    autostart_shutdown();

#ifdef HAS_JOYSTICK
    joystick_close();
#endif

    sound_close();

    printer_shutdown();
    gfxoutput_shutdown();

    fliplist_shutdown();
    file_system_shutdown();
    fsdevice_shutdown();

    tape_shutdown();

    traps_shutdown();

    kbdbuf_shutdown();
    keyboard_shutdown();

    monitor_shutdown();

    console_close_all();

    cmdline_shutdown();

    resources_shutdown();

    drive_shutdown();

    machine_maincpu_shutdown();

    video_shutdown();

    if (!console_mode) {
        ui_shutdown();
    }

    sysfile_shutdown();

    log_close_all();

    event_shutdown();

    network_shutdown();

    autostart_resources_shutdown();
    sound_resources_shutdown();
    video_resources_shutdown();
    machine_resources_shutdown();
    machine_common_resources_shutdown();

    sysfile_resources_shutdown();
    zfile_shutdown();
    ui_resources_shutdown();
    log_resources_shutdown();
    fliplist_resources_shutdown();
    romset_resources_shutdown();
#ifdef HAVE_NETWORK
    monitor_network_resources_shutdown();
#endif
    archdep_shutdown();
}

/* --------------------------------------------------------- */
/* Resources & cmdline */

static int set_jam_action(int val, void *param)
{
    switch (val) {
        case MACHINE_JAM_ACTION_DIALOG:
        case MACHINE_JAM_ACTION_CONTINUE:
        case MACHINE_JAM_ACTION_MONITOR:
        case MACHINE_JAM_ACTION_RESET_CPU:
        case MACHINE_JAM_ACTION_POWER_CYCLE:
        case MACHINE_JAM_ACTION_QUIT:
            break;
        default:
            return -1;
    }

    jam_action = val;

    return 0;
}

static int set_exit_screenshot_name(const char *val, void *param)
{
    if (util_string_set(&ExitScreenshotName, val)) {
        return 0;
    }

    return 0;
}

static resource_string_t resources_string[] = {
    { "ExitScreenshotName", "", RES_EVENT_NO, NULL,
      &ExitScreenshotName, set_exit_screenshot_name, NULL },
    RESOURCE_STRING_LIST_END
};

static const resource_int_t resources_int[] = {
    { "JAMAction", MACHINE_JAM_ACTION_DIALOG, RES_EVENT_SAME, NULL,
      &jam_action, set_jam_action, NULL },
    RESOURCE_INT_LIST_END
};

int machine_common_resources_init(void)
{
    if (machine_class != VICE_MACHINE_VSID) {
        if (resources_register_string(resources_string) < 0) {
           return -1;
        }
    }
    return resources_register_int(resources_int);
}

void machine_common_resources_shutdown(void)
{
    lib_free(ExitScreenshotName);
}

static const cmdline_option_t cmdline_options[] = {
    { "-jamaction", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "JAMAction", NULL,
      "<Type>", "Set action on CPU JAM: (0: Ask, 1: continue, 2: Monitor, 3: Reset, 4: Hard Reset, 5: Quit Emulator)" },
    { "-exitscreenshot", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "ExitScreenshotName", NULL,
      "<Name>", "Set name of screenshot to save when emulator exits." },
    CMDLINE_LIST_END
};

static const cmdline_option_t cmdline_options_vsid[] = {
    { "-jamaction", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "JAMAction", NULL,
      "<Type>", "Set action on CPU JAM: (0: Ask, 1: continue, 2: Monitor, 3: Reset, 4: Hard Reset, 5: Quit Emulator)" },
    CMDLINE_LIST_END
};

int machine_common_cmdline_options_init(void)
{
    if (machine_class != VICE_MACHINE_VSID) {
        return cmdline_register_options(cmdline_options);
    } else {
        return cmdline_register_options(cmdline_options_vsid);
    }
}

