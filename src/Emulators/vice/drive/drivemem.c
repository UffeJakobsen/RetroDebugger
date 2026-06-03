/*
 * drivemem.c - Drive memory handling.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ciad.h"
#include "drive.h"
#include "drivemem.h"
#include "driverom.h"
#include "drivetypes.h"
#include "ds1216e.h"
#include "log.h"
#include "machine-drive.h"
#include "mem.h"
#include "monitor.h"
#include "riotd.h"
#include "tpid.h"
#include "vicetypes.h"
#include "via1d1541.h"
#include "via4000.h"
#include "viad.h"

#include "ViceWrapper.h"


static drive_read_func_t *read_tab_watch[0x101];
static drive_store_func_t *store_tab_watch[0x101];

/* ------------------------------------------------------------------------- */
/* Common memory access.  */

static uint8_t drive_read_free(drive_context_t *drv, uint16_t address)
{
	c64d_mark_drive1541_cell_read(address);

    return drv->cpu->cpu_last_data;
}

static void drive_store_free(drive_context_t *drv, uint16_t address, uint8_t value)
{
	c64d_mark_drive1541_cell_write(address, value);

    drv->cpu->cpu_last_data = value;
    return;
}

uint8_t drive_peek_free(drive_context_t *drv, uint16_t address)
{
    return drv->cpu->cpu_last_data;
}

/* ------------------------------------------------------------------------- */
/* Watchpoint memory access.  */

static uint8_t drive_zero_read_watch(drive_context_t *drv, uint16_t addr)
{
    addr &= 0xff;
    monitor_watch_push_load_addr(addr, drv->cpu->monspace);
    return drv->cpu->cpu_last_data = drv->cpud->read_tab[0][0](drv, addr);
}

static void drive_zero_store_watch(drive_context_t *drv, uint16_t addr, uint8_t value)
{
    addr &= 0xff;
    drv->cpu->cpu_last_data = value;
    monitor_watch_push_store_addr(addr, drv->cpu->monspace);
    drv->cpud->store_tab[0][0](drv, addr, value);
}

static uint8_t drive_read_watch(drive_context_t *drv, uint16_t address)
{
    monitor_watch_push_load_addr(address, drv->cpu->monspace);
    return drv->cpu->cpu_last_data = drv->cpud->read_tab[0][address >> 8](drv, address);
}

static void drive_store_watch(drive_context_t *drv, uint16_t address, uint8_t value)
{
    drv->cpu->cpu_last_data = value;
    monitor_watch_push_store_addr(address, drv->cpu->monspace);
    drv->cpud->store_tab[0][address >> 8](drv, address, value);
}

void drivemem_toggle_watchpoints(int flag, void *context)
{
    drive_context_t *drv = (drive_context_t *)context;

    if (flag) {
        drv->cpud->read_func_ptr = read_tab_watch;
        drv->cpud->store_func_ptr = store_tab_watch;
    } else {
        drv->cpud->read_func_ptr = drv->cpud->read_tab[0];
        drv->cpud->store_func_ptr = drv->cpud->store_tab[0];
    }
}

/* ------------------------------------------------------------------------- */

void drivemem_set_func(drivecpud_context_t *cpud,
                       unsigned int start, unsigned int stop,
                       drive_read_func_t *read_func,
                       drive_store_func_t *store_func,
                       drive_peek_func_t *peek_func,
                       uint8_t *base, uint32_t limit)
{
    unsigned int i;

    if (read_func != NULL) {
        for (i = start; i < stop; i++) {
            cpud->read_tab[0][i] = read_func;
        }
        /* if no peek function is provided, use the read function instead */
        if (peek_func == NULL) {
            peek_func = read_func;
        }
    }
    if (store_func != NULL) {
        for (i = start; i < stop; i++) {
            cpud->store_tab[0][i] = store_func;
        }
    }
    if (peek_func != NULL) {
        for (i = start; i < stop; i++) {
            cpud->peek_tab[0][i] = peek_func;
        }
    }
    for (i = start; i < stop; i++) {
        cpud->read_base_tab[0][i] = base ? (base - (start << 8)) : NULL;
        cpud->read_limit_tab[0][i] = limit;
    }
}

/* ------------------------------------------------------------------------- */
/* This is the external interface for banked memory access.  */

uint8_t drivemem_bank_read(int bank, uint16_t addr, void *context)
{
    drive_context_t *drv = (drive_context_t *)context;

    return drv->cpud->read_func_ptr[addr >> 8](drv, addr);
}

uint8_t drivemem_bank_peek(int bank, uint16_t addr, void *context)
{
    drive_context_t *drv = (drive_context_t *)context;

    return drv->cpud->peek_func_ptr[addr >> 8](drv, addr);
}

void drivemem_bank_store(int bank, uint16_t addr, uint8_t value, void *context)
{
    drive_context_t *drv = (drive_context_t *)context;

    drv->cpud->store_func_ptr[addr >> 8](drv, addr, value);
}

/* ------------------------------------------------------------------------- */

/* Sync fields migrated from drive_t to drive_context_s (VICE 3.10 transition).
   Both structs have these fields during the transition period. Init/resource
   code sets them on drive_t; memory access functions read from context. */
