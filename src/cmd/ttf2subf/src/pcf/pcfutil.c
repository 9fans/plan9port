/*

Copyright 1990, 1994, 1998  The Open Group

All Rights Reserved.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/
/* $XFree86: xc/lib/font/util/utilbitmap.c,v 1.3 1999/08/22 08:58:58 dawes Exp $ */

/*
 * Author:  Keith Packard, MIT X Consortium
 */


#include <ft2build.h>
#include "pcfutil.h"


  /* Utility functions for reformatting font bitmaps */

  static const unsigned char  _reverse_byte[0x100] =
  {
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
  };

  /*
   *  Invert bit order within each BYTE of an array.
   */

  void
  BitOrderInvert( unsigned char*  buf,
                  int             nbytes )
  {
    const unsigned char*  rev = _reverse_byte;


    for ( ; --nbytes >= 0; buf++ )
      *buf = rev[*buf];
  }


  /*
   *  Invert byte order within each 16-bits of an array.
   */

  void
  TwoByteSwap( unsigned char*  buf,
               int             nbytes )
  {
    unsigned char  c;


    for ( ; nbytes > 0; nbytes -= 2, buf += 2 )
    {
      c      = buf[0];
      buf[0] = buf[1];
      buf[1] = c;
    }
  }

  /*
   *  Invert byte order within each 32-bits of an array.
   */

  void
  FourByteSwap( unsigned char*  buf,
                int             nbytes )
  {
    unsigned char  c;


    for ( ; nbytes > 0; nbytes -= 4, buf += 4 )
    {
      c      = buf[0];
      buf[0] = buf[3];
      buf[3] = c;

      c      = buf[1];
      buf[1] = buf[2];
      buf[2] = c;
    }
  }


  /*
   *  Repad a bitmap.
   */

  int
  RepadBitmap( char*         pSrc,
               char*         pDst,
               unsigned int  srcPad,
               unsigned int  dstPad,
               int           width,
               int           height )
  {
    int   srcWidthBytes, dstWidthBytes;
    int   row, col;
    char  *pTmpSrc, *pTmpDst;


    switch ( srcPad )
    {
    case 1:
      srcWidthBytes = ( width + 7 ) >> 3;
      break;

    case 2:
      srcWidthBytes = ( ( width + 15 ) >> 4 ) << 1;
      break;

    case 4:
      srcWidthBytes = ( ( width + 31 ) >> 5 ) << 2;
      break;

    case 8:
      srcWidthBytes = ( ( width + 63 ) >> 6 ) << 3;
      break;

    default:
      return 0;
    }

    switch ( dstPad )
    {
    case 1:
      dstWidthBytes = ( width + 7 ) >> 3;
      break;

    case 2:
      dstWidthBytes = ( ( width + 15 ) >> 4 ) << 1;
      break;

    case 4:
      dstWidthBytes = ( ( width + 31 ) >> 5 ) << 2;
      break;

    case 8:
      dstWidthBytes = ( ( width + 63 ) >> 6 ) << 3;
      break;

    default:
      return 0;
    }

    width = srcWidthBytes;
    if ( width > dstWidthBytes )
      width = dstWidthBytes;

    pTmpSrc= pSrc;
    pTmpDst= pDst;

    for ( row = 0; row < height; row++ )
    {
      for ( col = 0; col < width; col++ )
        *pTmpDst++ = *pTmpSrc++;

      while ( col < dstWidthBytes )
      {
        *pTmpDst++ = '\0';
        col++;
      }
      pTmpSrc += srcWidthBytes - width;
    }

    return dstWidthBytes * height;
  }


/* END */
