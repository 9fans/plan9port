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

#include "os.h"
#include "bzlib.h"
#include "bzlib_private.h"

/*---------------------------------------------------*/
/*--- Compression stuff                           ---*/
/*---------------------------------------------------*/

/*---------------------------------------------------*/
static
void prepare_new_block ( EState* s )
{
   Int32 i;
   s->nblock = 0;
   s->numZ = 0;
   s->state_out_pos = 0;
   BZ_INITIALISE_CRC ( s->blockCRC );
   for (i = 0; i < 256; i++) s->inUse[i] = False;
   s->blockNo++;
}


/*---------------------------------------------------*/
static
void init_RL ( EState* s )
{
   s->state_in_ch  = 256;
   s->state_in_len = 0;
}


static
Bool isempty_RL ( EState* s )
{
   if (s->state_in_ch < 256 && s->state_in_len > 0)
      return False; else
      return True;
}


/*---------------------------------------------------*/
int BZ_API(BZ2_bzCompressInit)
                    ( bz_stream* strm,
                     int        blockSize100k,
                     int        verbosity,
                     int        workFactor )
{
   Int32   n;
   EState* s;

   if (!bz_config_ok()) return BZ_CONFIG_ERROR;

   if (strm == NULL ||
       blockSize100k < 1 || blockSize100k > 9 ||
       workFactor < 0 || workFactor > 250)
     return BZ_PARAM_ERROR;

   if (workFactor == 0) workFactor = 30;
   if (strm->bzalloc == NULL) strm->bzalloc = default_bzalloc;
   if (strm->bzfree == NULL) strm->bzfree = default_bzfree;

   s = BZALLOC( sizeof(EState) );
   if (s == NULL) return BZ_MEM_ERROR;
   s->strm = strm;

   s->arr1 = NULL;
   s->arr2 = NULL;
   s->ftab = NULL;

   n       = 100000 * blockSize100k;
   s->arr1 = BZALLOC( n                  * sizeof(UInt32) );
   s->arr2 = BZALLOC( (n+BZ_N_OVERSHOOT) * sizeof(UInt32) );
   s->ftab = BZALLOC( 65537              * sizeof(UInt32) );

   if (s->arr1 == NULL || s->arr2 == NULL || s->ftab == NULL) {
      if (s->arr1 != NULL) BZFREE(s->arr1);
      if (s->arr2 != NULL) BZFREE(s->arr2);
      if (s->ftab != NULL) BZFREE(s->ftab);
      if (s       != NULL) BZFREE(s);
      return BZ_MEM_ERROR;
   }

   s->blockNo           = 0;
   s->state             = BZ_S_INPUT;
   s->mode              = BZ_M_RUNNING;
   s->combinedCRC       = 0;
   s->blockSize100k     = blockSize100k;
   s->nblockMAX         = 100000 * blockSize100k - 19;
   s->verbosity         = verbosity;
   s->workFactor        = workFactor;

   s->block             = (UChar*)s->arr2;
   s->mtfv              = (UInt16*)s->arr1;
   s->zbits             = NULL;
   s->ptr               = (UInt32*)s->arr1;

   strm->state          = s;
   strm->total_in_lo32  = 0;
   strm->total_in_hi32  = 0;
   strm->total_out_lo32 = 0;
   strm->total_out_hi32 = 0;
   init_RL ( s );
   prepare_new_block ( s );
   return BZ_OK;
}


/*---------------------------------------------------*/
static
void add_pair_to_block ( EState* s )
{
   Int32 i;
   UChar ch = (UChar)(s->state_in_ch);
   for (i = 0; i < s->state_in_len; i++) {
      BZ_UPDATE_CRC( s->blockCRC, ch );
   }
   s->inUse[s->state_in_ch] = True;
   switch (s->state_in_len) {
      case 1:
         s->block[s->nblock] = (UChar)ch; s->nblock++;
         break;
      case 2:
         s->block[s->nblock] = (UChar)ch; s->nblock++;
         s->block[s->nblock] = (UChar)ch; s->nblock++;
         break;
      case 3:
         s->block[s->nblock] = (UChar)ch; s->nblock++;
         s->block[s->nblock] = (UChar)ch; s->nblock++;
         s->block[s->nblock] = (UChar)ch; s->nblock++;
         break;
      default:
         s->inUse[s->state_in_len-4] = True;
         s->block[s->nblock] = (UChar)ch; s->nblock++;
         s->block[s->nblock] = (UChar)ch; s->nblock++;
         s->block[s->nblock] = (UChar)ch; s->nblock++;
         s->block[s->nblock] = (UChar)ch; s->nblock++;
         s->block[s->nblock] = ((UChar)(s->state_in_len-4));
         s->nblock++;
         break;
   }
}


