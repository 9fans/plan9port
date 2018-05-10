/***************************************************************************/
/*                                                                         */
/*  ftccmap.c                                                              */
/*                                                                         */
/*    FreeType CharMap cache (body)                                        */
/*                                                                         */
/*  Copyright 2000-2001, 2002 by                                           */
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
#include FT_FREETYPE_H
#include FT_CACHE_H
#include FT_CACHE_CHARMAP_H
#include FT_CACHE_MANAGER_H
#include FT_INTERNAL_MEMORY_H
#include FT_INTERNAL_DEBUG_H
#include FT_TRUETYPE_IDS_H

#include "ftcerror.h"

#undef  FT_COMPONENT
#define FT_COMPONENT  trace_cache

  /*************************************************************************/
  /*                                                                       */
  /* Each FTC_CMapNode contains a simple array to map a range of character */
  /* codes to equivalent glyph indices.                                    */
  /*                                                                       */
  /* For now, the implementation is very basic: Each node maps a range of  */
  /* 128 consecutive character codes to their corresponding glyph indices. */
  /*                                                                       */
  /* We could do more complex things, but I don't think it is really very  */
  /* useful.                                                               */
  /*                                                                       */
  /*************************************************************************/


  /* number of glyph indices / character code per node */
#define FTC_CMAP_INDICES_MAX  128


  typedef struct  FTC_CMapNodeRec_
  {
    FTC_NodeRec  node;
    FT_UInt32    first;                         /* first character in node */
    FT_UInt16    indices[FTC_CMAP_INDICES_MAX]; /* array of glyph indices  */

  } FTC_CMapNodeRec, *FTC_CMapNode;


#define FTC_CMAP_NODE( x ) ( (FTC_CMapNode)( x ) )


  /* compute node hash value from cmap family and "requested" glyph index */
#define FTC_CMAP_HASH( cfam, cquery )                                       \
          ( (cfam)->hash + ( (cquery)->char_code / FTC_CMAP_INDICES_MAX ) )

  /* if (indices[n] == FTC_CMAP_UNKNOWN), we assume that the corresponding */
  /* glyph indices haven't been queried through FT_Get_Glyph_Index() yet   */
#define FTC_CMAP_UNKNOWN  ( (FT_UInt16)-1 )


  /* the charmap query */
  typedef struct  FTC_CMapQueryRec_
  {
    FTC_QueryRec  query;
    FTC_CMapDesc  desc;
    FT_UInt32     char_code;

  } FTC_CMapQueryRec, *FTC_CMapQuery;


#define FTC_CMAP_QUERY( x )  ( (FTC_CMapQuery)( x ) )


  /* the charmap family */
  typedef struct FTC_CMapFamilyRec_
  {
    FTC_FamilyRec    family;
    FT_UInt32        hash;
    FTC_CMapDescRec  desc;
    FT_UInt          index;

  } FTC_CMapFamilyRec, *FTC_CMapFamily;


