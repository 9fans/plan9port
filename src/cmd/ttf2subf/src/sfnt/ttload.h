/***************************************************************************/
/*                                                                         */
/*  ttload.h                                                               */
/*                                                                         */
/*    Load the basic TrueType tables, i.e., tables that can be either in   */
/*    TTF or OTF fonts (specification).                                    */
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


#ifndef __TTLOAD_H__
#define __TTLOAD_H__


#include <ft2build.h>
#include FT_INTERNAL_STREAM_H
#include FT_INTERNAL_TRUETYPE_TYPES_H


FT_BEGIN_HEADER


  FT_LOCAL( TT_Table  )
  tt_face_lookup_table( TT_Face   face,
                        FT_ULong  tag );

  FT_LOCAL( FT_Error )
  tt_face_goto_table( TT_Face    face,
                      FT_ULong   tag,
                      FT_Stream  stream,
                      FT_ULong*  length );


  FT_LOCAL( FT_Error )
  tt_face_load_sfnt_header( TT_Face      face,
                            FT_Stream    stream,
                            FT_Long      face_index,
                            SFNT_Header  sfnt );

  FT_LOCAL( FT_Error )
  tt_face_load_directory( TT_Face      face,
                          FT_Stream    stream,
                          SFNT_Header  sfnt );

  FT_LOCAL( FT_Error )
  tt_face_load_any( TT_Face    face,
                    FT_ULong   tag,
                    FT_Long    offset,
                    FT_Byte*   buffer,
                    FT_ULong*  length );


  FT_LOCAL( FT_Error )
  tt_face_load_header( TT_Face    face,
                       FT_Stream  stream );


  FT_LOCAL( FT_Error )
  tt_face_load_metrics_header( TT_Face    face,
                               FT_Stream  stream,
                               FT_Bool    vertical );


  FT_LOCAL( FT_Error )
  tt_face_load_cmap( TT_Face    face,
                     FT_Stream  stream );


  FT_LOCAL( FT_Error )
  tt_face_load_max_profile( TT_Face    face,
                            FT_Stream  stream );


  FT_LOCAL( FT_Error )
  tt_face_load_names( TT_Face    face,
                      FT_Stream  stream );


  FT_LOCAL( FT_Error )
  tt_face_load_os2( TT_Face    face,
                    FT_Stream  stream );


  FT_LOCAL( FT_Error )
  tt_face_load_postscript( TT_Face    face,
                           FT_Stream  stream );


  FT_LOCAL( FT_Error )
  tt_face_load_hdmx( TT_Face    face,
                     FT_Stream  stream );

  FT_LOCAL( FT_Error )
  tt_face_load_pclt( TT_Face    face,
                     FT_Stream  stream );

  FT_LOCAL( void )
  tt_face_free_names( TT_Face  face );


  FT_LOCAL( void )
  tt_face_free_hdmx ( TT_Face  face );


  FT_LOCAL( FT_Error )
  tt_face_load_kern( TT_Face    face,
                     FT_Stream  stream );


  FT_LOCAL( FT_Error )
  tt_face_load_gasp( TT_Face    face,
                     FT_Stream  stream );

#ifdef TT_CONFIG_OPTION_EMBEDDED_BITMAPS

  FT_LOCAL( FT_Error )
  tt_face_load_bitmap_header( TT_Face    face,
                              FT_Stream  stream );

#endif /* TT_CONFIG_OPTION_EMBEDDED_BITMAPS */


FT_END_HEADER

#endif /* __TTLOAD_H__ */


/* END */