/*---------------------------------------------------*/
static
void flush_RL ( EState* s )
{
   if (s->state_in_ch < 256) add_pair_to_block ( s );
   init_RL ( s );
}


/*---------------------------------------------------*/
#define ADD_CHAR_TO_BLOCK(zs,zchh0)               \
{                                                 \
   UInt32 zchh = (UInt32)(zchh0);                 \
   /*-- fast track the common case --*/           \
   if (zchh != zs->state_in_ch &&                 \
       zs->state_in_len == 1) {                   \
      UChar ch = (UChar)(zs->state_in_ch);        \
      BZ_UPDATE_CRC( zs->blockCRC, ch );          \
      zs->inUse[zs->state_in_ch] = True;          \
      zs->block[zs->nblock] = (UChar)ch;          \
      zs->nblock++;                               \
      zs->state_in_ch = zchh;                     \
   }                                              \
   else                                           \
   /*-- general, uncommon cases --*/              \
   if (zchh != zs->state_in_ch ||                 \
      zs->state_in_len == 255) {                  \
      if (zs->state_in_ch < 256)                  \
         add_pair_to_block ( zs );                \
      zs->state_in_ch = zchh;                     \
      zs->state_in_len = 1;                       \
   } else {                                       \
      zs->state_in_len++;                         \
   }                                              \
}


/*---------------------------------------------------*/
static
Bool copy_input_until_stop ( EState* s )
{
   Bool progress_in = False;

   if (s->mode == BZ_M_RUNNING) {

      /*-- fast track the common case --*/
      while (True) {
         /*-- block full? --*/
         if (s->nblock >= s->nblockMAX) break;
         /*-- no input? --*/
         if (s->strm->avail_in == 0) break;
         progress_in = True;
         ADD_CHAR_TO_BLOCK ( s, (UInt32)(*((UChar*)(s->strm->next_in))) );
         s->strm->next_in++;
         s->strm->avail_in--;
         s->strm->total_in_lo32++;
         if (s->strm->total_in_lo32 == 0) s->strm->total_in_hi32++;
      }

   } else {

      /*-- general, uncommon case --*/
      while (True) {
         /*-- block full? --*/
         if (s->nblock >= s->nblockMAX) break;
         /*-- no input? --*/
         if (s->strm->avail_in == 0) break;
         /*-- flush/finish end? --*/
         if (s->avail_in_expect == 0) break;
         progress_in = True;
         ADD_CHAR_TO_BLOCK ( s, (UInt32)(*((UChar*)(s->strm->next_in))) );
         s->strm->next_in++;
         s->strm->avail_in--;
         s->strm->total_in_lo32++;
         if (s->strm->total_in_lo32 == 0) s->strm->total_in_hi32++;
         s->avail_in_expect--;
      }
   }
   return progress_in;
}


/*---------------------------------------------------*/
static
Bool copy_output_until_stop ( EState* s )
{
   Bool progress_out = False;

   while (True) {

      /*-- no output space? --*/
      if (s->strm->avail_out == 0) break;

      /*-- block done? --*/
      if (s->state_out_pos >= s->numZ) break;

      progress_out = True;
      *(s->strm->next_out) = s->zbits[s->state_out_pos];
      s->state_out_pos++;
      s->strm->avail_out--;
      s->strm->next_out++;
      s->strm->total_out_lo32++;
      if (s->strm->total_out_lo32 == 0) s->strm->total_out_hi32++;
   }

   return progress_out;
}


