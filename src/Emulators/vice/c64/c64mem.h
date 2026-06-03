/*
 * c64mem.h -- C64 memory handling.
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *  Ettore Perazzoli <ettore@comm2000.it>
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

#ifndef VICE_C64MEM_H
#define VICE_C64MEM_H

#include "mem.h"
#include "vicetypes.h"

#ifndef C64_RAM_SIZE
#define C64_RAM_SIZE 0x10000
#endif

#define C64_KERNAL_ROM_SIZE  0x2000
#define C64_BASIC_ROM_SIZE   0x2000
#define C64_CHARGEN_ROM_SIZE 0x1000

#define C64_BASIC_CHECKSUM         15702

#define C64_KERNAL_CHECKSUM_R01    54525
#define C64_KERNAL_CHECKSUM_R02    50955
#define C64_KERNAL_CHECKSUM_R03    50954
#define C64_KERNAL_CHECKSUM_R03swe 50633
#define C64_KERNAL_CHECKSUM_R43    50955
#define C64_KERNAL_CHECKSUM_R64    49680

/* the value located at 0xff80 */
#define C64_KERNAL_ID_R01    0xaa
#define C64_KERNAL_ID_R02    0x00
#define C64_KERNAL_ID_R03    0x03
#define C64_KERNAL_ID_R03swe 0x03
#define C64_KERNAL_ID_R43    0x43
#define C64_KERNAL_ID_R64    0x64

int c64_mem_init_resources(void);
int c64_mem_init_cmdline_options(void);

void mem_set_vbank(int new_vbank);

uint8_t ram_read(uint16_t addr);
void ram_store(uint16_t addr, uint8_t value);
void ram_hi_store(uint16_t addr, uint8_t value);

uint8_t chargen_read(uint16_t addr);
void chargen_store(uint16_t addr, uint8_t value);

void colorram_store(uint16_t addr, uint8_t value);
uint8_t colorram_read(uint16_t addr);

void mem_pla_config_changed(void);
void mem_set_tape_sense(int sense);
void mem_set_tape_write_in(int val);
void mem_set_tape_motor_in(int val);

extern uint8_t mem_chargen_rom[C64_CHARGEN_ROM_SIZE];

void mem_set_write_hook(int config, int page, store_func_t *f);
void mem_read_tab_set(unsigned int base, unsigned int index, read_func_ptr_t read_func);
void mem_read_base_set(unsigned int base, unsigned int index, uint8_t *mem_ptr);
void mem_read_addr_set(unsigned int base, unsigned int index, uintptr_t addr);
void mem_read_limit_set(unsigned int base, unsigned int index, uint32_t limit);

void mem_store_without_ultimax(uint16_t addr, uint8_t value);
uint8_t mem_read_without_ultimax(uint16_t addr);
void mem_store_without_romlh(uint16_t addr, uint8_t value);

void store_bank_io(uint16_t addr, uint8_t byte);
uint8_t read_bank_io(uint16_t addr);

void c64_mem_init(void);
int mem_get_current_bank_config(void);

int c64_mem_ui_init_early(void);
int c64_mem_ui_init(void);
void c64_mem_ui_shutdown(void);

uint8_t vsid_io_read(uint16_t addr);
uint8_t vsid_io_peek(uint16_t addr);
void vsid_io_store(uint16_t addr, uint8_t val);

/* DMA memory access (bypasses CPU port at $00/$01) (from VICE 3.10) */
void mem_dma_store(uint16_t addr, uint8_t value);
uint8_t mem_dma_read(uint16_t addr);
uint8_t zero_read_dma(uint16_t addr);
void zero_store_dma(uint16_t addr, uint8_t value);

#endif