#define FTC_CMAP_FAMILY( x )         ( (FTC_CMapFamily)( x ) )
#define FTC_CMAP_FAMILY_MEMORY( x )  FTC_FAMILY( x )->memory


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                        CHARMAP NODES                          *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  /* no need for specific finalizer; we use "ftc_node_done" directly */

  /* initialize a new cmap node */
  FT_CALLBACK_DEF( FT_Error )
  ftc_cmap_node_init( FTC_CMapNode   cnode,
                      FTC_CMapQuery  cquery,
                      FTC_Cache      cache )
  {
    FT_UInt32  first;
    FT_UInt    n;
    FT_UNUSED( cache );


    first = ( cquery->char_code / FTC_CMAP_INDICES_MAX ) *
            FTC_CMAP_INDICES_MAX;

    cnode->first = first;
    for ( n = 0; n < FTC_CMAP_INDICES_MAX; n++ )
      cnode->indices[n] = FTC_CMAP_UNKNOWN;

    return 0;
  }


  /* compute the weight of a given cmap node */
  FT_CALLBACK_DEF( FT_ULong )
  ftc_cmap_node_weight( FTC_CMapNode  cnode )
  {
    FT_UNUSED( cnode );

    return sizeof ( *cnode );
  }


  /* compare a cmap node to a given query */
  FT_CALLBACK_DEF( FT_Bool )
  ftc_cmap_node_compare( FTC_CMapNode   cnode,
                         FTC_CMapQuery  cquery )
  {
    FT_UInt32  offset = (FT_UInt32)( cquery->char_code - cnode->first );


    return FT_BOOL( offset < FTC_CMAP_INDICES_MAX );
  }


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                    CHARMAP FAMILY                             *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  FT_CALLBACK_DEF( FT_Error )
  ftc_cmap_family_init( FTC_CMapFamily  cfam,
                        FTC_CMapQuery   cquery,
                        FTC_Cache       cache )
  {
    FTC_Manager   manager = cache->manager;
    FTC_CMapDesc  desc = cquery->desc;
    FT_UInt32     hash = 0;
    FT_Error      error;
    FT_Face       face;


    /* setup charmap descriptor */
    cfam->desc = *desc;

    /* let's see whether the rest is correct too */
    error = FTC_Manager_Lookup_Face( manager, desc->face_id, &face );
    if ( !error )
    {
      FT_UInt      count = face->num_charmaps;
      FT_UInt      idx   = count;
      FT_CharMap*  cur   = face->charmaps;


      switch ( desc->type )
      {
      case FTC_CMAP_BY_INDEX:
        idx  = desc->u.index;
        hash = idx * 33;
        break;

      case FTC_CMAP_BY_ENCODING:
        if (desc->u.encoding == FT_ENCODING_UNICODE)
        {
         /* since the `interesting' table, with id's 3,10, is normally the
          * last one, we loop backwards. This looses with type1 fonts with
          * non-BMP characters (<.0001%), this wins with .ttf with non-BMP
          * chars (.01% ?), and this is the same about 99.99% of the time!
          */

          FT_UInt  unicmap_idx = count;  /* some UCS-2 map, if we found it */

          cur += count - 1;

          for ( idx = 0; idx < count; idx++, cur-- )
          {
            if ( cur[0]->encoding == FT_ENCODING_UNICODE )
            {
              unicmap_idx = idx;  /* record we found a Unicode charmap */

             /* XXX If some new encodings to represent UCS-4 are added,
              *     they should be added here.
              */
              if ( ( cur[0]->platform_id == TT_PLATFORM_MICROSOFT &&
                     cur[0]->encoding_id == TT_MS_ID_UCS_4        )          ||
                   ( cur[0]->platform_id == TT_PLATFORM_APPLE_UNICODE &&
                     cur[0]->encoding_id == TT_APPLE_ID_UNICODE_32    )      )

              /* Hurray! We found a UCS-4 charmap. We can stop the scan! */
              {
                idx = count - 1 - idx;
                goto Found_idx_for_FTC_CMAP_BY_ENCODING;
              }
            }
          }

         /* We do not have any UCS-4 charmap. Sigh.
          * Let's see if we have some other kind of Unicode charmap, though.
          */
          if ( unicmap_idx < count )
            idx = count - 1 - unicmap_idx;
        }
        else
        {
          for ( idx = 0; idx < count; idx++, cur++ )
            if ( cur[0]->encoding == desc->u.encoding )
              break;
        }

      Found_idx_for_FTC_CMAP_BY_ENCODING:
        hash = idx * 67;
        break;

      case FTC_CMAP_BY_ID:
        for ( idx = 0; idx < count; idx++, cur++ )
        {
          if ( (FT_UInt)cur[0]->platform_id == desc->u.id.platform &&
               (FT_UInt)cur[0]->encoding_id == desc->u.id.encoding )
          {
            hash = ( ( desc->u.id.platform << 8 ) | desc->u.id.encoding ) * 7;
            break;
          }
        }
        break;

      default:
        ;
      }

      if ( idx >= count )
        goto Bad_Descriptor;

      /* compute hash value, both in family and query */
      cfam->index               = idx;
      cfam->hash                = hash ^ FTC_FACE_ID_HASH( desc->face_id );
      FTC_QUERY( cquery )->hash = FTC_CMAP_HASH( cfam, cquery );

      error = ftc_family_init( FTC_FAMILY( cfam ),
                               FTC_QUERY( cquery ), cache );
    }

    return error;

  Bad_Descriptor:
    FT_TRACE1(( "ftp_cmap_family_init: invalid charmap descriptor\n" ));
    return FTC_Err_Invalid_Argument;
  }


  FT_CALLBACK_DEF( FT_Bool )
  ftc_cmap_family_compare( FTC_CMapFamily  cfam,
                           FTC_CMapQuery   cquery )
  {
    FT_Int  result = 0;


    /* first, compare face id and type */
    if ( cfam->desc.face_id != cquery->desc->face_id ||
         cfam->desc.type    != cquery->desc->type    )
      goto Exit;

    switch ( cfam->desc.type )
    {
    case FTC_CMAP_BY_INDEX:
      result = ( cfam->desc.u.index == cquery->desc->u.index );
      break;

    case FTC_CMAP_BY_ENCODING:
      result = ( cfam->desc.u.encoding == cquery->desc->u.encoding );
      break;

    case FTC_CMAP_BY_ID:
      result = ( cfam->desc.u.id.platform == cquery->desc->u.id.platform &&
                 cfam->desc.u.id.encoding == cquery->desc->u.id.encoding );
      break;

    default:
      ;
    }

    if ( result )
    {
      /* when found, update the 'family' and 'hash' field of the query */
      FTC_QUERY( cquery )->family = FTC_FAMILY( cfam );
      FTC_QUERY( cquery )->hash   = FTC_CMAP_HASH( cfam, cquery );
    }

  Exit:
    return FT_BOOL( result );
  }


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                    GLYPH IMAGE CACHE                          *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  FT_CALLBACK_TABLE_DEF
  const FTC_Cache_ClassRec  ftc_cmap_cache_class =
  {
    sizeof ( FTC_CacheRec ),
    (FTC_Cache_InitFunc) ftc_cache_init,
    (FTC_Cache_ClearFunc)ftc_cache_clear,
    (FTC_Cache_DoneFunc) ftc_cache_done,

    sizeof ( FTC_CMapFamilyRec ),
    (FTC_Family_InitFunc)   ftc_cmap_family_init,
    (FTC_Family_CompareFunc)ftc_cmap_family_compare,
    (FTC_Family_DoneFunc)   ftc_family_done,

    sizeof ( FTC_CMapNodeRec ),
    (FTC_Node_InitFunc)   ftc_cmap_node_init,
    (FTC_Node_WeightFunc) ftc_cmap_node_weight,
    (FTC_Node_CompareFunc)ftc_cmap_node_compare,
    (FTC_Node_DoneFunc)   ftc_node_done
  };


  /* documentation is in ftccmap.h */

  FT_EXPORT_DEF( FT_Error )
  FTC_CMapCache_New( FTC_Manager     manager,
                     FTC_CMapCache  *acache )
  {
    return FTC_Manager_Register_Cache(
             manager,
             (FTC_Cache_Class)&ftc_cmap_cache_class,
             FTC_CACHE_P( acache ) );
  }


