/*
 * cmdhd.h - CMD HD emulation stub
 *
 * VICE 3.10 compatibility stub. The full CMD HD implementation
 * (cmdhd.c, ~1628 lines) is deferred to Phase 3.
 * This provides enough declarations for iecbus.c and drivetypes.h to compile.
 */
#ifndef VICE_CMDHD_H
#define VICE_CMDHD_H

struct drive_context_s;
struct cmdhd_context_s;
struct snapshot_s;

/* cmdbus — parallel command bus for CMD HD devices */
typedef struct cmdbus_s {
    uint8_t drv_bus[NUM_DISK_UNITS];
    uint8_t drv_data[NUM_DISK_UNITS];
    uint8_t cpu_bus;
    uint8_t cpu_data;
    uint8_t bus;
    uint8_t data;
} cmdbus_t;

extern cmdbus_t cmdbus;

void cmdbus_init(void);
void cmdbus_update(void);
void cmdbus_patn_changed(int new_val, int old_val);

#endif
