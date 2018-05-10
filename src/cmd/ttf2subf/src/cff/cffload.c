/***************************************************************************/
/*                                                                         */
/*  cffload.c                                                              */
/*                                                                         */
/*    OpenType and CFF data/program tables loader (body).                  */
/*                                                                         */
/*  Copyright 1996-2001, 2002 by                                           */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/


#include <ft2build.h>
#include FT_INTERNAL_DEBUG_H
#include FT_INTERNAL_OBJECTS_H
#include FT_INTERNAL_STREAM_H
#include FT_INTERNAL_POSTSCRIPT_NAMES_H
#include FT_TRUETYPE_TAGS_H

#include "cffload.h"
#include "cffparse.h"

#include "cfferrs.h"


#if 1
  static const FT_UShort  cff_isoadobe_charset[229] =
  {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    50,
    51,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
    59,
    60,
    61,
    62,
    63,
    64,
    65,
    66,
    67,
    68,
    69,
    70,
    71,
    72,
    73,
    74,
    75,
    76,
    77,
    78,
    79,
    80,
    81,
    82,
    83,
    84,
    85,
    86,
    87,
    88,
    89,
    90,
    91,
    92,
    93,
    94,
    95,
    96,
    97,
    98,
    99,
    100,
    101,
    102,
    103,
    104,
    105,
    106,
    107,
    108,
    109,
    110,
    111,
    112,
    113,
    114,
    115,
    116,
    117,
    118,
    119,
    120,
    121,
    122,
    123,
    124,
    125,
    126,
    127,
    128,
    129,
    130,
    131,
    132,
    133,
    134,
    135,
    136,
    137,
    138,
    139,
    140,
    141,
    142,
    143,
    144,
    145,
    146,
    147,
    148,
    149,
    150,
    151,
    152,
    153,
    154,
    155,
    156,
    157,
    158,
    159,
    160,
    161,
    162,
    163,
    164,
    165,
    166,
    167,
    168,
    169,
    170,
    171,
    172,
    173,
    174,
    175,
    176,
    177,
    178,
    179,
    180,
    181,
    182,
    183,
    184,
    185,
    186,
    187,
    188,
    189,
    190,
    191,
    192,
    193,
    194,
    195,
    196,
    197,
    198,
    199,
    200,
    201,
    202,
    203,
    204,
    205,
    206,
    207,
    208,
    209,
    210,
    211,
    212,
    213,
    214,
    215,
    216,
    217,
    218,
    219,
    220,
    221,
    222,
    223,
    224,
    225,
    226,
    227,
    228
  };

  static const FT_UShort  cff_expert_charset[166] =
  {
    0,
    1,
    229,
    230,
    231,
    232,
    233,
    234,
    235,
    236,
    237,
    238,
    13,
    14,
    15,
    99,
    239,
    240,
    241,
    242,
    243,
    244,
    245,
    246,
    247,
    248,
    27,
    28,
    249,
    250,
    251,
    252,
    253,
    254,
    255,
    256,
    257,
    258,
    259,
    260,
    261,
    262,
    263,
    264,
    265,
    266,
    109,
    110,
    267,
    268,
    269,
    270,
    271,
    272,
    273,
    274,
    275,
    276,
    277,
    278,
    279,
    280,
    281,
    282,
    283,
    284,
    285,
    286,
    287,
    288,
    289,
    290,
    291,
    292,
    293,
    294,
    295,
    296,
    297,
    298,
    299,
    300,
    301,
    302,
    303,
    304,
    305,
    306,
    307,
    308,
    309,
    310,
    311,
    312,
    313,
    314,
    315,
    316,
    317,
    318,
    158,
    155,
    163,
    319,
    320,
    321,
    322,
    323,
    324,
    325,
    326,
    150,
    164,
    169,
    327,
    328,
    329,
    330,
    331,
    332,
    333,
    334,
    335,
    336,
    337,
    338,
    339,
    340,
    341,
    342,
    343,
    344,
    345,
    346,
    347,
    348,
    349,
    350,
    351,
    352,
    353,
    354,
    355,
    356,
    357,
    358,
    359,
    360,
    361,
    362,
    363,
    364,
    365,
    366,
    367,
    368,
    369,
    370,
    371,
    372,
    373,
    374,
    375,
    376,
    377,
    378
  };

  static const FT_UShort  cff_expertsubset_charset[87] =
  {
    0,
    1,
    231,
    232,
    235,
    236,
    237,
    238,
    13,
    14,
    15,
    99,
    239,
    240,
    241,
    242,
    243,
    244,
    245,
    246,
    247,
    248,
    27,
    28,
    249,
    250,
    251,
    253,
    254,
    255,
    256,
    257,
    258,
    259,
    260,
    261,
    262,
    263,
    264,
    265,
    266,
    109,
    110,
    267,
    268,
    269,
    270,
    272,
    300,
    301,
    302,
    305,
    314,
    315,
    158,
    155,
    163,
    320,
    321,
    322,
    323,
    324,
    325,
    326,
    150,
    164,
    169,
    327,
    328,
    329,
    330,
    331,
    332,
    333,
    334,
    335,
    336,
    337,
    338,
    339,
    340,
    341,
    342,
    343,
    344,
    345,
    346
  };

  static const FT_UShort  cff_standard_encoding[256] =
  {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    50,
    51,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
    59,
    60,
    61,
    62,
    63,
    64,
    65,
    66,
    67,
    68,
    69,
    70,
    71,
    72,
    73,
    74,
    75,
    76,
    77,
    78,
    79,
    80,
    81,
    82,
    83,
    84,
    85,
    86,
    87,
    88,
    89,
    90,
    91,
    92,
    93,
    94,
    95,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    96,
    97,
    98,
    99,
    100,
    101,
    102,
    103,
    104,
    105,
    106,
    107,
    108,
    109,
    110,
    0,
    111,
    112,
    113,
    114,
    0,
    115,
    116,
    117,
    118,
    119,
    120,
    121,
    122,
    0,
    123,
    0,
    124,
    125,
    126,
    127,
    128,
    129,
    130,
    131,
    0,
    132,
    133,
    0,
    134,
    135,
    136,
    137,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    138,
    0,
    139,
    0,
    0,
    0,
    0,
    140,
    141,
    142,
    143,
    0,
    0,
    0,
    0,
    0,
    144,
    0,
    0,
    0,
    145,
    0,
    0,
    146,
    147,
    148,
    149,
    0,
    0,
    0,
    0
  };

  static const FT_UShort  cff_expert_encoding[256] =
  {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    229,
    230,
    0,
    231,
    232,
    233,
    234,
    235,
    236,
    237,
    238,
    13,
    14,
    15,
    99,
    239,
    240,
    241,
    242,
    243,
    244,
    245,
    246,
    247,
    248,
    27,
    28,
    249,
    250,
    251,
    252,
    0,
    253,
    254,
    255,
    256,
    257,
    0,
    0,
    0,
    258,
    0,
    0,
    259,
    260,
    261,
    262,
    0,
    0,
    263,
    264,
    265,
    0,
    266,
    109,
    110,
    267,
    268,
    269,
    0,
    270,
    271,
    272,
    273,
    274,
    275,
    276,
    277,
    278,
    279,
    280,
    281,
    282,
    283,
    284,
    285,
    286,
    287,
    288,
    289,
    290,
    291,
    292,
    293,
    294,
    295,
    296,
    297,
    298,
    299,
    300,
    301,
    302,
    303,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    304,
    305,
    306,
    0,
    0,
    307,
    308,
    309,
    310,
    311,
    0,
    312,
    0,
    0,
    312,
    0,
    0,
    314,
    315,
    0,
    0,
    316,
    317,
    318,
    0,
    0,
    0,
    158,
    155,
    163,
    319,
    320,
    321,
    322,
    323,
    324,
    325,
    0,
    0,
    326,
    150,
    164,
    169,
    327,
    328,
    329,
    330,
    331,
    332,
    333,
    334,
    335,
    336,
    337,
    338,
    339,
    340,
    341,
    342,
    343,
    344,
    345,
    346,
    347,
    348,
    349,
    350,
    351,
    352,
    353,
    354,
    355,
    356,
    357,
    358,
    359,
    360,
    361,
    362,
    363,
    364,
    365,
    366,
    367,
    368,
    369,
    370,
    371,
    372,
    373,
    374,
    375,
    376,
    377,
    378
  };
