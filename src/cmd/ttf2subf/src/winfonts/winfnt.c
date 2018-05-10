/***************************************************************************/
/*                                                                         */
/*  winfnt.c                                                               */
/*                                                                         */
/*    FreeType font driver for Windows FNT/FON files                       */
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
#include FT_WINFONTS_H
#include FT_INTERNAL_DEBUG_H
#include FT_INTERNAL_STREAM_H
#include FT_INTERNAL_OBJECTS_H
#include FT_INTERNAL_FNT_TYPES_H

#include "winfnt.h"

#include "fnterrs.h"


  /*************************************************************************/
  /*                                                                       */
  /* The macro FT_COMPONENT is used in trace mode.  It is an implicit      */
  /* parameter of the FT_TRACE() and FT_ERROR() macros, used to print/log  */
  /* messages during execution.                                            */
  /*                                                                       */
#undef  FT_COMPONENT
#define FT_COMPONENT  trace_winfnt


  static
  const FT_Frame_Field  winmz_header_fields[] =
  {
#undef  FT_STRUCTURE
#define FT_STRUCTURE  WinMZ_HeaderRec

    FT_FRAME_START( 64 ),
      FT_FRAME_USHORT_LE ( magic ),
      FT_FRAME_SKIP_BYTES( 29 * 2 ),
      FT_FRAME_ULONG_LE  ( lfanew ),
    FT_FRAME_END
  };

  static
  const FT_Frame_Field  winne_header_fields[] =
  {
#undef  FT_STRUCTURE
#define FT_STRUCTURE  WinNE_HeaderRec

    FT_FRAME_START( 40 ),
      FT_FRAME_USHORT_LE ( magic ),
      FT_FRAME_SKIP_BYTES( 34 ),
      FT_FRAME_USHORT_LE ( resource_tab_offset ),
      FT_FRAME_USHORT_LE ( rname_tab_offset ),
    FT_FRAME_END
  };

  static
  const FT_Frame_Field  winfnt_header_fields[] =
  {
#undef  FT_STRUCTURE
#define FT_STRUCTURE  FT_WinFNT_HeaderRec

    FT_FRAME_START( 146 ),
      FT_FRAME_USHORT_LE( version ),
      FT_FRAME_ULONG_LE ( file_size ),
      FT_FRAME_BYTES    ( copyright, 60 ),
      FT_FRAME_USHORT_LE( file_type ),
      FT_FRAME_USHORT_LE( nominal_point_size ),
      FT_FRAME_USHORT_LE( vertical_resolution ),
      FT_FRAME_USHORT_LE( horizontal_resolution ),
      FT_FRAME_USHORT_LE( ascent ),
      FT_FRAME_USHORT_LE( internal_leading ),
      FT_FRAME_USHORT_LE( external_leading ),
      FT_FRAME_BYTE     ( italic ),
      FT_FRAME_BYTE     ( underline ),
      FT_FRAME_BYTE     ( strike_out ),
      FT_FRAME_USHORT_LE( weight ),
      FT_FRAME_BYTE     ( charset ),
      FT_FRAME_USHORT_LE( pixel_width ),
      FT_FRAME_USHORT_LE( pixel_height ),
      FT_FRAME_BYTE     ( pitch_and_family ),
      FT_FRAME_USHORT_LE( avg_width ),
      FT_FRAME_USHORT_LE( max_width ),
      FT_FRAME_BYTE     ( first_char ),
      FT_FRAME_BYTE     ( last_char ),
      FT_FRAME_BYTE     ( default_char ),
      FT_FRAME_BYTE     ( break_char ),
      FT_FRAME_USHORT_LE( bytes_per_row ),
      FT_FRAME_ULONG_LE ( device_offset ),
      FT_FRAME_ULONG_LE ( face_name_offset ),
      FT_FRAME_ULONG_LE ( bits_pointer ),
      FT_FRAME_ULONG_LE ( bits_offset ),
      FT_FRAME_BYTE     ( reserved ),
      FT_FRAME_ULONG_LE ( flags ),
      FT_FRAME_USHORT_LE( A_space ),
      FT_FRAME_USHORT_LE( B_space ),
      FT_FRAME_USHORT_LE( C_space ),
      FT_FRAME_USHORT_LE( color_table_offset ),
      FT_FRAME_BYTES    ( reserved1, 16 ),
    FT_FRAME_END
  };


  static void
  fnt_font_done( FNT_Font   font,
                 FT_Stream  stream )
  {
    if ( font->fnt_frame )
      FT_FRAME_RELEASE( font->fnt_frame );

    font->fnt_size  = 0;
    font->fnt_frame = 0;
  }


  static FT_Error
  fnt_font_load( FNT_Font   font,
                 FT_Stream  stream )
  {
    FT_Error          error;
    FT_WinFNT_Header  header = &font->header;


    /* first of all, read the FNT header */
    if ( FT_STREAM_SEEK( font->offset )                   ||
         FT_STREAM_READ_FIELDS( winfnt_header_fields, header ) )
      goto Exit;

    /* check header */
    if ( header->version != 0x200 &&
         header->version != 0x300 )
    {
      FT_TRACE2(( "[not a valid FNT file]\n" ));
      error = FNT_Err_Unknown_File_Format;
      goto Exit;
    }

    /* Version 2 doesn't have these fields */
    if ( header->version == 0x200 )
    {
      header->flags   = 0;
      header->A_space = 0;
      header->B_space = 0;
      header->C_space = 0;

      header->color_table_offset = 0;
    }

    if ( header->file_type & 1 )
    {
      FT_TRACE2(( "[can't handle vector FNT fonts]\n" ));
      error = FNT_Err_Unknown_File_Format;
      goto Exit;
    }

    /* small fixup -- some fonts have the `pixel_width' field set to 0 */
    if ( header->pixel_width == 0 )
      header->pixel_width = header->pixel_height;

    /* this is a FNT file/table, we now extract its frame */
    if ( FT_STREAM_SEEK( font->offset )                         ||
         FT_FRAME_EXTRACT( header->file_size, font->fnt_frame ) )
      goto Exit;

  Exit:
    return error;
  }


  static void
  fnt_face_done_fonts( FNT_Face  face )
  {
    FT_Memory  memory = FT_FACE( face )->memory;
    FT_Stream  stream = FT_FACE( face )->stream;
    FNT_Font   cur    = face->fonts;
    FNT_Font   limit  = cur + face->num_fonts;


    for ( ; cur < limit; cur++ )
      fnt_font_done( cur, stream );

    FT_FREE( face->fonts );
    face->num_fonts = 0;
  }


  static FT_Error
  fnt_face_get_dll_fonts( FNT_Face  face )
  {
    FT_Error         error;
    FT_Stream        stream = FT_FACE( face )->stream;
    FT_Memory        memory = FT_FACE( face )->memory;
    WinMZ_HeaderRec  mz_header;


    face->fonts     = 0;
    face->num_fonts = 0;

    /* does it begin with a MZ header? */
    if ( FT_STREAM_SEEK( 0 )                                 ||
         FT_STREAM_READ_FIELDS( winmz_header_fields, &mz_header ) )
      goto Exit;

    error = FNT_Err_Unknown_File_Format;
    if ( mz_header.magic == WINFNT_MZ_MAGIC )
    {
      /* yes, now look for a NE header in the file */
      WinNE_HeaderRec  ne_header;


      if ( FT_STREAM_SEEK( mz_header.lfanew )                  ||
           FT_STREAM_READ_FIELDS( winne_header_fields, &ne_header ) )
        goto Exit;

      error = FNT_Err_Unknown_File_Format;
      if ( ne_header.magic == WINFNT_NE_MAGIC )
      {
        /* good, now look in the resource table for each FNT resource */
        FT_ULong   res_offset = mz_header.lfanew +
                                ne_header.resource_tab_offset;

        FT_UShort  size_shift;
        FT_UShort  font_count  = 0;
        FT_ULong   font_offset = 0;


        if ( FT_STREAM_SEEK( res_offset ) ||
             FT_FRAME_ENTER( ne_header.rname_tab_offset -
                             ne_header.resource_tab_offset ) )
          goto Exit;

        size_shift = FT_GET_USHORT_LE();

        for (;;)
        {
          FT_UShort  type_id, count;


          type_id = FT_GET_USHORT_LE();
          if ( !type_id )
            break;

          count = FT_GET_USHORT_LE();

          if ( type_id == 0x8008 )
          {
            font_count  = count;
            font_offset = (FT_ULong)( FT_STREAM_POS() + 4 +
                                      ( stream->cursor - stream->limit ) );
            break;
          }

          stream->cursor += 4 + count * 12;
        }
        FT_FRAME_EXIT();

        if ( !font_count || !font_offset )
        {
          FT_TRACE2(( "this file doesn't contain any FNT resources!\n" ));
          error = FNT_Err_Unknown_File_Format;
          goto Exit;
        }

        if ( FT_STREAM_SEEK( font_offset )           ||
             FT_NEW_ARRAY( face->fonts, font_count ) )
          goto Exit;

        face->num_fonts = font_count;

        if ( FT_FRAME_ENTER( (FT_Long)font_count * 12 ) )
          goto Exit;

        /* now read the offset and position of each FNT font */
        {
          FNT_Font  cur   = face->fonts;
          FNT_Font  limit = cur + font_count;


          for ( ; cur < limit; cur++ )
          {
            cur->offset     = (FT_ULong)FT_GET_USHORT_LE() << size_shift;
            cur->fnt_size   = (FT_ULong)FT_GET_USHORT_LE() << size_shift;
            cur->size_shift = size_shift;
            stream->cursor += 8;
          }
        }
        FT_FRAME_EXIT();

        /* finally, try to load each font there */
        {
          FNT_Font  cur   = face->fonts;
          FNT_Font  limit = cur + font_count;


          for ( ; cur < limit; cur++ )
          {
            error = fnt_font_load( cur, stream );
            if ( error )
              goto Fail;
          }
        }
      }
    }

  Fail:
    if ( error )
      fnt_face_done_fonts( face );

  Exit:
    return error;
  }



  typedef struct  FNT_CMapRec_
  {
    FT_CMapRec  cmap;
    FT_UInt32   first;
    FT_UInt32   count;

  } FNT_CMapRec, *FNT_CMap;


  static FT_Error
  fnt_cmap_init( FNT_CMap  cmap )
  {
    FNT_Face  face = (FNT_Face)FT_CMAP_FACE( cmap );
    FNT_Font  font = face->fonts;


    cmap->first = (FT_UInt32)  font->header.first_char;
    cmap->count = (FT_UInt32)( font->header.last_char - cmap->first + 1 );

    return 0;
  }


  static FT_UInt
  fnt_cmap_char_index( FNT_CMap   cmap,
                       FT_UInt32  char_code )
  {
    FT_UInt  gindex = 0;


    char_code -= cmap->first;
    if ( char_code < cmap->count )
      gindex = char_code + 1;

    return gindex;
  }


  static FT_UInt
  fnt_cmap_char_next( FNT_CMap    cmap,
                      FT_UInt32  *pchar_code )
  {
    FT_UInt    gindex = 0;
    FT_UInt32  result = 0;
    FT_UInt32  char_code = *pchar_code + 1;


    if ( char_code <= cmap->first )
    {
      result = cmap->first;
      gindex = 1;
    }
    else
    {
      char_code -= cmap->first;
      if ( char_code < cmap->count )
      {
        result = cmap->first + char_code;
        gindex = char_code + 1;
      }
    }

    *pchar_code = result;
    return gindex;
  }


  static FT_CMap_ClassRec  fnt_cmap_class_rec =
  {
    sizeof ( FNT_CMapRec ),

    (FT_CMap_InitFunc)     fnt_cmap_init,
    (FT_CMap_DoneFunc)     NULL,
    (FT_CMap_CharIndexFunc)fnt_cmap_char_index,
    (FT_CMap_CharNextFunc) fnt_cmap_char_next
  };

  static FT_CMap_Class  fnt_cmap_class = &fnt_cmap_class_rec;



  static void
  FNT_Face_Done( FNT_Face  face )
  {
    FT_Memory  memory = FT_FACE_MEMORY( face );


    fnt_face_done_fonts( face );

    FT_FREE( face->root.available_sizes );
    face->root.num_fixed_sizes = 0;
  }


  static FT_Error
  FNT_Face_Init( FT_Stream      stream,
                 FNT_Face       face,
                 FT_Int         face_index,
                 FT_Int         num_params,
                 FT_Parameter*  params )
  {
    FT_Error   error;
    FT_Memory  memory = FT_FACE_MEMORY( face );

    FT_UNUSED( num_params );
    FT_UNUSED( params );
    FT_UNUSED( face_index );


    /* try to load several fonts from a DLL */
    error = fnt_face_get_dll_fonts( face );
    if ( error )
    {
      /* this didn't work, now try to load a single FNT font */
      FNT_Font  font;


      if ( FT_NEW( face->fonts ) )
        goto Exit;

      face->num_fonts = 1;
      font            = face->fonts;

      font->offset   = 0;
      font->fnt_size = stream->size;

      error = fnt_font_load( font, stream );
      if ( error )
        goto Fail;
    }

    /* all right, one or more fonts were loaded; we now need to */
    /* fill the root FT_Face fields with relevant information   */
    {
      FT_Face   root  = FT_FACE( face );
      FNT_Font  fonts = face->fonts;
      FNT_Font  limit = fonts + face->num_fonts;
      FNT_Font  cur;


      root->num_faces  = 1;
      root->face_flags = FT_FACE_FLAG_FIXED_SIZES |
                         FT_FACE_FLAG_HORIZONTAL;

      if ( fonts->header.avg_width == fonts->header.max_width )
        root->face_flags |= FT_FACE_FLAG_FIXED_WIDTH;

      if ( fonts->header.italic )
        root->style_flags |= FT_STYLE_FLAG_ITALIC;

      if ( fonts->header.weight >= 800 )
        root->style_flags |= FT_STYLE_FLAG_BOLD;

      /* Setup the `fixed_sizes' array */
      if ( FT_NEW_ARRAY( root->available_sizes, face->num_fonts ) )
        goto Fail;

      root->num_fixed_sizes = face->num_fonts;

      {
        FT_Bitmap_Size*  size = root->available_sizes;


        for ( cur = fonts; cur < limit; cur++, size++ )
        {
          size->width  = cur->header.pixel_width;
          size->height = cur->header.pixel_height;
        }
      }

      {
        FT_CharMapRec  charmap;

        charmap.encoding    = FT_ENCODING_UNICODE;
        charmap.platform_id = 3;
        charmap.encoding_id = 1;
        charmap.face        = root;

        error = FT_CMap_New( fnt_cmap_class,
                             NULL,
                             &charmap,
                             NULL );
        if ( error )
          goto Fail;

        /* Select default charmap */
        if ( root->num_charmaps )
          root->charmap = root->charmaps[0];
      }

      /* setup remaining flags */
      root->num_glyphs = fonts->header.last_char -
                         fonts->header.first_char + 1;

      root->family_name = (FT_String*)fonts->fnt_frame +
                          fonts->header.face_name_offset;
      root->style_name  = (char *)"Regular";

      if ( root->style_flags & FT_STYLE_FLAG_BOLD )
      {
        if ( root->style_flags & FT_STYLE_FLAG_ITALIC )
          root->style_name = (char *)"Bold Italic";
        else
          root->style_name = (char *)"Bold";
      }
      else if ( root->style_flags & FT_STYLE_FLAG_ITALIC )
        root->style_name = (char *)"Italic";
    }

  Fail:
    if ( error )
      FNT_Face_Done( face );

  Exit:
    return error;
  }


  static FT_Error
  FNT_Size_Set_Pixels( FNT_Size  size )
  {
    /* look up a font corresponding to the current pixel size */
    FNT_Face  face  = (FNT_Face)FT_SIZE_FACE( size );
    FNT_Font  cur   = face->fonts;
    FNT_Font  limit = cur + face->num_fonts;


    size->font = 0;
    for ( ; cur < limit; cur++ )
    {
      /* we only compare the character height, as fonts used some strange */
      /* values                                                           */
      if ( cur->header.pixel_height == size->root.metrics.y_ppem )
      {
        size->font = cur;

        size->root.metrics.ascender  = cur->header.ascent * 64;
        size->root.metrics.descender = ( cur->header.pixel_height -
                                           cur->header.ascent ) * 64;
        size->root.metrics.height    = cur->header.pixel_height * 64;
        break;
      }
    }

    return ( size->font ? FNT_Err_Ok : FNT_Err_Invalid_Pixel_Size );
  }


  static FT_Error
  FNT_Load_Glyph( FT_GlyphSlot  slot,
                  FNT_Size      size,
                  FT_UInt       glyph_index,
                  FT_Int32      load_flags )
  {
    FNT_Font    font  = size->font;
    FT_Error    error = 0;
    FT_Byte*    p;
    FT_Int      len;
    FT_Bitmap*  bitmap = &slot->bitmap;
    FT_ULong    offset;
    FT_Bool     new_format;

    FT_UNUSED( slot );
    FT_UNUSED( load_flags );


    if ( !font )
    {
      error = FNT_Err_Invalid_Argument;
      goto Exit;
    }

    if ( glyph_index > 0 )
      glyph_index--;
    else
      glyph_index = font->header.default_char - font->header.first_char;

    new_format = FT_BOOL( font->header.version == 0x300 );
    len        = new_format ? 6 : 4;

    /* jump to glyph entry */
    p = font->fnt_frame + ( new_format ? 146 : 118 ) + len * glyph_index;

    bitmap->width = FT_NEXT_SHORT_LE( p );

    if ( new_format )
      offset = FT_NEXT_ULONG_LE( p );
    else
      offset = FT_NEXT_USHORT_LE( p );

    /* jump to glyph data */
    p = font->fnt_frame + /* font->header.bits_offset */ + offset;

    /* allocate and build bitmap */
    {
      FT_Int     pitch  = ( bitmap->width + 7 ) >> 3;


      bitmap->pitch      = pitch;
      bitmap->rows       = font->header.pixel_height;
      bitmap->pixel_mode = FT_PIXEL_MODE_MONO;

      /* note: we don't allocate a new buffer for the bitmap since we */
      /*       already store the images in the FT_Face                */
      ft_glyphslot_set_bitmap( slot, p );
    }

    slot->bitmap_left = 0;
    slot->bitmap_top  = font->header.ascent;
    slot->format      = FT_GLYPH_FORMAT_BITMAP;

    /* now set up metrics */
    slot->metrics.horiAdvance  = bitmap->width << 6;
    slot->metrics.horiBearingX = 0;
    slot->metrics.horiBearingY = slot->bitmap_top << 6;

    slot->linearHoriAdvance    = (FT_Fixed)bitmap->width << 16;
    slot->format               = FT_GLYPH_FORMAT_BITMAP;

  Exit:
    return error;
  }


  FT_CALLBACK_TABLE_DEF
  const FT_Driver_ClassRec  winfnt_driver_class =
  {
    {
      ft_module_font_driver,
      sizeof ( FT_DriverRec ),

      "winfonts",
      0x10000L,
      0x20000L,

      0,

      (FT_Module_Constructor)0,
      (FT_Module_Destructor) 0,
      (FT_Module_Requester)  0
    },

    sizeof( FNT_FaceRec ),
    sizeof( FNT_SizeRec ),
    sizeof( FT_GlyphSlotRec ),

    (FT_Face_InitFunc)        FNT_Face_Init,
    (FT_Face_DoneFunc)        FNT_Face_Done,
    (FT_Size_InitFunc)        0,
    (FT_Size_DoneFunc)        0,
    (FT_Slot_InitFunc)        0,
    (FT_Slot_DoneFunc)        0,

    (FT_Size_ResetPointsFunc) FNT_Size_Set_Pixels,
    (FT_Size_ResetPixelsFunc) FNT_Size_Set_Pixels,
    (FT_Slot_LoadFunc)        FNT_Load_Glyph,

    (FT_Face_GetKerningFunc)  0,
    (FT_Face_AttachFunc)      0,
    (FT_Face_GetAdvancesFunc) 0
  };


/* END */
