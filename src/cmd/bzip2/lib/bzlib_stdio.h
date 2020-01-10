/*
 * THIS FILE IS NOT IDENTICAL TO THE ORIGINAL
 * FROM THE BZIP2 DISTRIBUTION.
 *
 * It has been modified, mainly to break the library
 * into smaller pieces.
 *
 * Russ Cox
 * rsc@plan9.bell-labs.com
 * July 2000
 */

/*-------------------------------------------------------------*/
/*--- Library top-level functions.                          ---*/
/*---                                               bzlib.c ---*/
/*-------------------------------------------------------------*/

/*--
  This file is a part of bzip2 and/or libbzip2, a program and
  library for lossless, block-sorting data compression.

  Copyright (C) 1996-2000 Julian R Seward.  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

  2. The origin of this software must not be misrepresented; you must
     not claim that you wrote the original software.  If you use this
     software in a product, an acknowledgment in the product
     documentation would be appreciated but is not required.

  3. Altered source versions must be plainly marked as such, and must
     not be misrepresented as being the original software.

  4. The name of the author may not be used to endorse or promote
     products derived from this software without specific prior written
     permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  Julian Seward, Cambridge, UK.
  jseward@acm.org
  bzip2/libbzip2 version 1.0 of 21 March 2000

  This program is based on (at least) the work of:
     Mike Burrows
     David Wheeler
     Peter Fenwick
     Alistair Moffat
     Radford Neal
     Ian H. Witten
     Robert Sedgewick
     Jon L. Bentley

  For more information on these sources, see the manual.
--*/

/*--
   CHANGES
   ~~~~~~~
   0.9.0 -- original version.

   0.9.0a/b -- no changes in this file.

   0.9.0c
      * made zero-length BZ_FLUSH work correctly in bzCompress().
      * fixed bzWrite/bzRead to ignore zero-length requests.
      * fixed bzread to correctly handle read requests after EOF.
      * wrong parameter order in call to bzDecompressInit in
        bzBuffToBuffDecompress.  Fixed.
--*/

#include <stdio.h>

#define BZ_MAX_UNUSED 5000

typedef void BZFILE;

BZ_EXTERN BZFILE * BZ_API(BZ2_bzopen) (
      const char *path,
      const char *mode
   );

BZ_EXTERN BZFILE * BZ_API(BZ2_bzdopen) (
      int        fd,
      const char *mode
   );

BZ_EXTERN int BZ_API(BZ2_bzread) (
      BZFILE* b,
      void* buf,
      int len
   );

BZ_EXTERN int BZ_API(BZ2_bzwrite) (
      BZFILE* b,
      void*   buf,
      int     len
   );

BZ_EXTERN int BZ_API(BZ2_bzflush) (
      BZFILE* b
   );

BZ_EXTERN void BZ_API(BZ2_bzclose) (
      BZFILE* b
   );

BZ_EXTERN const char * BZ_API(BZ2_bzerror) (
      BZFILE *b,
      int    *errnum
   );


/*-- High(er) level library functions --*/
BZ_EXTERN BZFILE* BZ_API(BZ2_bzReadOpen) (
      int*  bzerror,
      FILE* f,
      int   verbosity,
      int   small,
      void* unused,
      int   nUnused
   );

BZ_EXTERN void BZ_API(BZ2_bzReadClose) (
      int*    bzerror,
      BZFILE* b
   );

BZ_EXTERN void BZ_API(BZ2_bzReadGetUnused) (
      int*    bzerror,
      BZFILE* b,
      void**  unused,
      int*    nUnused
   );

BZ_EXTERN int BZ_API(BZ2_bzRead) (
      int*    bzerror,
      BZFILE* b,
      void*   buf,
      int     len
   );

BZ_EXTERN BZFILE* BZ_API(BZ2_bzWriteOpen) (
      int*  bzerror,
      FILE* f,
      int   blockSize100k,
      int   verbosity,
      int   workFactor
   );

BZ_EXTERN void BZ_API(BZ2_bzWrite) (
      int*    bzerror,
      BZFILE* b,
      void*   buf,
      int     len
   );

BZ_EXTERN void BZ_API(BZ2_bzWriteClose) (
      int*          bzerror,
      BZFILE*       b,
      int           abandon,
      unsigned int* nbytes_in,
      unsigned int* nbytes_out
   );

BZ_EXTERN void BZ_API(BZ2_bzWriteClose64) (
      int*          bzerror,
      BZFILE*       b,
      int           abandon,
      unsigned int* nbytes_in_lo32,
      unsigned int* nbytes_in_hi32,
      unsigned int* nbytes_out_lo32,
      unsigned int* nbytes_out_hi32
   );
