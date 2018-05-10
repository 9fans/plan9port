/***************************************************************************/
/*                                                                         */
/*  cidparse.c                                                             */
/*                                                                         */
/*    CID-keyed Type1 parser (body).                                       */
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
#include FT_INTERNAL_CALC_H
#include FT_INTERNAL_OBJECTS_H
#include FT_INTERNAL_STREAM_H

#include "cidparse.h"

#include "ciderrs.h"


  /*************************************************************************/
  /*                                                                       */
  /* The macro FT_COMPONENT is used in trace mode.  It is an implicit      */
  /* parameter of the FT_TRACE() and FT_ERROR() macros, used to print/log  */
  /* messages during execution.                                            */
  /*                                                                       */
#undef  FT_COMPONENT
#define FT_COMPONENT  trace_cidparse


  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                    INPUT STREAM PARSER                        *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/


  FT_LOCAL_DEF( FT_Error )
  cid_parser_new( CID_Parser*    parser,
                  FT_Stream      stream,
                  FT_Memory      memory,
                  PSAux_Service  psaux )
  {
    FT_Error  error;
    FT_ULong  base_offset, offset, ps_len;
    FT_Byte   buffer[256 + 10];
    FT_Int    buff_len;


    FT_MEM_ZERO( parser, sizeof ( *parser ) );
    psaux->ps_parser_funcs->init( &parser->root, 0, 0, memory );

    parser->stream = stream;

    base_offset = FT_STREAM_POS();

    /* first of all, check the font format in the  header */
    if ( FT_FRAME_ENTER( 31 ) )
      goto Exit;

    if ( ft_strncmp( (char *)stream->cursor,
                     "%!PS-Adobe-3.0 Resource-CIDFont", 31 ) )
    {
      FT_TRACE2(( "[not a valid CID-keyed font]\n" ));
      error = CID_Err_Unknown_File_Format;
    }

    FT_FRAME_EXIT();
    if ( error )
      goto Exit;

    /* now, read the rest of the file, until we find a `StartData' */
    buff_len = 256;
    for (;;)
    {
      FT_Byte   *p, *limit = buffer + 256;
      FT_ULong  top_position;


      /* fill input buffer */
      buff_len -= 256;
      if ( buff_len > 0 )
        FT_MEM_MOVE( buffer, limit, buff_len );

      p = buffer + buff_len;

      if ( FT_STREAM_READ( p, 256 + 10 - buff_len ) )
        goto Exit;

      top_position = FT_STREAM_POS() - buff_len;
      buff_len = 256 + 10;

      /* look for `StartData' */
      for ( p = buffer; p < limit; p++ )
      {
        if ( p[0] == 'S' && ft_strncmp( (char*)p, "StartData", 9 ) == 0 )
        {
          /* save offset of binary data after `StartData' */
          offset = (FT_ULong)( top_position - ( limit - p ) + 10 );
          goto Found;
        }
      }
    }

  Found:
    /* we have found the start of the binary data.  We will now        */
    /* rewind and extract the frame of corresponding to the Postscript */
    /* section                                                         */

    ps_len = offset - base_offset;
    if ( FT_STREAM_SEEK( base_offset )                    ||
         FT_FRAME_EXTRACT( ps_len, parser->postscript ) )
      goto Exit;

    parser->data_offset    = offset;
    parser->postscript_len = ps_len;
    parser->root.base      = parser->postscript;
    parser->root.cursor    = parser->postscript;
    parser->root.limit     = parser->root.cursor + ps_len;
    parser->num_dict       = -1;

  Exit:
    return error;
  }


  FT_LOCAL_DEF( void )
  cid_parser_done( CID_Parser*  parser )
  {
    /* always free the private dictionary */
    if ( parser->postscript )
    {
      FT_Stream  stream = parser->stream;


      FT_FRAME_RELEASE( parser->postscript );
    }
    parser->root.funcs.done( &parser->root );
  }


/* END */
