//
// GoatTracker v2 - additive instrument operations (RetroDebugger)
//

#include "goattrk2.h"
#include "ginstrops.h"
#include <string.h>

int instr_has_data(int num)
{
  const INSTR *p = &ginstr[num];
  return (p->ad || p->sr || p->ptr[0] || p->ptr[1] || p->ptr[2] ||
          p->ptr[3] || p->vibdelay) ? 1 : 0;
}

int instr_referenced_in_patterns(int num)
{
  int c, d;
  for (c = 0; c < MAX_PATT; c++)
  {
    for (d = 0; d <= MAX_PATTROWS; d++)
    {
      if (pattern[c][d*4] == ENDPATT) break;
      if (pattern[c][d*4+1] == num) return 1;
    }
  }
  return 0;
}

int insertinstrument(int num)
{
  int c, d;

  if (num < 1 || num > GT2_LAST_INSTR) return 0;

  // Refuse if shifting up would overflow the instrument range.
  if (instr_has_data(GT2_LAST_INSTR)) return 0;
  if (instr_referenced_in_patterns(GT2_LAST_INSTR)) return 0;

  stopsong();

  memmove(&ginstr[num+1], &ginstr[num], (GT2_LAST_INSTR-num) * sizeof(INSTR));
  clearinstr(num);

  countpatternlengths();
  for (c = 0; c < MAX_PATT; c++)
  {
    for (d = 0; d < pattlen[c]; d++)
    {
      if (pattern[c][d*4+1] >= num) pattern[c][d*4+1]++;
    }
  }
  countpatternlengths();
  return 1;
}

void freeinstrtable_partial(int num)
{
  int c, r, d;

  if (num < 1 || num >= MAX_INSTR) return;

  for (c = 0; c < MAX_TABLES; c++)
  {
    int pos, len;
    if (!ginstr[num].ptr[c]) continue;

    pos = ginstr[num].ptr[c] - 1;
    len = gettablepartlen(c, pos);

    // High-to-low: deletetable() shifts indices > r, never indices < r.
    for (r = pos + len - 1; r >= pos; r--)
    {
      int covered = 0;
      for (d = 1; d < MAX_INSTR; d++)
      {
        int dpos, dlen;
        if (d == num) continue;
        if (!ginstr[d].ptr[c]) continue;
        dpos = ginstr[d].ptr[c] - 1;
        dlen = gettablepartlen(c, dpos);
        if (r >= dpos && r < dpos + dlen) { covered = 1; break; }
      }
      if (!covered) deletetable(c, r);
    }
  }
}

void deleteinstrument(int num)
{
  int c, d;

  if (num < 1 || num > GT2_LAST_INSTR) return;

  stopsong();

  freeinstrtable_partial(num);

  memmove(&ginstr[num], &ginstr[num+1], (GT2_LAST_INSTR-num) * sizeof(INSTR));
  clearinstr(GT2_LAST_INSTR);

  countpatternlengths();
  for (c = 0; c < MAX_PATT; c++)
  {
    for (d = 0; d < pattlen[c]; d++)
    {
      if (pattern[c][d*4+1] > num) pattern[c][d*4+1]--;
    }
  }
  countpatternlengths();
}

void deleteunusedinstruments(void)
{
  int c, d;
  unsigned char used[MAX_INSTR];

  stopsong();
  countpatternlengths();

  memset(used, 0, sizeof(used));
  for (c = 0; c < MAX_PATT; c++)
    for (d = 0; d < pattlen[c]; d++)
      if (pattern[c][d*4+1]) used[pattern[c][d*4+1]] = 1;

  // High-to-low: deleting instrument c only shifts instruments > c, which
  // were already processed; used[] for indices < c stays valid.
  for (c = GT2_LAST_INSTR; c >= 1; c--)
  {
    if (!used[c])
      deleteinstrument(c);
  }
}

void instrpackage_capture(int num, INSTRPACKAGE *pkg)
{
  int c, i;

  if (num < 1 || num > GT2_LAST_INSTR) return;
  memset(pkg, 0, sizeof(INSTRPACKAGE));
  memcpy(&pkg->instr, &ginstr[num], sizeof(INSTR));

  for (c = 0; c < MAX_TABLES; c++)
  {
    pkg->origptr[c] = ginstr[num].ptr[c];
    if (ginstr[num].ptr[c])
    {
      int pos = ginstr[num].ptr[c] - 1;
      int len = gettablepartlen(c, pos);
      pkg->tablen[c] = len;
      for (i = 0; i < len && (pos + i) < MAX_TABLELEN; i++)
      {
        pkg->ltab[c][i] = ltable[c][pos+i];
        pkg->rtab[c][i] = rtable[c][pos+i];
      }
    }
  }
  pkg->valid = 1;
}

void instrpackage_apply(int num, const INSTRPACKAGE *pkg)
{
  int c, i;

  if (!pkg->valid) return;
  if (num < 1 || num > GT2_LAST_INSTR) return;

  stopsong();

  // Free the target's current uniquely-owned table rows.
  freeinstrtable_partial(num);

  memcpy(&ginstr[num], &pkg->instr, sizeof(INSTR));

  for (c = 0; c < MAX_TABLES; c++)
  {
    int len = pkg->tablen[c];
    if (len)
    {
      int start = gettablelen(c);
      if (start + len > MAX_TABLELEN) len = MAX_TABLELEN - start;
      for (i = 0; i < len; i++)
      {
        ltable[c][start+i] = pkg->ltab[c][i];
        rtable[c][start+i] = pkg->rtab[c][i];
      }
      // Rebase internal jump pointers (mirror loadinstrument, gsong.c).
      if (c != STBL)
      {
        for (i = 0; i < len; i++)
        {
          if (ltable[c][start+i] == 0xff && rtable[c][start+i])
            rtable[c][start+i] =
              rtable[c][start+i] - pkg->origptr[c] + start + 1;
        }
      }
      ginstr[num].ptr[c] = start + 1;
    }
    else ginstr[num].ptr[c] = 0;
  }
}
