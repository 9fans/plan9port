/***************************************************************************/
/*                                                                         */
/*  ttcmap0.h                                                              */
/*                                                                         */
/*    TrueType new character mapping table (cmap) support (specification). */
/*                                                                         */
/*  Copyright 2002 by                                                      */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/


#ifndef __TTCMAP0_H__
#define __TTCMAP0_H__


#include <ft2build.h>
#include FT_INTERNAL_TRUETYPE_TYPES_H
#include FT_INTERNAL_OBJECTS_H


FT_BEGIN_HEADER

  typedef struct  TT_CMapRec_
  {
    FT_CMapRec  cmap;
    FT_Byte*    data;           /* pointer to in-memory cmap table */

  } TT_CMapRec, *TT_CMap;

  typedef const struct TT_CMap_ClassRec_*  TT_CMap_Class;


  typedef FT_Error
  (*TT_CMap_ValidateFunc)( FT_Byte*      data,
                           FT_Validator  valid );

  typedef struct  TT_CMap_ClassRec_
  {
    FT_CMap_ClassRec      clazz;
    FT_UInt               format;
    TT_CMap_ValidateFunc  validate;

  } TT_CMap_ClassRec;


  typedef struct  TT_ValidatorRec_
  {
    FT_ValidatorRec  validator;
    FT_UInt          num_glyphs;

  } TT_ValidatorRec, *TT_Validator;


#define TT_VALIDATOR( x )          ((TT_Validator)( x ))
#define TT_VALID_GLYPH_COUNT( x )  TT_VALIDATOR( x )->num_glyphs


  FT_LOCAL( FT_Error )
  tt_face_build_cmaps( TT_Face  face );


FT_END_HEADER

#endif /* __TTCMAP0_H__ */


/* END */