#ifdef FTC_CACHE_USE_INLINE

#define GEN_CACHE_FAMILY_COMPARE( f, q, c ) \
          ftc_cmap_family_compare( (FTC_CMapFamily)(f), (FTC_CMapQuery)(q) )

#define GEN_CACHE_NODE_COMPARE( n, q, c ) \
          ftc_cmap_node_compare( (FTC_CMapNode)(n), (FTC_CMapQuery)(q) )

#define GEN_CACHE_LOOKUP  ftc_cmap_cache_lookup

#include "ftccache.i"

#else  /* !FTC_CACHE_USE_INLINE */

#define ftc_cmap_cache_lookup  ftc_cache_lookup

#endif /* !FTC_CACHE_USE_INLINE */


  /* documentation is in ftccmap.h */

  FT_EXPORT_DEF( FT_UInt )
  FTC_CMapCache_Lookup( FTC_CMapCache  cache,
                        FTC_CMapDesc   desc,
                        FT_UInt32      char_code )
  {
    FTC_CMapQueryRec  cquery;
    FTC_CMapNode      node;
    FT_Error          error;
    FT_UInt           gindex = 0;


    if ( !cache || !desc )
    {
      FT_ERROR(( "FTC_CMapCache_Lookup: bad arguments, returning 0!\n" ));
      return 0;
    }

    cquery.desc      = desc;
    cquery.char_code = char_code;

    error = ftc_cmap_cache_lookup( FTC_CACHE( cache ),
                                   FTC_QUERY( &cquery ),
                                   (FTC_Node*)&node );
    if ( !error )
    {
      FT_UInt  offset = (FT_UInt)( char_code - node->first );


      FT_ASSERT( offset < FTC_CMAP_INDICES_MAX );

      gindex = node->indices[offset];
      if ( gindex == FTC_CMAP_UNKNOWN )
      {
        FT_Face  face;


        /* we need to use FT_Get_Char_Index */
        gindex = 0;

        error = FTC_Manager_Lookup_Face( FTC_CACHE(cache)->manager,
                                         desc->face_id,
                                         &face );
        if ( !error )
        {
          FT_CharMap  old, cmap  = NULL;
          FT_UInt     cmap_index;


          /* save old charmap, select new one */
          old        = face->charmap;
          cmap_index = FTC_CMAP_FAMILY( FTC_QUERY( &cquery )->family )->index;
          cmap       = face->charmaps[cmap_index];

          FT_Set_Charmap( face, cmap );

          /* perform lookup */
          gindex                = FT_Get_Char_Index( face, char_code );
          node->indices[offset] = (FT_UInt16)gindex;

          /* restore old charmap */
          FT_Set_Charmap( face, old );
        }
      }
    }

    return gindex;
  }


/* END */
