/*
 * cmdhd_stubs.c - Stub implementations for CMD HD functions
 *
 * The full CMD HD implementation (cmdhd.c, ~1628 lines) is deferred.
 * These stubs provide enough to link ramlink.c which calls cmdbus functions.
 */

#include "vice.h"
#include "vicetypes.h"
#include "drivetypes.h"
#include "cmdhd.h"

cmdbus_t cmdbus;

void cmdbus_init(void)
{
    memset(&cmdbus, 0, sizeof(cmdbus));
}

void cmdbus_update(void)
{
    cmdbus.bus = 0xff;
    cmdbus.data = 0xff;
}

void cmdbus_patn_changed(int new_val, int old_val)
{
    /* stub — full CMD HD parallel bus implementation deferred */
}
