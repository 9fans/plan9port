/*
 * The authors of this software are Rob Pike and Ken Thompson.
 *              Copyright (c) 2002 by Lucent Technologies.
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES MAKE
 * ANY REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */
#include <stdio.h>
#include <stdarg.h>
#include <utf.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

int
main(int argc, char *argv[])
{
	quotefmtinstall();
	print("hello world\n");
	print("x: %x\n", 0x87654321);
	print("u: %u\n", 0x87654321);
	print("d: %d\n", 0x87654321);
	print("s: %s\n", "hi there");
	print("q: %q\n", "hi i'm here");
	print("c: %c\n", '!');
	print("g: %g %g %g\n", 3.14159, 3.14159e10, 3.14159e-10);
	print("e: %e %e %e\n", 3.14159, 3.14159e10, 3.14159e-10);
	print("f: %f %f %f\n", 3.14159, 3.14159e10, 3.14159e-10);
	print("smiley: %C\n", (Rune)0x263a);
	print("%g %.18g\n", 2e25, 2e25);
	print("%2.18g\n", 1.0);
	print("%2.18f\n", 1.0);
	print("%f\n", 3.1415927/4);
	print("%d\n", 23);
	print("%i\n", 23);
	print("%0.10d\n", 12345);
	return 0;
}
