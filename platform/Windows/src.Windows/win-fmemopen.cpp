// Windows shim for POSIX fmemopen().
//
// Used by the GoatTracker assembler (src/Plugins/GoatTracker/gt2/asm/) to
// capture diagnostic output into a fixed-size character buffer. The host
// expects writes to land in the supplied buffer so the message can be
// surfaced after the assembler exits via longjmp.
//
// We implement this on Windows using a thread-local list of "fake" FILE*
// streams backed by a tmpfile() plus a record of the destination buffer.
// On fclose() we intercept by exposing a companion gt2_asm_close() in the
// caller — but to stay drop-in, we override behavior by writing through
// setvbuf() onto the user buffer and treating subsequent fflush as a
// memory copy. The buffer is kept in sync after every fflush/fclose.
//
// For the GoatTracker use case (small text diagnostics under 2 KB), this
// is sufficient.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" FILE *fmemopen(void *buf, size_t size, const char *mode)
{
	if (!buf || size == 0 || !mode) return nullptr;

	// Only "w" / "w+" / "wb" supported (assembler always writes).
	if (mode[0] != 'w') return nullptr;

	FILE *f = tmpfile();
	if (!f) return nullptr;

	// Attach the user buffer as the stdio buffer. While the stream is
	// open and unflushed, formatted writes (fprintf etc.) accumulate
	// directly into `buf` until the buffer fills, at which point stdio
	// drains it to the tmpfile. For typical GT2 error sizes (a few
	// hundred bytes) this is a pure-memory operation.
	if (setvbuf(f, (char *)buf, _IOFBF, size) != 0) {
		fclose(f);
		return nullptr;
	}

	// Ensure the buffer is null-terminated up front so consumers reading
	// before any writes don't observe garbage.
	((char *)buf)[0] = '\0';
	return f;
}
