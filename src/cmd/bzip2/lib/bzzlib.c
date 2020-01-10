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
#include "bzlib_stdio.h"
#include "bzlib_stdio_private.h"

/*---------------------------------------------------*/
/*--
   Code contributed by Yoshioka Tsuneo
   (QWF00133@niftyserve.or.jp/tsuneo-y@is.aist-nara.ac.jp),
   to support better zlib compatibility.
   This code is not _officially_ part of libbzip2 (yet);
   I haven't tested it, documented it, or considered the
   threading-safeness of it.
   If this code breaks, please contact both Yoshioka and me.
--*/
/*---------------------------------------------------*/

/*---------------------------------------------------*/

#if defined(_WIN32) || defined(OS2) || defined(MSDOS)
#   include <fcntl.h>
#   include <io.h>
#   define SET_BINARY_MODE(file) setmode(fileno(file),O_BINARY)
#else
#   define SET_BINARY_MODE(file)
#endif
static
BZFILE * bzopen_or_bzdopen
               ( const char *path,   /* no use when bzdopen */
                 int fd,             /* no use when bzdopen */
                 const char *mode,
                 int open_mode)      /* bzopen: 0, bzdopen:1 */
{
   int    bzerr;
   char   unused[BZ_MAX_UNUSED];
   int    blockSize100k = 9;
   int    writing       = 0;
   char   mode2[10]     = "";
   FILE   *fp           = NULL;
   BZFILE *bzfp         = NULL;
   int    verbosity     = 0;
   int    workFactor    = 30;
   int    smallMode     = 0;
   int    nUnused       = 0;

   if (mode == NULL) return NULL;
   while (*mode) {
      switch (*mode) {
      case 'r':
         writing = 0; break;
      case 'w':
         writing = 1; break;
      case 's':
         smallMode = 1; break;
      default:
         if (isdigit((int)(*mode))) {
            blockSize100k = *mode-'0';
         }
      }
      mode++;
   }
   strcat(mode2, writing ? "w" : "r" );
   strcat(mode2,"b");   /* binary mode */

   if (open_mode==0) {
      if (path==NULL || strcmp(path,"")==0) {
        fp = (writing ? stdout : stdin);
        SET_BINARY_MODE(fp);
      } else {
        fp = fopen(path,mode2);
      }
   } else {
#ifdef BZ_STRICT_ANSI
      fp = NULL;
#else
      fp = fdopen(fd,mode2);
#endif
   }
   if (fp == NULL) return NULL;

   if (writing) {
      /* Guard against total chaos and anarchy -- JRS */
      if (blockSize100k < 1) blockSize100k = 1;
      if (blockSize100k > 9) blockSize100k = 9;
      bzfp = BZ2_bzWriteOpen(&bzerr,fp,blockSize100k,
                             verbosity,workFactor);
   } else {
      bzfp = BZ2_bzReadOpen(&bzerr,fp,verbosity,smallMode,
                            unused,nUnused);
   }
   if (bzfp == NULL) {
      if (fp != stdin && fp != stdout) fclose(fp);
      return NULL;
   }
   return bzfp;
}


/*---------------------------------------------------*/
/*--
   open file for read or write.
      ex) bzopen("file","w9")
      case path="" or NULL => use stdin or stdout.
--*/
BZFILE * BZ_API(BZ2_bzopen)
               ( const char *path,
                 const char *mode )
{
   return bzopen_or_bzdopen(path,-1,mode,/*bzopen*/0);
}


/*---------------------------------------------------*/
BZFILE * BZ_API(BZ2_bzdopen)
               ( int fd,
                 const char *mode )
{
   return bzopen_or_bzdopen(NULL,fd,mode,/*bzdopen*/1);
}


/*---------------------------------------------------*/
int BZ_API(BZ2_bzread) (BZFILE* b, void* buf, int len )
{
   int bzerr, nread;
   if (((bzFile*)b)->lastErr == BZ_STREAM_END) return 0;
   nread = BZ2_bzRead(&bzerr,b,buf,len);
   if (bzerr == BZ_OK || bzerr == BZ_STREAM_END) {
      return nread;
   } else {
      return -1;
   }
}


/*---------------------------------------------------*/
int BZ_API(BZ2_bzwrite) (BZFILE* b, void* buf, int len )
{
   int bzerr;

   BZ2_bzWrite(&bzerr,b,buf,len);
   if(bzerr == BZ_OK){
      return len;
   }else{
      return -1;
   }
}


/*---------------------------------------------------*/
int BZ_API(BZ2_bzflush) (BZFILE *b)
{
   /* do nothing now... */
   return 0;
}


/*---------------------------------------------------*/
void BZ_API(BZ2_bzclose) (BZFILE* b)
{
   int bzerr;
   FILE *fp = ((bzFile *)b)->handle;

   if (b==NULL) {return;}
   if(((bzFile*)b)->writing){
      BZ2_bzWriteClose(&bzerr,b,0,NULL,NULL);
      if(bzerr != BZ_OK){
         BZ2_bzWriteClose(NULL,b,1,NULL,NULL);
      }
   }else{
      BZ2_bzReadClose(&bzerr,b);
   }
   if(fp!=stdin && fp!=stdout){
      fclose(fp);
   }
}


/*---------------------------------------------------*/
/*--
   return last error code
--*/
static char *bzerrorstrings[] = {
       "OK"
      ,"SEQUENCE_ERROR"
      ,"PARAM_ERROR"
      ,"MEM_ERROR"
      ,"DATA_ERROR"
      ,"DATA_ERROR_MAGIC"
      ,"IO_ERROR"
      ,"UNEXPECTED_EOF"
      ,"OUTBUFF_FULL"
      ,"CONFIG_ERROR"
      ,"???"   /* for future */
      ,"???"   /* for future */
      ,"???"   /* for future */
      ,"???"   /* for future */
      ,"???"   /* for future */
      ,"???"   /* for future */
};


const char * BZ_API(BZ2_bzerror) (BZFILE *b, int *errnum)
{
   int err = ((bzFile *)b)->lastErr;

   if(err>0) err = 0;
   *errnum = err;
   return bzerrorstrings[err*-1];
}
