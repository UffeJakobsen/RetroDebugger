#ifndef GINSTROPS_H
#define GINSTROPS_H

#include "gcommon.h"

// Last freely usable instrument slot. Task 2.0 confirmed slot MAX_INSTR-1
// (63) is reserved as the tempo-override slot (read by gplay.c / greloc.c),
// so the last usable slot is MAX_INSTR-2 (62).
#define GT2_LAST_INSTR (MAX_INSTR-2)

// Package clipboard: an instrument struct plus its 4 table slices,
// captured so paste/favorites produce a fully independent instrument.
typedef struct
{
  int valid;
  INSTR instr;
  unsigned char origptr[MAX_TABLES];          // 1-based source ptr per table
  int tablen[MAX_TABLES];                     // captured slice length per table
  unsigned char ltab[MAX_TABLES][MAX_TABLELEN];
  unsigned char rtab[MAX_TABLES][MAX_TABLELEN];
} INSTRPACKAGE;

// Frees only the table rows uniquely owned by instrument `num` (rows not
// covered by any other instrument's table part). Iterates high-to-low.
void freeinstrtable_partial(int num);

// Inserts an empty instrument at `num`, shifting later instruments up and
// renumbering pattern references. Returns 1 on success, 0 if refused
// (last slot occupied, or a pattern references the last slot).
int insertinstrument(int num);

// Deletes instrument `num`, freeing its uniquely-owned tables, shifting
// later instruments down and renumbering pattern references.
void deleteinstrument(int num);

// Deletes every instrument not referenced by any pattern cell.
void deleteunusedinstruments(void);

// Package clipboard / favorites helpers.
void instrpackage_capture(int num, INSTRPACKAGE *pkg);
void instrpackage_apply(int num, const INSTRPACKAGE *pkg);

// True if instrument `num` holds any non-default data.
int instr_has_data(int num);
// True if any pattern cell references instrument `num`.
int instr_referenced_in_patterns(int num);

#endif
