/***************************************************************************/
/*                                                                         */
/*  ahglyph.h                                                              */
/*                                                                         */
/*    Routines used to load and analyze a given glyph before hinting       */
/*    (specification).                                                     */
/*                                                                         */
/*  Copyright 2000-2001, 2002 Catharon Productions Inc.                    */
/*  Author: David Turner                                                   */
/*                                                                         */
/*  This file is part of the Catharon Typography Project and shall only    */
/*  be used, modified, and distributed under the terms of the Catharon     */
/*  Open Source License that should come with this file under the name     */
/*  `CatharonLicense.txt'.  By continuing to use, modify, or distribute    */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/*  Note that this license is compatible with the FreeType license.        */
/*                                                                         */
/***************************************************************************/


#ifndef __AHGLYPH_H__
#define __AHGLYPH_H__


#include <ft2build.h>
#include "ahtypes.h"


FT_BEGIN_HEADER


  typedef enum  AH_UV_
  {
    AH_UV_FXY,
    AH_UV_FYX,
    AH_UV_OXY,
    AH_UV_OYX,
    AH_UV_OX,
    AH_UV_OY,
    AH_UV_YX,
    AH_UV_XY  /* should always be last! */

  } AH_UV;


  FT_LOCAL( void )
  ah_setup_uv( AH_Outline  outline,
               AH_UV       source );


  /* AH_OutlineRec functions - they should be typically called in this order */

  FT_LOCAL( FT_Error )
  ah_outline_new( FT_Memory    memory,
                  AH_Outline*  aoutline );

  FT_LOCAL( FT_Error )
  ah_outline_load( AH_Outline  outline,
                   FT_Fixed    x_scale,
                   FT_Fixed    y_scale,
                   FT_Face     face );

  FT_LOCAL( void )
  ah_outline_compute_segments( AH_Outline  outline );

  FT_LOCAL( void )
  ah_outline_link_segments( AH_Outline  outline );

  FT_LOCAL( void )
  ah_outline_detect_features( AH_Outline  outline );

  FT_LOCAL( void )
  ah_outline_compute_blue_edges( AH_Outline       outline,
                                 AH_Face_Globals  globals );

  FT_LOCAL( void )
  ah_outline_scale_blue_edges( AH_Outline       outline,
                               AH_Face_Globals  globals );

  FT_LOCAL( void )
  ah_outline_save( AH_Outline  outline,
                   AH_Loader   loader );

  FT_LOCAL( void )
  ah_outline_done( AH_Outline  outline );


FT_END_HEADER

#endif /* __AHGLYPH_H__ */


/* END */