static void drive_sync_fields_to_context(drive_context_t *drv)
{
    drive_t *drive = drv->drives[0];

    drv->enable = drive->enable;
    drv->type = drive->type;
    drv->clock_frequency = drive->clock_frequency;
    drv->idling_method = drive->idling_method;
    drv->ds1216 = drive->ds1216;
    drv->rtc_save = drive->rtc_save;
    drv->drive_ram2_enabled = drive->drive_ram2_enabled;
    drv->drive_ram4_enabled = drive->drive_ram4_enabled;
    drv->drive_ram6_enabled = drive->drive_ram6_enabled;
    drv->drive_ram8_enabled = drive->drive_ram8_enabled;
    drv->drive_rama_enabled = drive->drive_rama_enabled;
    drv->parallel_cable = drive->parallel_cable;
    drv->profdos = drive->profdos;
    drv->supercard = drive->supercard;
    drv->stardos = drive->stardos;

    memcpy(drv->rom, drive->rom, DRIVE_ROM_SIZE);
    memcpy(drv->trap_rom, drive->trap_rom, DRIVE_ROM_SIZE);
    memcpy(drv->drive_ram, drive->drive_ram, DRIVE_RAM_SIZE);
}

void drivemem_init(drive_context_t *drv, unsigned int type)
{
    int i;

    /* Sync migrated fields from drive_t before setting up memory map */
    drive_sync_fields_to_context(drv);

    /* setup watchpoint tables */
    if (!read_tab_watch[0]) {
        read_tab_watch[0] = drive_zero_read_watch;
        store_tab_watch[0] = drive_zero_store_watch;
        for (i = 1; i < 0x101; i++) {
            read_tab_watch[i] = drive_read_watch;
            store_tab_watch[i] = drive_store_watch;
        }
    }

    drivemem_set_func(drv->cpud, 0x00, 0x101, drive_read_free, drive_store_free, drive_peek_free, NULL, 0);

    machine_drive_mem_init(drv, type);

    drv->cpud->read_tab[0][0x100] = drv->cpud->read_tab[0][0];
    drv->cpud->store_tab[0][0x100] = drv->cpud->store_tab[0][0];
    drv->cpud->peek_tab[0][0x100] = drv->cpud->peek_tab[0][0];

    drv->cpud->read_func_ptr = drv->cpud->read_tab[0];
    drv->cpud->store_func_ptr = drv->cpud->store_tab[0];
    drv->cpud->peek_func_ptr = drv->cpud->peek_tab[0];

    drv->cpud->read_base_tab_ptr = drv->cpud->read_base_tab[0];
    drv->cpud->read_limit_tab_ptr = drv->cpud->read_limit_tab[0];
}

mem_ioreg_list_t *drivemem_ioreg_list_get(void *context)
{
    unsigned int type;
    mem_ioreg_list_t *drivemem_ioreg_list = NULL;

    type = ((drive_context_t *)context)->type;

    switch (type) {
        case DRIVE_TYPE_1540:
        case DRIVE_TYPE_1541:
        case DRIVE_TYPE_1541II:
        case DRIVE_TYPE_2031:
            mon_ioreg_add_list(&drivemem_ioreg_list, "VIA1", 0x1800, 0x180f, via1d1541_dump, context, 0);
            mon_ioreg_add_list(&drivemem_ioreg_list, "VIA2", 0x1c00, 0x1c0f, via2d_dump, context, 0);
            break;
        case DRIVE_TYPE_1551:
            mon_ioreg_add_list(&drivemem_ioreg_list, "TPI", 0x4000, 0x4007, tpid_dump, context, 0);
            break;
        case DRIVE_TYPE_1570:
        case DRIVE_TYPE_1571:
        case DRIVE_TYPE_1571CR:
            mon_ioreg_add_list(&drivemem_ioreg_list, "VIA1", 0x1800, 0x180f, via1d1541_dump, context, 0);
            mon_ioreg_add_list(&drivemem_ioreg_list, "VIA2", 0x1c00, 0x1c0f, via2d_dump, context, 0);
            mon_ioreg_add_list(&drivemem_ioreg_list, "WD1770", 0x2000, 0x2003, NULL, context, 0); /* FIXME: register dump function */
            mon_ioreg_add_list(&drivemem_ioreg_list, "CIA", 0x4000, 0x400f, cia1571_dump, context, 0);
            break;
        case DRIVE_TYPE_1581:
            mon_ioreg_add_list(&drivemem_ioreg_list, "CIA", 0x4000, 0x400f, cia1581_dump, context, 0);
            mon_ioreg_add_list(&drivemem_ioreg_list, "WD1770", 0x6000, 0x6003, NULL, context, 0); /* FIXME: register dump function */
            break;
        case DRIVE_TYPE_2000:
            mon_ioreg_add_list(&drivemem_ioreg_list, "VIA", 0x4000, 0x400f, via4000_dump, context, 0);
            mon_ioreg_add_list(&drivemem_ioreg_list, "DP8473", 0x4e00, 0x4e07, NULL, context, 0); /* FIXME: register dump function */
            break;
        case DRIVE_TYPE_4000:
            mon_ioreg_add_list(&drivemem_ioreg_list, "VIA", 0x4000, 0x400f, via4000_dump, context, 0);
            mon_ioreg_add_list(&drivemem_ioreg_list, "PC8477", 0x4e00, 0x4e07, NULL, context, 0); /* FIXME: register dump function */
            break;
        case DRIVE_TYPE_2040:
        case DRIVE_TYPE_3040:
        case DRIVE_TYPE_4040:
        case DRIVE_TYPE_1001:
        case DRIVE_TYPE_8050:
        case DRIVE_TYPE_8250:
            mon_ioreg_add_list(&drivemem_ioreg_list, "RIOT1", 0x0200, 0x021f, riot1_dump, context, 0);
            mon_ioreg_add_list(&drivemem_ioreg_list, "RIOT2", 0x0280, 0x029f, riot2_dump, context, 0);
            break;
        default:
            log_error(LOG_DEFAULT, "DRIVEMEM: Unknown drive type `%i'.", type);
            break;
    }
    return drivemem_ioreg_list;
}
