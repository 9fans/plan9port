/***************************************************************************/
/*                                                                         */
/*  t1objs.c                                                               */
/*                                                                         */
/*    Type 1 objects manager (body).                                       */
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
#include FT_INTERNAL_STREAM_H

#include "t1gload.h"
#include "t1load.h"

#include "t1errors.h"

#ifndef T1_CONFIG_OPTION_NO_AFM
#include "t1afm.h"
#endif

#include FT_INTERNAL_POSTSCRIPT_NAMES_H
#include FT_INTERNAL_POSTSCRIPT_AUX_H


  /*************************************************************************/
  /*                                                                       */
  /* The macro FT_COMPONENT is used in trace mode.  It is an implicit      */
  /* parameter of the FT_TRACE() and FT_ERROR() macros, used to print/log  */
  /* messages during execution.                                            */
  /*                                                                       */
#undef  FT_COMPONENT
#define FT_COMPONENT  trace_t1objs


  /*************************************************************************/
  /*                                                                       */
  /*                            SIZE FUNCTIONS                             */
  /*                                                                       */
  /*  note that we store the global hints in the size's "internal" root    */
  /*  field                                                                */
  /*                                                                       */
  /*************************************************************************/


  static PSH_Globals_Funcs
  T1_Size_Get_Globals_Funcs( T1_Size  size )
  {
    T1_Face           face     = (T1_Face)size->root.face;
    PSHinter_Service  pshinter = (PSHinter_Service)face->pshinter;
    FT_Module         module;


    module = FT_Get_Module( size->root.face->driver->root.library,
                            "pshinter" );
    return ( module && pshinter && pshinter->get_globals_funcs )
           ? pshinter->get_globals_funcs( module )
           : 0 ;
  }


  FT_LOCAL_DEF( void )
  T1_Size_Done( T1_Size  size )
  {
    if ( size->root.internal )
    {
      PSH_Globals_Funcs  funcs;


      funcs = T1_Size_Get_Globals_Funcs( size );
      if ( funcs )
        funcs->destroy( (PSH_Globals)size->root.internal );

      size->root.internal = 0;
    }
  }


  FT_LOCAL_DEF( FT_Error )
  T1_Size_Init( T1_Size  size )
  {
    FT_Error           error = 0;
    PSH_Globals_Funcs  funcs = T1_Size_Get_Globals_Funcs( size );


    if ( funcs )
    {
      PSH_Globals  globals;
      T1_Face      face = (T1_Face)size->root.face;


      error = funcs->create( size->root.face->memory,
                             &face->type1.private_dict, &globals );
      if ( !error )
        size->root.internal = (FT_Size_Internal)(void*)globals;
    }

    return error;
  }


  FT_LOCAL_DEF( FT_Error )
  T1_Size_Reset( T1_Size  size )
  {
    PSH_Globals_Funcs  funcs = T1_Size_Get_Globals_Funcs( size );
    FT_Error           error = 0;


    if ( funcs )
      error = funcs->set_scale( (PSH_Globals)size->root.internal,
                                 size->root.metrics.x_scale,
                                 size->root.metrics.y_scale,
                                 0, 0 );
    return error;
  }


  /*************************************************************************/
  /*                                                                       */
  /*                            SLOT  FUNCTIONS                            */
  /*                                                                       */
  /*************************************************************************/

  FT_LOCAL_DEF( void )
  T1_GlyphSlot_Done( T1_GlyphSlot  slot )
  {
    slot->root.internal->glyph_hints = 0;
  }


  FT_LOCAL_DEF( FT_Error )
  T1_GlyphSlot_Init( T1_GlyphSlot  slot )
  {
    T1_Face           face;
    PSHinter_Service  pshinter;


    face     = (T1_Face)slot->root.face;
    pshinter = (PSHinter_Service)face->pshinter;

    if ( pshinter )
    {
      FT_Module  module;


      module = FT_Get_Module( slot->root.face->driver->root.library, "pshinter" );
      if (module)
      {
        T1_Hints_Funcs  funcs;

        funcs = pshinter->get_t1_funcs( module );
        slot->root.internal->glyph_hints = (void*)funcs;
      }
    }
    return 0;
  }


  /*************************************************************************/
  /*                                                                       */
  /*                            FACE  FUNCTIONS                            */
  /*                                                                       */
  /*************************************************************************/


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    T1_Face_Done                                                       */
  /*                                                                       */
  /* <Description>                                                         */
  /*    The face object destructor.                                        */
  /*                                                                       */
  /* <Input>                                                               */
  /*    face :: A typeless pointer to the face object to destroy.          */
  /*                                                                       */
  FT_LOCAL_DEF( void )
  T1_Face_Done( T1_Face  face )
  {
    FT_Memory  memory;
    T1_Font    type1 = &face->type1;


    if ( face )
    {
      memory = face->root.memory;

#ifndef T1_CONFIG_OPTION_NO_MM_SUPPORT
      /* release multiple masters information */
      T1_Done_Blend( face );
      face->blend = 0;
#endif

      /* release font info strings */
      {
        PS_FontInfo  info = &type1->font_info;


        FT_FREE( info->version );
        FT_FREE( info->notice );
        FT_FREE( info->full_name );
        FT_FREE( info->family_name );
        FT_FREE( info->weight );
      }

      /* release top dictionary */
      FT_FREE( type1->charstrings_len );
      FT_FREE( type1->charstrings );
      FT_FREE( type1->glyph_names );

      FT_FREE( type1->subrs );
      FT_FREE( type1->subrs_len );

      FT_FREE( type1->subrs_block );
      FT_FREE( type1->charstrings_block );
      FT_FREE( type1->glyph_names_block );

      FT_FREE( type1->encoding.char_index );
      FT_FREE( type1->encoding.char_name );
      FT_FREE( type1->font_name );

#ifndef T1_CONFIG_OPTION_NO_AFM
      /* release afm data if present */
      if ( face->afm_data )
        T1_Done_AFM( memory, (T1_AFM*)face->afm_data );
#endif

      /* release unicode map, if any */
      FT_FREE( face->unicode_map.maps );
      face->unicode_map.num_maps = 0;

      face->root.family_name = 0;
      face->root.style_name  = 0;
    }
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    T1_Face_Init                                                       */
  /*                                                                       */
  /* <Description>                                                         */
  /*    The face object constructor.                                       */
  /*                                                                       */
  /* <Input>                                                               */
  /*    stream     ::  input stream where to load font data.               */
  /*                                                                       */
  /*    face_index :: The index of the font face in the resource.          */
  /*                                                                       */
  /*    num_params :: Number of additional generic parameters.  Ignored.   */
  /*                                                                       */
  /*    params     :: Additional generic parameters.  Ignored.             */
  /*                                                                       */
  /* <InOut>                                                               */
  /*    face       :: The face record to build.                            */
  /*                                                                       */
  /* <Return>                                                              */
  /*    FreeType error code.  0 means success.                             */
  /*                                                                       */
  FT_LOCAL_DEF( FT_Error )
  T1_Face_Init( FT_Stream      stream,
                T1_Face        face,
                FT_Int         face_index,
                FT_Int         num_params,
                FT_Parameter*  params )
  {
    FT_Error          error;
    PSNames_Service   psnames;
    PSAux_Service     psaux;
    PSHinter_Service  pshinter;

    FT_UNUSED( num_params );
    FT_UNUSED( params );
    FT_UNUSED( face_index );
    FT_UNUSED( stream );


    face->root.num_faces = 1;

    face->psnames = FT_Get_Module_Interface( FT_FACE_LIBRARY( face ),
                                             "psnames" );
    psnames = (PSNames_Service)face->psnames;

    face->psaux = FT_Get_Module_Interface( FT_FACE_LIBRARY( face ),
                                           "psaux" );
    psaux = (PSAux_Service)face->psaux;

    face->pshinter = FT_Get_Module_Interface( FT_FACE_LIBRARY( face ),
                                              "pshinter" );
    pshinter = (PSHinter_Service)face->pshinter;

    /* open the tokenizer, this will also check the font format */
    error = T1_Open_Face( face );
    if ( error )
      goto Exit;

    /* if we just wanted to check the format, leave successfully now */
    if ( face_index < 0 )
      goto Exit;

    /* check the face index */
    if ( face_index != 0 )
    {
      FT_ERROR(( "T1_Face_Init: invalid face index\n" ));
      error = T1_Err_Invalid_Argument;
      goto Exit;
    }

    /* Now, load the font program into the face object */

    /* Init the face object fields */
    /* Now set up root face fields */
    {
      FT_Face  root = (FT_Face)&face->root;


      root->num_glyphs = face->type1.num_glyphs;
      root->face_index = face_index;

      root->face_flags = FT_FACE_FLAG_SCALABLE;
      root->face_flags |= FT_FACE_FLAG_HORIZONTAL;
      root->face_flags |= FT_FACE_FLAG_GLYPH_NAMES;

      if ( face->type1.font_info.is_fixed_pitch )
        root->face_flags |= FT_FACE_FLAG_FIXED_WIDTH;

      if ( face->blend )
        root->face_flags |= FT_FACE_FLAG_MULTIPLE_MASTERS;

      /* XXX: TODO -- add kerning with .afm support */

      /* get style name -- be careful, some broken fonts only */
      /* have a `/FontName' dictionary entry!                 */
      root->family_name = face->type1.font_info.family_name;
      if ( root->family_name )
      {
        char*  full   = face->type1.font_info.full_name;
        char*  family = root->family_name;


        if ( full )
        {
          while ( *family && *full == *family )
          {
            family++;
            full++;
          }

          root->style_name = ( *full == ' ' ? full + 1
                                            : (char *)"Regular" );
        }
        else
          root->style_name = (char *)"Regular";
      }
      else
      {
        /* do we have a `/FontName'? */
        if ( face->type1.font_name )
        {
          root->family_name = face->type1.font_name;
          root->style_name  = (char *)"Regular";
        }
      }

      /* compute style flags */
      root->style_flags = 0;
      if ( face->type1.font_info.italic_angle )
        root->style_flags |= FT_STYLE_FLAG_ITALIC;
      if ( face->type1.font_info.weight )
      {
        if ( !ft_strcmp( face->type1.font_info.weight, "Bold"  ) ||
             !ft_strcmp( face->type1.font_info.weight, "Black" ) )
          root->style_flags |= FT_STYLE_FLAG_BOLD;
      }

      /* no embedded bitmap support */
      root->num_fixed_sizes = 0;
      root->available_sizes = 0;

      root->bbox.xMin =   face->type1.font_bbox.xMin             >> 16;
      root->bbox.yMin =   face->type1.font_bbox.yMin             >> 16;
      root->bbox.xMax = ( face->type1.font_bbox.xMax + 0xFFFFU ) >> 16;
      root->bbox.yMax = ( face->type1.font_bbox.yMax + 0xFFFFU ) >> 16;

      /* Set units_per_EM if we didn't set it in parse_font_matrix. */
      if ( !root->units_per_EM )
        root->units_per_EM = 1000;

      root->ascender  = (FT_Short)( root->bbox.yMax );
      root->descender = (FT_Short)( root->bbox.yMin );
      root->height    = (FT_Short)(
                          ( ( root->ascender - root->descender ) * 12 ) / 10 );

      /* now compute the maximum advance width */
      root->max_advance_width =
        (FT_Short)( root->bbox.xMax );
      {
        FT_Int  max_advance;


        error = T1_Compute_Max_Advance( face, &max_advance );

        /* in case of error, keep the standard width */
        if ( !error )
          root->max_advance_width = (FT_Short)max_advance;
        else
          error = 0;   /* clear error */
      }

      root->max_advance_height = root->height;

      root->underline_position  = face->type1.font_info.underline_position;
      root->underline_thickness = face->type1.font_info.underline_thickness;

      root->internal->max_points   = 0;
      root->internal->max_contours = 0;
    }

    {
      FT_Face  root = &face->root;


      if ( psnames && psaux )
      {
        FT_CharMapRec    charmap;
        T1_CMap_Classes  cmap_classes = psaux->t1_cmap_classes;
        FT_CMap_Class    clazz;


        charmap.face = root;

        /* first of all, try to synthetize a Unicode charmap */
        charmap.platform_id = 3;
        charmap.encoding_id = 1;
        charmap.encoding    = FT_ENCODING_UNICODE;

        FT_CMap_New( cmap_classes->unicode, NULL, &charmap, NULL );

        /* now, generate an Adobe Standard encoding when appropriate */
        charmap.platform_id = 7;
        clazz               = NULL;

        switch ( face->type1.encoding_type )
        {
        case T1_ENCODING_TYPE_STANDARD:
          charmap.encoding    = FT_ENCODING_ADOBE_STANDARD;
          charmap.encoding_id = 0;
          clazz               = cmap_classes->standard;
          break;

        case T1_ENCODING_TYPE_EXPERT:
          charmap.encoding    = FT_ENCODING_ADOBE_EXPERT;
          charmap.encoding_id = 1;
          clazz               = cmap_classes->expert;
          break;

        case T1_ENCODING_TYPE_ARRAY:
          charmap.encoding    = FT_ENCODING_ADOBE_CUSTOM;
          charmap.encoding_id = 2;
          clazz               = cmap_classes->custom;
          break;

        case T1_ENCODING_TYPE_ISOLATIN1:
          charmap.encoding    = FT_ENCODING_ADOBE_LATIN_1;
          charmap.encoding_id = 3;
          clazz               = cmap_classes->unicode;
          break;

        default:
          ;
        }

        if ( clazz )
          FT_CMap_New( clazz, NULL, &charmap, NULL );

#if 0
        /* Select default charmap */
        if (root->num_charmaps)
          root->charmap = root->charmaps[0];
#endif
      }
    }

  Exit:
    return error;
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    T1_Driver_Init                                                     */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Initializes a given Type 1 driver object.                          */
  /*                                                                       */
  /* <Input>                                                               */
  /*    driver :: A handle to the target driver object.                    */
  /*                                                                       */
  /* <Return>                                                              */
  /*    FreeType error code.  0 means success.                             */
  /*                                                                       */
  FT_LOCAL_DEF( FT_Error )
  T1_Driver_Init( T1_Driver  driver )
  {
    FT_UNUSED( driver );

    return T1_Err_Ok;
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    T1_Driver_Done                                                     */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Finalizes a given Type 1 driver.                                   */
  /*                                                                       */
  /* <Input>                                                               */
  /*    driver :: A handle to the target Type 1 driver.                    */
  /*                                                                       */
  FT_LOCAL_DEF( void )
  T1_Driver_Done( T1_Driver  driver )
  {
    FT_UNUSED( driver );
  }


/* END */