/*---------------------------------------------------*/
static
Bool handle_compress ( bz_stream* strm )
{
   Bool progress_in  = False;
   Bool progress_out = False;
   EState* s = strm->state;

   while (True) {

      if (s->state == BZ_S_OUTPUT) {
         progress_out |= copy_output_until_stop ( s );
         if (s->state_out_pos < s->numZ) break;
         if (s->mode == BZ_M_FINISHING &&
             s->avail_in_expect == 0 &&
             isempty_RL(s)) break;
         prepare_new_block ( s );
         s->state = BZ_S_INPUT;
         if (s->mode == BZ_M_FLUSHING &&
             s->avail_in_expect == 0 &&
             isempty_RL(s)) break;
      }

      if (s->state == BZ_S_INPUT) {
         progress_in |= copy_input_until_stop ( s );
         if (s->mode != BZ_M_RUNNING && s->avail_in_expect == 0) {
            flush_RL ( s );
            BZ2_compressBlock ( s, (Bool)(s->mode == BZ_M_FINISHING) );
            s->state = BZ_S_OUTPUT;
         }
         else
         if (s->nblock >= s->nblockMAX) {
            BZ2_compressBlock ( s, False );
            s->state = BZ_S_OUTPUT;
         }
         else
         if (s->strm->avail_in == 0) {
            break;
         }
      }

   }

   return progress_in || progress_out;
}

/*---------------------------------------------------*/
int BZ_API(BZ2_bzCompress) ( bz_stream *strm, int action )
{
   Bool progress;
   EState* s;
   if (strm == NULL) return BZ_PARAM_ERROR;
   s = strm->state;
   if (s == NULL) return BZ_PARAM_ERROR;
   if (s->strm != strm) return BZ_PARAM_ERROR;

   preswitch:
   switch (s->mode) {

      case BZ_M_IDLE:
         return BZ_SEQUENCE_ERROR;

      case BZ_M_RUNNING:
         if (action == BZ_RUN) {
            progress = handle_compress ( strm );
            return progress ? BZ_RUN_OK : BZ_PARAM_ERROR;
         }
         else
	 if (action == BZ_FLUSH) {
            s->avail_in_expect = strm->avail_in;
            s->mode = BZ_M_FLUSHING;
            goto preswitch;
         }
         else
         if (action == BZ_FINISH) {
            s->avail_in_expect = strm->avail_in;
            s->mode = BZ_M_FINISHING;
            goto preswitch;
         }
         else
            return BZ_PARAM_ERROR;

      case BZ_M_FLUSHING:
         if (action != BZ_FLUSH) return BZ_SEQUENCE_ERROR;
         if (s->avail_in_expect != s->strm->avail_in)
            return BZ_SEQUENCE_ERROR;
         progress = handle_compress ( strm );
         if (!progress) return BZ_SEQUENCE_ERROR;	/*rsc added */
         if (s->avail_in_expect > 0 || !isempty_RL(s) ||
             s->state_out_pos < s->numZ) return BZ_FLUSH_OK;
         s->mode = BZ_M_RUNNING;
         return BZ_RUN_OK;

      case BZ_M_FINISHING:
         if (action != BZ_FINISH) return BZ_SEQUENCE_ERROR;
         if (s->avail_in_expect != s->strm->avail_in)
            return BZ_SEQUENCE_ERROR;
         progress = handle_compress ( strm );
         if (!progress) return BZ_SEQUENCE_ERROR;
         if (s->avail_in_expect > 0 || !isempty_RL(s) ||
             s->state_out_pos < s->numZ) return BZ_FINISH_OK;
         s->mode = BZ_M_IDLE;
         return BZ_STREAM_END;
   }
   return BZ_OK; /*--not reached--*/
}


/*---------------------------------------------------*/
int BZ_API(BZ2_bzCompressEnd)  ( bz_stream *strm )
{
   EState* s;
   if (strm == NULL) return BZ_PARAM_ERROR;
   s = strm->state;
   if (s == NULL) return BZ_PARAM_ERROR;
   if (s->strm != strm) return BZ_PARAM_ERROR;

   if (s->arr1 != NULL) BZFREE(s->arr1);
   if (s->arr2 != NULL) BZFREE(s->arr2);
   if (s->ftab != NULL) BZFREE(s->ftab);
   BZFREE(strm->state);

   strm->state = NULL;

   return BZ_OK;
}