#endif


  FT_LOCAL_DEF( FT_UShort )
  cff_get_standard_encoding( FT_UInt  charcode )
  {
    return  (FT_UShort)(charcode < 256 ? cff_standard_encoding[charcode] : 0);
  }


  /*************************************************************************/
  /*                                                                       */
  /* The macro FT_COMPONENT is used in trace mode.  It is an implicit      */
  /* parameter of the FT_TRACE() and FT_ERROR() macros, used to print/log  */
  /* messages during execution.                                            */
  /*                                                                       */
#undef  FT_COMPONENT
#define FT_COMPONENT  trace_cffload


  /* read a CFF offset from memory */
  static FT_ULong
  cff_get_offset( FT_Byte*  p,
                  FT_Byte   off_size )
  {
    FT_ULong  result;


    for ( result = 0; off_size > 0; off_size-- )
    {
      result <<= 8;
      result  |= *p++;
    }

    return result;
  }


  static FT_Error
  cff_new_index( CFF_Index  idx,
                 FT_Stream  stream,
                 FT_Bool    load )
  {
    FT_Error   error;
    FT_Memory  memory = stream->memory;
    FT_UShort  count;


    FT_MEM_ZERO( idx, sizeof ( *idx ) );

    idx->stream = stream;
    if ( !FT_READ_USHORT( count ) &&
         count > 0             )
    {
      FT_Byte*   p;
      FT_Byte    offsize;
      FT_ULong   data_size;
      FT_ULong*  poff;


      /* there is at least one element; read the offset size,           */
      /* then access the offset table to compute the index's total size */
      if ( FT_READ_BYTE( offsize ) )
        goto Exit;

      idx->stream   = stream;
      idx->count    = count;
      idx->off_size = offsize;
      data_size     = (FT_ULong)( count + 1 ) * offsize;

      if ( FT_NEW_ARRAY( idx->offsets, count + 1 ) ||
           FT_FRAME_ENTER( data_size )             )
        goto Exit;

      poff = idx->offsets;
      p    = (FT_Byte*)stream->cursor;

      for ( ; (FT_Short)count >= 0; count-- )
      {
        poff[0] = cff_get_offset( p, offsize );
        poff++;
        p += offsize;
      }

      FT_FRAME_EXIT();

      idx->data_offset = FT_STREAM_POS();
      data_size        = poff[-1] - 1;

      if ( load )
      {
        /* load the data */
        if ( FT_FRAME_EXTRACT( data_size, idx->bytes ) )
          goto Exit;
      }
      else
      {
        /* skip the data */
        if ( FT_STREAM_SKIP( data_size ) )
          goto Exit;
      }
    }

  Exit:
    if ( error )
      FT_FREE( idx->offsets );

    return error;
  }


  static void
  cff_done_index( CFF_Index  idx )
  {
    if ( idx->stream )
    {
      FT_Stream  stream = idx->stream;
      FT_Memory  memory = stream->memory;


      if ( idx->bytes )
        FT_FRAME_RELEASE( idx->bytes );

      FT_FREE( idx->offsets );
      FT_MEM_ZERO( idx, sizeof ( *idx ) );
    }
  }


 /* allocate a table containing pointers to an index's elements */
  static FT_Error
  cff_index_get_pointers( CFF_Index   idx,
                          FT_Byte***  table )
  {
    FT_Error   error  = 0;
    FT_Memory  memory = idx->stream->memory;
    FT_ULong   n, offset, old_offset;
    FT_Byte**  t;


    *table = 0;

    if ( idx->count > 0 && !FT_NEW_ARRAY( t, idx->count + 1 ) )
    {
      old_offset = 1;
      for ( n = 0; n <= idx->count; n++ )
      {
        offset = idx->offsets[n];
        if ( !offset )
          offset = old_offset;

        t[n] = idx->bytes + offset - 1;

        old_offset = offset;
      }
      *table = t;
    }

    return error;
  }


  FT_LOCAL_DEF( FT_Error )
  cff_index_access_element( CFF_Index  idx,
                            FT_UInt    element,
                            FT_Byte**  pbytes,
                            FT_ULong*  pbyte_len )
  {
    FT_Error  error = 0;


    if ( idx && idx->count > element )
    {
      /* compute start and end offsets */
      FT_ULong  off1, off2 = 0;


      off1 = idx->offsets[element];
      if ( off1 )
      {
        do
        {
          element++;
          off2 = idx->offsets[element];

        } while ( off2 == 0 && element < idx->count );

        if ( !off2 )
          off1 = 0;
      }

      /* access element */
      if ( off1 )
      {
        *pbyte_len = off2 - off1;

        if ( idx->bytes )
        {
          /* this index was completely loaded in memory, that's easy */
          *pbytes = idx->bytes + off1 - 1;
        }
        else
        {
          /* this index is still on disk/file, access it through a frame */
          FT_Stream  stream = idx->stream;


          if ( FT_STREAM_SEEK( idx->data_offset + off1 - 1 ) ||
               FT_FRAME_EXTRACT( off2 - off1, *pbytes )      )
            goto Exit;
        }
      }
      else
      {
        /* empty index element */
        *pbytes    = 0;
        *pbyte_len = 0;
      }
    }
    else
      error = CFF_Err_Invalid_Argument;

  Exit:
    return error;
  }


  FT_LOCAL_DEF( void )
  cff_index_forget_element( CFF_Index  idx,
                            FT_Byte**  pbytes )
  {
    if ( idx->bytes == 0 )
    {
      FT_Stream  stream = idx->stream;


      FT_FRAME_RELEASE( *pbytes );
    }
  }


  FT_LOCAL_DEF( FT_String* )
  cff_index_get_name( CFF_Index  idx,
                      FT_UInt    element )
  {
    FT_Memory   memory = idx->stream->memory;
    FT_Byte*    bytes;
    FT_ULong    byte_len;
    FT_Error    error;
    FT_String*  name = 0;


    error = cff_index_access_element( idx, element, &bytes, &byte_len );
    if ( error )
      goto Exit;

    if ( !FT_ALLOC( name, byte_len + 1 ) )
    {
      FT_MEM_COPY( name, bytes, byte_len );
      name[byte_len] = 0;
    }
    cff_index_forget_element( idx, &bytes );

  Exit:
    return name;
  }


  FT_LOCAL_DEF( FT_String* )
  cff_index_get_sid_string( CFF_Index        idx,
                            FT_UInt          sid,
                            PSNames_Service  psnames_service )
  {
    /* if it is not a standard string, return it */
    if ( sid > 390 )
      return cff_index_get_name( idx, sid - 391 );

    /* that's a standard string, fetch a copy from the PSName module */
    {
      FT_String*   name       = 0;
      const char*  adobe_name = psnames_service->adobe_std_strings( sid );
      FT_UInt      len;


      if ( adobe_name )
      {
        FT_Memory  memory = idx->stream->memory;
        FT_Error   error;


        len = (FT_UInt)ft_strlen( adobe_name );
        if ( !FT_ALLOC( name, len + 1 ) )
        {
          FT_MEM_COPY( name, adobe_name, len );
          name[len] = 0;
        }

        FT_UNUSED( error );
      }

      return name;
    }
  }


  /*************************************************************************/
  /*************************************************************************/
  /***                                                                   ***/
  /***   FD Select table support                                         ***/
  /***                                                                   ***/
  /*************************************************************************/
  /*************************************************************************/


  static void
  CFF_Done_FD_Select( CFF_FDSelect  fdselect,
                      FT_Stream     stream )
  {
    if ( fdselect->data )
      FT_FRAME_RELEASE( fdselect->data );

    fdselect->data_size   = 0;
    fdselect->format      = 0;
    fdselect->range_count = 0;
  }


  static FT_Error
  CFF_Load_FD_Select( CFF_FDSelect  fdselect,
                      FT_UInt       num_glyphs,
                      FT_Stream     stream,
                      FT_ULong      offset )
  {
    FT_Error  error;
    FT_Byte   format;
    FT_UInt   num_ranges;


    /* read format */
    if ( FT_STREAM_SEEK( offset ) || FT_READ_BYTE( format ) )
      goto Exit;

    fdselect->format      = format;
    fdselect->cache_count = 0;   /* clear cache */

    switch ( format )
    {
    case 0:     /* format 0, that's simple */
      fdselect->data_size = num_glyphs;
      goto Load_Data;

    case 3:     /* format 3, a tad more complex */
      if ( FT_READ_USHORT( num_ranges ) )
        goto Exit;

      fdselect->data_size = num_ranges * 3 + 2;

    Load_Data:
      if ( FT_FRAME_EXTRACT( fdselect->data_size, fdselect->data ) )
        goto Exit;
      break;

    default:    /* hmm... that's wrong */
      error = CFF_Err_Invalid_File_Format;
    }

  Exit:
    return error;
  }


  FT_LOCAL_DEF( FT_Byte )
  cff_fd_select_get( CFF_FDSelect  fdselect,
                     FT_UInt       glyph_index )
  {
    FT_Byte  fd = 0;


    switch ( fdselect->format )
    {
    case 0:
      fd = fdselect->data[glyph_index];
      break;

    case 3:
      /* first, compare to cache */
      if ( (FT_UInt)( glyph_index - fdselect->cache_first ) <
                        fdselect->cache_count )
      {
        fd = fdselect->cache_fd;
        break;
      }

      /* then, lookup the ranges array */
      {
        FT_Byte*  p       = fdselect->data;
        FT_Byte*  p_limit = p + fdselect->data_size;
        FT_Byte   fd2;
        FT_UInt   first, limit;


        first = FT_NEXT_USHORT( p );
        do
        {
          if ( glyph_index < first )
            break;

          fd2   = *p++;
          limit = FT_NEXT_USHORT( p );

          if ( glyph_index < limit )
          {
            fd = fd2;

            /* update cache */
            fdselect->cache_first = first;
            fdselect->cache_count = limit-first;
            fdselect->cache_fd    = fd2;
            break;
          }
          first = limit;

        } while ( p < p_limit );
      }
      break;

    default:
      ;
    }

    return fd;
  }


  /*************************************************************************/
  /*************************************************************************/
  /***                                                                   ***/
  /***   CFF font support                                                ***/
  /***                                                                   ***/
  /*************************************************************************/
  /*************************************************************************/

  static void
  cff_charset_done( CFF_Charset  charset,
                    FT_Stream    stream )
  {
    FT_Memory  memory = stream->memory;


    FT_FREE( charset->sids );
    charset->format = 0;
    charset->offset = 0;
  }


  static FT_Error
  cff_charset_load( CFF_Charset  charset,
                    FT_UInt      num_glyphs,
                    FT_Stream    stream,
                    FT_ULong     base_offset,
                    FT_ULong     offset )
  {
    FT_Memory  memory     = stream->memory;
    FT_Error   error      = 0;
    FT_UShort  glyph_sid;


    /* If the the offset is greater than 2, we have to parse the */
    /* charset table.                                            */
    if ( offset > 2 )
    {
      FT_UInt  j;


      charset->offset = base_offset + offset;

      /* Get the format of the table. */
      if ( FT_STREAM_SEEK( charset->offset ) ||
           FT_READ_BYTE( charset->format )   )
        goto Exit;

      /* Allocate memory for sids. */
      if ( FT_NEW_ARRAY( charset->sids, num_glyphs ) )
        goto Exit;

      /* assign the .notdef glyph */
      charset->sids[0] = 0;

      switch ( charset->format )
      {
      case 0:
        if ( num_glyphs > 0 )
        {
          if ( FT_FRAME_ENTER( ( num_glyphs - 1 ) * 2 ) )
            goto Exit;

          for ( j = 1; j < num_glyphs; j++ )
            charset->sids[j] = FT_GET_USHORT();

          FT_FRAME_EXIT();
        }
        break;

      case 1:
      case 2:
        {
          FT_UInt  nleft;
          FT_UInt  i;


          j = 1;

          while ( j < num_glyphs )
          {

            /* Read the first glyph sid of the range. */
            if ( FT_READ_USHORT( glyph_sid ) )
              goto Exit;

            /* Read the number of glyphs in the range.  */
            if ( charset->format == 2 )
            {
              if ( FT_READ_USHORT( nleft ) )
                goto Exit;
            }
            else
            {
              if ( FT_READ_BYTE( nleft ) )
                goto Exit;
            }

            /* Fill in the range of sids -- `nleft + 1' glyphs. */
            for ( i = 0; j < num_glyphs && i <= nleft; i++, j++, glyph_sid++ )
              charset->sids[j] = glyph_sid;
          }
        }
        break;

      default:
        FT_ERROR(( "cff_charset_load: invalid table format!\n" ));
        error = CFF_Err_Invalid_File_Format;
        goto Exit;
      }
    }
    else
    {
      /* Parse default tables corresponding to offset == 0, 1, or 2.  */
      /* CFF specification intimates the following:                   */
      /*                                                              */
      /* In order to use a predefined charset, the following must be  */
      /* true: The charset constructed for the glyphs in the font's   */
      /* charstrings dictionary must match the predefined charset in  */
      /* the first num_glyphs                                         */

      charset->offset = offset;  /* record charset type */

      switch ( (FT_UInt)offset )
      {
      case 0:
        if ( num_glyphs > 229 )
        {
          FT_ERROR(("cff_charset_load: implicit charset larger than\n"
                    "predefined charset (Adobe ISO-Latin)!\n" ));
          error = CFF_Err_Invalid_File_Format;
          goto Exit;
        }

        /* Allocate memory for sids. */
        if ( FT_NEW_ARRAY( charset->sids, num_glyphs ) )
          goto Exit;

        /* Copy the predefined charset into the allocated memory. */
        FT_MEM_COPY( charset->sids, cff_isoadobe_charset,
                     num_glyphs * sizeof ( FT_UShort ) );

        break;

      case 1:
        if ( num_glyphs > 166 )
        {
          FT_ERROR(( "cff_charset_load: implicit charset larger than\n"
                     "predefined charset (Adobe Expert)!\n" ));
          error = CFF_Err_Invalid_File_Format;
          goto Exit;
        }

        /* Allocate memory for sids. */
        if ( FT_NEW_ARRAY( charset->sids, num_glyphs ) )
          goto Exit;

        /* Copy the predefined charset into the allocated memory.     */
        FT_MEM_COPY( charset->sids, cff_expert_charset,
                     num_glyphs * sizeof ( FT_UShort ) );

        break;

      case 2:
        if ( num_glyphs > 87 )
        {
          FT_ERROR(( "cff_charset_load: implicit charset larger than\n"
                     "predefined charset (Adobe Expert Subset)!\n" ));
          error = CFF_Err_Invalid_File_Format;
          goto Exit;
        }

        /* Allocate memory for sids. */
        if ( FT_NEW_ARRAY( charset->sids, num_glyphs ) )
          goto Exit;

        /* Copy the predefined charset into the allocated memory.     */
        FT_MEM_COPY( charset->sids, cff_expertsubset_charset,
                     num_glyphs * sizeof ( FT_UShort ) );

        break;

      default:
        error = CFF_Err_Invalid_File_Format;
        goto Exit;
      }
    }

  Exit:

    /* Clean up if there was an error. */
    if ( error )
      if ( charset->sids )
      {
        FT_FREE( charset->sids );
        charset->format = 0;
        charset->offset = 0;
        charset->sids   = 0;
      }

    return error;
  }



  static void
  cff_encoding_done( CFF_Encoding  encoding )
  {
    encoding->format = 0;
    encoding->offset = 0;
    encoding->count  = 0;
  }



  static FT_Error
  cff_encoding_load( CFF_Encoding  encoding,
                     CFF_Charset   charset,
                     FT_UInt       num_glyphs,
                     FT_Stream     stream,
                     FT_ULong      base_offset,
                     FT_ULong      offset )
  {
    FT_Error    error  = 0;
    FT_UInt     count;
    FT_UInt     j;
    FT_UShort   glyph_sid;
    FT_UInt     glyph_code;


    /* Check for charset->sids.  If we do not have this, we fail. */
    if ( !charset->sids )
    {
      error = CFF_Err_Invalid_File_Format;
      goto Exit;
    }

    /* Zero out the code to gid/sid mappings. */
    for ( j = 0; j < 256; j++ )
    {
      encoding->sids [j] = 0;
      encoding->codes[j] = 0;
    }

    /* Note: The encoding table in a CFF font is indexed by glyph index,  */
    /* where the first encoded glyph index is 1.  Hence, we read the char */
    /* code (`glyph_code') at index j and make the assignment:            */
    /*                                                                    */
    /*    encoding->codes[glyph_code] = j + 1                             */
    /*                                                                    */
    /* We also make the assignment:                                       */
    /*                                                                    */
    /*    encoding->sids[glyph_code] = charset->sids[j + 1]               */
    /*                                                                    */
    /* This gives us both a code to GID and a code to SID mapping.        */

    if ( offset > 1 )
    {

      encoding->offset = base_offset + offset;

      /* we need to parse the table to determine its size */
      if ( FT_STREAM_SEEK( encoding->offset ) ||
           FT_READ_BYTE( encoding->format )   ||
           FT_READ_BYTE( count )              )
        goto Exit;

      switch ( encoding->format & 0x7F )
      {
      case 0:
        {
          FT_Byte*  p;

          /* by convention, GID 0 is always ".notdef" and is never */
          /* coded in the font. Hence, the number of codes found   */
          /* in the table is 'count+1'                             */
          /*                                                       */
          encoding->count = count + 1;

          if ( FT_FRAME_ENTER( count ) )
            goto Exit;

          p = (FT_Byte*)stream->cursor;

          for ( j = 1; j <= count; j++ )
          {
            glyph_code = *p++;

            /* Make sure j is not too big. */
            if ( j < num_glyphs )
            {
              /* Assign code to GID mapping. */
              encoding->codes[glyph_code] = (FT_UShort)j;

              /* Assign code to SID mapping. */
              encoding->sids[glyph_code] = charset->sids[j];
            }
          }

          FT_FRAME_EXIT();
        }
        break;

      case 1:
        {
          FT_Byte  nleft;
          FT_UInt  i = 1;
          FT_UInt  k;


          encoding->count = 0;

          /* Parse the Format1 ranges. */
          for ( j = 0;  j < count; j++, i += nleft )
          {
            /* Read the first glyph code of the range. */
            if ( FT_READ_BYTE( glyph_code ) )
              goto Exit;

            /* Read the number of codes in the range. */
            if ( FT_READ_BYTE( nleft ) )
              goto Exit;

            /* Increment nleft, so we read `nleft + 1' codes/sids. */
            nleft++;

            /* compute max number of character codes */
            if ( (FT_UInt)nleft > encoding->count )
              encoding->count = nleft;

            /* Fill in the range of codes/sids. */
            for ( k = i; k < nleft + i; k++, glyph_code++ )
            {
              /* Make sure k is not too big. */
              if ( k < num_glyphs && glyph_code < 256 )
              {
                /* Assign code to GID mapping. */
                encoding->codes[glyph_code] = (FT_UShort)k;

                /* Assign code to SID mapping. */
                encoding->sids[glyph_code] = charset->sids[k];
              }
            }
          }

          /* simple check, one never knows what can be found in a font */
          if ( encoding->count > 256 )
            encoding->count = 256;
        }
        break;

      default:
        FT_ERROR(( "cff_encoding_load: invalid table format!\n" ));
        error = CFF_Err_Invalid_File_Format;
        goto Exit;
      }

      /* Parse supplemental encodings, if any. */
      if ( encoding->format & 0x80 )
      {
        FT_UInt  gindex;


        /* count supplements */
        if ( FT_READ_BYTE( count ) )
          goto Exit;

        for ( j = 0; j < count; j++ )
        {
          /* Read supplemental glyph code. */
          if ( FT_READ_BYTE( glyph_code ) )
            goto Exit;

          /* Read the SID associated with this glyph code. */
          if ( FT_READ_USHORT( glyph_sid ) )
            goto Exit;

          /* Assign code to SID mapping. */
          encoding->sids[glyph_code] = glyph_sid;

          /* First, lookup GID which has been assigned to */
          /* SID glyph_sid.                               */
          for ( gindex = 0; gindex < num_glyphs; gindex++ )
          {
            if ( charset->sids[gindex] == glyph_sid )
            {
              encoding->codes[glyph_code] = (FT_UShort) gindex;
              break;
            }
          }
        }
      }
    }
    else
    {
      FT_UInt i;


      /* We take into account the fact a CFF font can use a predefined  */
      /* encoding without containing all of the glyphs encoded by this  */
      /* encoding (see the note at the end of section 12 in the CFF     */
      /* specification).                                                */

      switch ( (FT_UInt)offset )
      {
      case 0:
        /* First, copy the code to SID mapping. */
        FT_MEM_COPY( encoding->sids, cff_standard_encoding,
                     256 * sizeof ( FT_UShort ) );

        goto Populate;

      case 1:
        /* First, copy the code to SID mapping. */
        FT_MEM_COPY( encoding->sids, cff_expert_encoding,
                     256 * sizeof ( FT_UShort ) );

      Populate:
        /* Construct code to GID mapping from code to SID mapping */
        /* and charset.                                           */

        encoding->count = 0;


        for ( j = 0; j < 256; j++ )
        {
          /* If j is encoded, find the GID for it. */
          if ( encoding->sids[j] )
          {
            for ( i = 1; i < num_glyphs; i++ )
              /* We matched, so break. */
              if ( charset->sids[i] == encoding->sids[j] )
                break;

            /* i will be equal to num_glyphs if we exited the above */
            /* loop without a match.  In this case, we also have to */
            /* fix the code to SID mapping.                         */
            if ( i == num_glyphs )
            {
              encoding->codes[j] = 0;
              encoding->sids [j] = 0;
            }
            else
            {
              encoding->codes[j] = (FT_UShort)i;

              /* update encoding count */
              if ( encoding->count < j+1 )
                encoding->count = j+1;
            }
          }
        }
        break;

      default:
        FT_ERROR(( "cff_encoding_load: invalid table format!\n" ));
        error = CFF_Err_Invalid_File_Format;
        goto Exit;
      }
    }

  Exit:

    /* Clean up if there was an error. */
    return error;
  }


  static FT_Error
  cff_subfont_load( CFF_SubFont  font,
                    CFF_Index    idx,
                    FT_UInt      font_index,
                    FT_Stream    stream,
                    FT_ULong     base_offset )
  {
    FT_Error         error;
    CFF_ParserRec    parser;
    FT_Byte*         dict;
    FT_ULong         dict_len;
    CFF_FontRecDict  top  = &font->font_dict;
    CFF_Private      priv = &font->private_dict;


    cff_parser_init( &parser, CFF_CODE_TOPDICT, &font->font_dict );

    /* set defaults */
    FT_MEM_ZERO( top, sizeof ( *top ) );

    top->underline_position  = -100;
    top->underline_thickness = 50;
    top->charstring_type     = 2;
    top->font_matrix.xx      = 0x10000L;
    top->font_matrix.yy      = 0x10000L;
    top->cid_count           = 8720;

    error = cff_index_access_element( idx, font_index, &dict, &dict_len ) ||
            cff_parser_run( &parser, dict, dict + dict_len );

    cff_index_forget_element( idx, &dict );

    if ( error )
      goto Exit;

    /* if it is a CID font, we stop there */
    if ( top->cid_registry )
      goto Exit;

    /* parse the private dictionary, if any */
    if ( top->private_offset && top->private_size )
    {
      /* set defaults */
      FT_MEM_ZERO( priv, sizeof ( *priv ) );

      priv->blue_shift       = 7;
      priv->blue_fuzz        = 1;
      priv->lenIV            = -1;
      priv->expansion_factor = (FT_Fixed)0.06 * 0x10000L;
      priv->blue_scale       = (FT_Fixed)0.039625 * 0x10000L;

      cff_parser_init( &parser, CFF_CODE_PRIVATE, priv );

      if ( FT_STREAM_SEEK( base_offset + font->font_dict.private_offset ) ||
           FT_FRAME_ENTER( font->font_dict.private_size )                 )
        goto Exit;

      error = cff_parser_run( &parser,
                              (FT_Byte*)stream->cursor,
                              (FT_Byte*)stream->limit );
      FT_FRAME_EXIT();
      if ( error )
        goto Exit;
    }

    /* read the local subrs, if any */
    if ( priv->local_subrs_offset )
    {
      if ( FT_STREAM_SEEK( base_offset + top->private_offset +
                           priv->local_subrs_offset ) )
        goto Exit;

      error = cff_new_index( &font->local_subrs_index, stream, 1 );
      if ( error )
        goto Exit;

      font->num_local_subrs = font->local_subrs_index.count;
      error = cff_index_get_pointers( &font->local_subrs_index,
                                      &font->local_subrs );
      if ( error )
        goto Exit;
    }

  Exit:
    return error;
  }


  static void
  cff_subfont_done( FT_Memory    memory,
                    CFF_SubFont  subfont )
  {
    if ( subfont )
    {
      cff_done_index( &subfont->local_subrs_index );
      FT_FREE( subfont->local_subrs );
    }
  }


  FT_LOCAL_DEF( FT_Error )
  cff_font_load( FT_Stream  stream,
                 FT_Int     face_index,
                 CFF_Font   font )
  {
    static const FT_Frame_Field  cff_header_fields[] =
    {
#undef  FT_STRUCTURE
#define FT_STRUCTURE  CFF_FontRec

      FT_FRAME_START( 4 ),
        FT_FRAME_BYTE( version_major ),
        FT_FRAME_BYTE( version_minor ),
        FT_FRAME_BYTE( header_size ),
        FT_FRAME_BYTE( absolute_offsize ),
      FT_FRAME_END
    };

    FT_Error         error;
    FT_Memory        memory = stream->memory;
    FT_ULong         base_offset;
    CFF_FontRecDict  dict;

    FT_ZERO( font );

    font->stream = stream;
    font->memory = memory;
    dict         = &font->top_font.font_dict;
    base_offset  = FT_STREAM_POS();

    /* read CFF font header */
    if ( FT_STREAM_READ_FIELDS( cff_header_fields, font ) )
      goto Exit;

    /* check format */
    if ( font->version_major   != 1 ||
         font->header_size      < 4 ||
         font->absolute_offsize > 4 )
    {
      FT_TRACE2(( "[not a CFF font header!]\n" ));
      error = CFF_Err_Unknown_File_Format;
      goto Exit;
    }

    /* skip the rest of the header */
    if ( FT_STREAM_SKIP( font->header_size - 4 ) )
      goto Exit;

    /* read the name, top dict, string and global subrs index */
    if ( FT_SET_ERROR( cff_new_index( &font->name_index,         stream, 0 )) ||
         FT_SET_ERROR( cff_new_index( &font->font_dict_index,    stream, 0 )) ||
         FT_SET_ERROR( cff_new_index( &font->string_index,       stream, 0 )) ||
         FT_SET_ERROR( cff_new_index( &font->global_subrs_index, stream, 1 )) )
      goto Exit;

    /* well, we don't really forget the `disabled' fonts... */
    font->num_faces = font->name_index.count;
    if ( face_index >= (FT_Int)font->num_faces )
    {
      FT_ERROR(( "cff_font_load: incorrect face index = %d\n",
                 face_index ));
      error = CFF_Err_Invalid_Argument;
    }

    /* in case of a font format check, simply exit now */
    if ( face_index < 0 )
      goto Exit;

    /* now, parse the top-level font dictionary */
    error = cff_subfont_load( &font->top_font,
                              &font->font_dict_index,
                              face_index,
                              stream,
                              base_offset );
    if ( error )
      goto Exit;

    /* now, check for a CID font */
    if ( dict->cid_registry )
    {
      CFF_IndexRec  fd_index;
      CFF_SubFont   sub;
      FT_UInt       idx;


      /* this is a CID-keyed font, we must now allocate a table of */
      /* sub-fonts, then load each of them separately              */
      if ( FT_STREAM_SEEK( base_offset + dict->cid_fd_array_offset ) )
        goto Exit;

      error = cff_new_index( &fd_index, stream, 0 );
      if ( error )
        goto Exit;

      if ( fd_index.count > CFF_MAX_CID_FONTS )
      {
        FT_ERROR(( "cff_font_load: FD array too large in CID font\n" ));
        goto Fail_CID;
      }

      /* allocate & read each font dict independently */
      font->num_subfonts = fd_index.count;
      if ( FT_NEW_ARRAY( sub, fd_index.count ) )
        goto Fail_CID;

      /* setup pointer table */
      for ( idx = 0; idx < fd_index.count; idx++ )
        font->subfonts[idx] = sub + idx;

      /* now load each sub font independently */
      for ( idx = 0; idx < fd_index.count; idx++ )
      {
        sub = font->subfonts[idx];
        error = cff_subfont_load( sub, &fd_index, idx,
                                  stream, base_offset );
        if ( error )
          goto Fail_CID;
      }

      /* now load the FD Select array */
      error = CFF_Load_FD_Select( &font->fd_select,
                                  (FT_UInt)dict->cid_count,
                                  stream,
                                  base_offset + dict->cid_fd_select_offset );

    Fail_CID:
      cff_done_index( &fd_index );

      if ( error )
        goto Exit;
    }
    else
      font->num_subfonts = 0;

    /* read the charstrings index now */
    if ( dict->charstrings_offset == 0 )
    {
      FT_ERROR(( "cff_font_load: no charstrings offset!\n" ));
      error = CFF_Err_Unknown_File_Format;
      goto Exit;
    }

    if ( FT_STREAM_SEEK( base_offset + dict->charstrings_offset ) )
      goto Exit;

    error = cff_new_index( &font->charstrings_index, stream, 0 );
    if ( error )
      goto Exit;

    /* explicit the global subrs */
    font->num_global_subrs = font->global_subrs_index.count;
    font->num_glyphs       = font->charstrings_index.count;

    error = cff_index_get_pointers( &font->global_subrs_index,
                                    &font->global_subrs ) ;

    if ( error )
      goto Exit;

    /* read the Charset and Encoding tables when available */
    if ( font->num_glyphs > 0 )
    {
      error = cff_charset_load( &font->charset, font->num_glyphs, stream,
                              base_offset, dict->charset_offset );
      if ( error )
        goto Exit;

      error = cff_encoding_load( &font->encoding,
                                 &font->charset,
                                 font->num_glyphs,
                                 stream,
                                 base_offset,
                                 dict->encoding_offset );
      if ( error )
        goto Exit;
    }

    /* get the font name */
    font->font_name = cff_index_get_name( &font->name_index, face_index );

  Exit:
    return error;
  }


  FT_LOCAL_DEF( void )
  cff_font_done( CFF_Font  font )
  {
    FT_Memory  memory = font->memory;
    FT_UInt    idx;


    cff_done_index( &font->global_subrs_index );
    cff_done_index( &font->string_index );
    cff_done_index( &font->font_dict_index );
    cff_done_index( &font->name_index );
    cff_done_index( &font->charstrings_index );

    /* release font dictionaries, but only if working with */
    /* a CID keyed CFF font                                */
    if ( font->num_subfonts > 0 )
    {
      for ( idx = 0; idx < font->num_subfonts; idx++ )
        cff_subfont_done( memory, font->subfonts[idx] );

      FT_FREE( font->subfonts );
    }

    cff_encoding_done( &font->encoding );
    cff_charset_done( &font->charset, font->stream );

    cff_subfont_done( memory, &font->top_font );

    CFF_Done_FD_Select( &font->fd_select, font->stream );

    FT_FREE( font->global_subrs );
    FT_FREE( font->font_name );
  }


/* END */
