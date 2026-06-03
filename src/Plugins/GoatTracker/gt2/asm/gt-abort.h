#ifndef GT_ABORT_H
#define GT_ABORT_H

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

// When gt2_asm_abort_active is set, exit() is redirected to longjmp
// so assembler errors don't kill the host application
extern jmp_buf gt2_asm_abort_jmp;
extern int gt2_asm_abort_active;

// Include stdlib before this macro so later guarded stdlib includes do not
// expand exit() inside the C runtime declaration.
#define exit(code) do { \
    fflush(stderr); \
    fflush(stdout); \
    if (gt2_asm_abort_active) \
        longjmp(gt2_asm_abort_jmp, (code) ? (code) : -99); \
    else \
        _Exit(code); \
} while(0)

#endif
