/***************************************************************************/
/*                                                                         */
/*  ftobjs.h                                                               */
/*                                                                         */
/*    The FreeType private base classes (specification).                   */
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


  /*************************************************************************/
  /*                                                                       */
  /*  This file contains the definition of all internal FreeType classes.  */
  /*                                                                       */
  /*************************************************************************/


#ifndef __FTOBJS_H__
#define __FTOBJS_H__

#include <ft2build.h>
#include FT_CONFIG_STANDARD_LIBRARY_H   /* for ft_setjmp and ft_longjmp */
#include FT_RENDER_H
#include FT_SIZES_H
#include FT_INTERNAL_MEMORY_H
#include FT_INTERNAL_GLYPH_LOADER_H
#include FT_INTERNAL_DRIVER_H
#include FT_INTERNAL_AUTOHINT_H
#include FT_INTERNAL_OBJECT_H

#ifdef FT_CONFIG_OPTION_INCREMENTAL
#include FT_INCREMENTAL_H
#endif


FT_BEGIN_HEADER


  /*************************************************************************/
  /*                                                                       */
  /* Some generic definitions.                                             */
  /*                                                                       */
#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE  0
#endif

#ifndef NULL
#define NULL  (void*)0
#endif


  /*************************************************************************/
  /*                                                                       */
  /* The min and max functions missing in C.  As usual, be careful not to  */
  /* write things like MIN( a++, b++ ) to avoid side effects.              */
  /*                                                                       */
#ifndef MIN
#define MIN( a, b )  ( (a) < (b) ? (a) : (b) )
#endif

#ifndef MAX
#define MAX( a, b )  ( (a) > (b) ? (a) : (b) )
#endif

#ifndef ABS
#define ABS( a )     ( (a) < 0 ? -(a) : (a) )
#endif


  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/
  /****                                                                 ****/
  /****                                                                 ****/
  /****                    V A L I D A T I O N                          ****/
  /****                                                                 ****/
  /****                                                                 ****/
  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/

  /* handle to a validation object */
  typedef struct FT_ValidatorRec_*  FT_Validator;


  /*************************************************************************/
  /*                                                                       */
  /* There are three distinct validation levels defined here:              */
  /*                                                                       */
  /* FT_VALIDATE_DEFAULT ::                                                */
  /*   A table that passes this validation level can be used reliably by   */
  /*   FreeType.  It generally means that all offsets have been checked to */
  /*   prevent out-of-bound reads, array counts are correct, etc.          */
  /*                                                                       */
  /* FT_VALIDATE_TIGHT ::                                                  */
  /*   A table that passes this validation level can be used reliably and  */
  /*   doesn't contain invalid data.  For example, a charmap table that    */
  /*   returns invalid glyph indices will not pass, even though it can     */
  /*   be used with FreeType in default mode (the library will simply      */
  /*   return an error later when trying to load the glyph).               */
  /*                                                                       */
  /*   It also check that fields that must be a multiple of 2, 4, or 8     */
  /*   dont' have incorrect values, etc.                                   */
  /*                                                                       */
  /* FT_VALIDATE_PARANOID ::                                               */
  /*   Only for font debugging.  Checks that a table follows the           */
  /*   specification by 100%.  Very few fonts will be able to pass this    */
  /*   level anyway but it can be useful for certain tools like font       */
  /*   editors/converters.                                                 */
  /*                                                                       */
  typedef enum  FT_ValidationLevel_
  {
    FT_VALIDATE_DEFAULT = 0,
    FT_VALIDATE_TIGHT,
    FT_VALIDATE_PARANOID

  } FT_ValidationLevel;


  /* validator structure */
  typedef struct  FT_ValidatorRec_
  {
    const FT_Byte*      base;        /* address of table in memory       */
    const FT_Byte*      limit;       /* `base' + sizeof(table) in memory */
    FT_ValidationLevel  level;       /* validation level                 */
    FT_Error            error;       /* error returned. 0 means success  */

    ft_jmp_buf          jump_buffer; /* used for exception handling      */

  } FT_ValidatorRec;


#define FT_VALIDATOR( x )  ((FT_Validator)( x ))


  FT_BASE( void )
  ft_validator_init( FT_Validator        valid,
                     const FT_Byte*      base,
                     const FT_Byte*      limit,
                     FT_ValidationLevel  level );

  FT_BASE( FT_Int )
  ft_validator_run( FT_Validator  valid );

  /* Sets the error field in a validator, then calls `longjmp' to return */
  /* to high-level caller.  Using `setjmp/longjmp' avoids many stupid    */
  /* error checks within the validation routines.                        */
  /*                                                                     */
  FT_BASE( void )
  ft_validator_error( FT_Validator  valid,
                      FT_Error      error );


  /* Calls ft_validate_error.  Assumes that the `valid' local variable */
  /* holds a pointer to the current validator object.                  */
  /*                                                                   */
#define FT_INVALID( _error )  ft_validator_error( valid, _error )

  /* called when a broken table is detected */
#define FT_INVALID_TOO_SHORT  FT_INVALID( FT_Err_Invalid_Table )

  /* called when an invalid offset is detected */
#define FT_INVALID_OFFSET     FT_INVALID( FT_Err_Invalid_Offset )

  /* called when an invalid format/value is detected */
#define FT_INVALID_FORMAT     FT_INVALID( FT_Err_Invalid_Table )

  /* called when an invalid glyph index is detected */
#define FT_INVALID_GLYPH_ID   FT_INVALID( FT_Err_Invalid_Glyph_Index )

  /* called when an invalid field value is detected */
#define FT_INVALID_DATA       FT_INVALID( FT_Err_Invalid_Table )


  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/
  /****                                                                 ****/
  /****                                                                 ****/
  /****                       C H A R M A P S                           ****/
  /****                                                                 ****/
  /****                                                                 ****/
  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/

  /* handle to internal charmap object */
  typedef struct FT_CMapRec_*              FT_CMap;

  /* handle to charmap class structure */
  typedef const struct FT_CMap_ClassRec_*  FT_CMap_Class;

  /* internal charmap object structure */
  typedef struct  FT_CMapRec_
  {
    FT_CharMapRec  charmap;
    FT_CMap_Class  clazz;

  } FT_CMapRec;

  /* typecase any pointer to a charmap handle */
#define FT_CMAP( x )              ((FT_CMap)( x ))

  /* obvious macros */
#define FT_CMAP_PLATFORM_ID( x )  FT_CMAP( x )->charmap.platform_id
#define FT_CMAP_ENCODING_ID( x )  FT_CMAP( x )->charmap.encoding_id
#define FT_CMAP_ENCODING( x )     FT_CMAP( x )->charmap.encoding
#define FT_CMAP_FACE( x )         FT_CMAP( x )->charmap.face


  /* class method definitions */
  typedef FT_Error
  (*FT_CMap_InitFunc)( FT_CMap     cmap,
                       FT_Pointer  init_data );

  typedef void
  (*FT_CMap_DoneFunc)( FT_CMap  cmap );

  typedef FT_UInt
  (*FT_CMap_CharIndexFunc)( FT_CMap    cmap,
                            FT_UInt32  char_code );

  typedef FT_UInt
  (*FT_CMap_CharNextFunc)( FT_CMap     cmap,
                           FT_UInt32  *achar_code );


  typedef struct  FT_CMap_ClassRec_
  {
    FT_UInt                size;
    FT_CMap_InitFunc       init;
    FT_CMap_DoneFunc       done;
    FT_CMap_CharIndexFunc  char_index;
    FT_CMap_CharNextFunc   char_next;

  } FT_CMap_ClassRec;


  /* create a new charmap and add it to charmap->face */
  FT_BASE( FT_Error )
  FT_CMap_New( FT_CMap_Class  clazz,
               FT_Pointer     init_data,
               FT_CharMap     charmap,
               FT_CMap       *acmap );

  /* destroy a charmap (don't remove it from face's list though) */
  FT_BASE( void )
  FT_CMap_Done( FT_CMap  cmap );


  /*************************************************************************/
  /*                                                                       */
  /* <Struct>                                                              */
  /*    FT_Face_InternalRec                                                */
  /*                                                                       */
  /* <Description>                                                         */
  /*    This structure contains the internal fields of each FT_Face        */
  /*    object.  These fields may change between different releases of     */
  /*    FreeType.                                                          */
  /*                                                                       */
  /* <Fields>                                                              */
  /*    max_points       :: The maximal number of points used to store the */
  /*                        vectorial outline of any glyph in this face.   */
  /*                        If this value cannot be known in advance, or   */
  /*                        if the face isn't scalable, this should be set */
  /*                        to 0.  Only relevant for scalable formats.     */
  /*                                                                       */
  /*    max_contours     :: The maximal number of contours used to store   */
  /*                        the vectorial outline of any glyph in this     */
  /*                        face.  If this value cannot be known in        */
  /*                        advance, or if the face isn't scalable, this   */
  /*                        should be set to 0.  Only relevant for         */
  /*                        scalable formats.                              */
  /*                                                                       */
  /*    transform_matrix :: A 2x2 matrix of 16.16 coefficients used to     */
  /*                        transform glyph outlines after they are loaded */
  /*                        from the font.  Only used by the convenience   */
  /*                        functions.                                     */
  /*                                                                       */
  /*    transform_delta  :: A translation vector used to transform glyph   */
  /*                        outlines after they are loaded from the font.  */
  /*                        Only used by the convenience functions.        */
  /*                                                                       */
  /*    transform_flags  :: Some flags used to classify the transform.     */
  /*                        Only used by the convenience functions.        */
  /*                                                                       */
  /*    hint_flags       :: Some flags used to change the hinters'         */
  /*                        behaviour.  Only used for debugging for now.   */
  /*                                                                       */
  /*    postscript_name  :: Postscript font name for this face.            */
  /*                                                                       */
  /*    incremental_interface ::                                           */
  /*                        If non-null, the interface through             */
  /*                        which glyph data and metrics are loaded        */
  /*                        incrementally for faces that do not provide    */
  /*                        all of this data when first opened.            */
  /*                        This field exists only if                      */
  /*                        @FT_CONFIG_OPTION_INCREMENTAL is defined.      */
  /*                                                                       */
  typedef struct  FT_Face_InternalRec_
  {
    FT_UShort    max_points;
    FT_Short     max_contours;

    FT_Matrix    transform_matrix;
    FT_Vector    transform_delta;
    FT_Int       transform_flags;

    FT_UInt32    hint_flags;

    const char*  postscript_name;

#ifdef FT_CONFIG_OPTION_INCREMENTAL
    FT_Incremental_InterfaceRec*  incremental_interface;
#endif

  } FT_Face_InternalRec;


  /*************************************************************************/
  /*                                                                       */
  /* <Struct>                                                              */
  /*    FT_Slot_InternalRec                                                */
  /*                                                                       */
  /* <Description>                                                         */
  /*    This structure contains the internal fields of each FT_GlyphSlot   */
  /*    object.  These fields may change between different releases of     */
  /*    FreeType.                                                          */
  /*                                                                       */
  /* <Fields>                                                              */
  /*    loader            :: The glyph loader object used to load outlines */
  /*                         into the glyph slot.                          */
  /*                                                                       */
  /*    glyph_transformed :: Boolean.  Set to TRUE when the loaded glyph   */
  /*                         must be transformed through a specific        */
  /*                         font transformation.  This is _not_ the same  */
  /*                         as the face transform set through             */
  /*                         FT_Set_Transform().                           */
  /*                                                                       */
  /*    glyph_matrix      :: The 2x2 matrix corresponding to the glyph     */
  /*                         transformation, if necessary.                 */
  /*                                                                       */
  /*    glyph_delta       :: The 2d translation vector corresponding to    */
  /*                         the glyph transformation, if necessary.       */
  /*                                                                       */
  /*    glyph_hints       :: Format-specific glyph hints management.       */
  /*                                                                       */
  typedef struct  FT_Slot_InternalRec_
  {
    FT_GlyphLoader  loader;
    FT_Bool         glyph_transformed;
    FT_Matrix       glyph_matrix;
    FT_Vector       glyph_delta;
    void*           glyph_hints;

  } FT_GlyphSlot_InternalRec;


  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/
  /****                                                                 ****/
  /****                                                                 ****/
  /****                         M O D U L E S                           ****/
  /****                                                                 ****/
  /****                                                                 ****/
  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/


  /*************************************************************************/
  /*                                                                       */
  /* <Struct>                                                              */
  /*    FT_ModuleRec                                                       */
  /*                                                                       */
  /* <Description>                                                         */
  /*    A module object instance.                                          */
  /*                                                                       */
  /* <Fields>                                                              */
  /*    clazz   :: A pointer to the module's class.                        */
  /*                                                                       */
  /*    library :: A handle to the parent library object.                  */
  /*                                                                       */
  /*    memory  :: A handle to the memory manager.                         */
  /*                                                                       */
  /*    generic :: A generic structure for user-level extensibility (?).   */
  /*                                                                       */
  typedef struct  FT_ModuleRec_
  {
    FT_Module_Class*  clazz;
    FT_Library        library;
    FT_Memory         memory;
    FT_Generic        generic;

  } FT_ModuleRec;


  /* typecast an object to a FT_Module */
#define FT_MODULE( x )          ((FT_Module)( x ))
#define FT_MODULE_CLASS( x )    FT_MODULE( x )->clazz
#define FT_MODULE_LIBRARY( x )  FT_MODULE( x )->library
#define FT_MODULE_MEMORY( x )   FT_MODULE( x )->memory


#define FT_MODULE_IS_DRIVER( x )  ( FT_MODULE_CLASS( x )->module_flags & \
                                    ft_module_font_driver )

#define FT_MODULE_IS_RENDERER( x )  ( FT_MODULE_CLASS( x )->module_flags & \
                                      ft_module_renderer )

#define FT_MODULE_IS_HINTER( x )  ( FT_MODULE_CLASS( x )->module_flags & \
                                    ft_module_hinter )

#define FT_MODULE_IS_STYLER( x )  ( FT_MODULE_CLASS( x )->module_flags & \
                                    ft_module_styler )

#define FT_DRIVER_IS_SCALABLE( x )  ( FT_MODULE_CLASS( x )->module_flags & \
                                      ft_module_driver_scalable )

#define FT_DRIVER_USES_OUTLINES( x )  !( FT_MODULE_CLASS( x )->module_flags & \
                                         ft_module_driver_no_outlines )

#define FT_DRIVER_HAS_HINTER( x )  ( FT_MODULE_CLASS( x )->module_flags & \
                                     ft_module_driver_has_hinter )


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    FT_Get_Module_Interface                                            */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Finds a module and returns its specific interface as a typeless    */
  /*    pointer.                                                           */
  /*                                                                       */
  /* <Input>                                                               */
  /*    library     :: A handle to the library object.                     */
  /*                                                                       */
  /*    module_name :: The module's name (as an ASCII string).             */
  /*                                                                       */
  /* <Return>                                                              */
  /*    A module-specific interface if available, 0 otherwise.             */
  /*                                                                       */
  /* <Note>                                                                */
  /*    You should better be familiar with FreeType internals to know      */
  /*    which module to look for, and what its interface is :-)            */
  /*                                                                       */
  FT_BASE( const void* )
  FT_Get_Module_Interface( FT_Library   library,
                           const char*  mod_name );

 /* */


  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/
  /****                                                                 ****/
  /****                                                                 ****/
  /****               FACE, SIZE & GLYPH SLOT OBJECTS                   ****/
  /****                                                                 ****/
  /****                                                                 ****/
  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/

  /* a few macros used to perform easy typecasts with minimal brain damage */

#define FT_FACE( x )          ((FT_Face)(x))
#define FT_SIZE( x )          ((FT_Size)(x))
#define FT_SLOT( x )          ((FT_GlyphSlot)(x))

#define FT_FACE_DRIVER( x )   FT_FACE( x )->driver
#define FT_FACE_LIBRARY( x )  FT_FACE_DRIVER( x )->root.library
#define FT_FACE_MEMORY( x )   FT_FACE( x )->memory
#define FT_FACE_STREAM( x )   FT_FACE( x )->stream

#define FT_SIZE_FACE( x )     FT_SIZE( x )->face
#define FT_SLOT_FACE( x )     FT_SLOT( x )->face

#define FT_FACE_SLOT( x )     FT_FACE( x )->glyph
#define FT_FACE_SIZE( x )     FT_FACE( x )->size


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    FT_New_GlyphSlot                                                   */
  /*                                                                       */
  /* <Description>                                                         */
  /*    It is sometimes useful to have more than one glyph slot for a      */
  /*    given face object.  This function is used to create additional     */
  /*    slots.  All of them are automatically discarded when the face is   */
  /*    destroyed.                                                         */
  /*                                                                       */
  /* <Input>                                                               */
  /*    face  :: A handle to a parent face object.                         */
  /*                                                                       */
  /* <Output>                                                              */
  /*    aslot :: A handle to a new glyph slot object.                      */
  /*                                                                       */
  /* <Return>                                                              */
  /*    FreeType error code.  0 means success.                             */
  /*                                                                       */
  FT_BASE( FT_Error )
  FT_New_GlyphSlot( FT_Face        face,
                    FT_GlyphSlot  *aslot );


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    FT_Done_GlyphSlot                                                  */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Destroys a given glyph slot.  Remember however that all slots are  */
  /*    automatically destroyed with its parent.  Using this function is   */
  /*    not always mandatory.                                              */
  /*                                                                       */
  /* <Input>                                                               */
  /*    slot :: A handle to a target glyph slot.                           */
  /*                                                                       */
  FT_BASE( void )
  FT_Done_GlyphSlot( FT_GlyphSlot  slot );

 /* */
 
 /*
  * free the bitmap of a given glyphslot when needed
  * (i.e. only when it was allocated with ft_glyphslot_alloc_bitmap)
  */
  FT_BASE( void )
  ft_glyphslot_free_bitmap( FT_GlyphSlot  slot );
 
 /*
  * allocate a new bitmap buffer in a glyph slot
  */
  FT_BASE( FT_Error )
  ft_glyphslot_alloc_bitmap( FT_GlyphSlot  slot,
                             FT_ULong      size );

 /*
  * set the bitmap buffer in a glyph slot to a given pointer.
  * the buffer will not be freed by a later call to ft_glyphslot_free_bitmap
  */
  FT_BASE( void )
  ft_glyphslot_set_bitmap( FT_GlyphSlot   slot,
                           FT_Pointer     buffer );


  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/
  /****                                                                 ****/
  /****                                                                 ****/
  /****                        R E N D E R E R S                        ****/
  /****                                                                 ****/
  /****                                                                 ****/
  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/


#define FT_RENDERER( x )      ((FT_Renderer)( x ))
#define FT_GLYPH( x )         ((FT_Glyph)( x ))
#define FT_BITMAP_GLYPH( x )  ((FT_BitmapGlyph)( x ))
#define FT_OUTLINE_GLYPH( x ) ((FT_OutlineGlyph)( x ))


  typedef struct  FT_RendererRec_
  {
    FT_ModuleRec            root;
    FT_Renderer_Class*      clazz;
    FT_Glyph_Format         glyph_format;
    FT_Glyph_Class          glyph_class;

    FT_Raster               raster;
    FT_Raster_Render_Func   raster_render;
    FT_Renderer_RenderFunc  render;

  } FT_RendererRec;


  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/
  /****                                                                 ****/
  /****                                                                 ****/
  /****                    F O N T   D R I V E R S                      ****/
  /****                                                                 ****/
  /****                                                                 ****/
  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/


  /* typecast a module into a driver easily */
#define FT_DRIVER( x )        ((FT_Driver)(x))

  /* typecast a module as a driver, and get its driver class */
#define FT_DRIVER_CLASS( x )  FT_DRIVER( x )->clazz


  /*************************************************************************/
  /*                                                                       */
  /* <Struct>                                                              */
  /*    FT_DriverRec                                                       */
  /*                                                                       */
  /* <Description>                                                         */
  /*    The root font driver class.  A font driver is responsible for      */
  /*    managing and loading font files of a given format.                 */
  /*                                                                       */
  /*  <Fields>                                                             */
  /*     root         :: Contains the fields of the root module class.     */
  /*                                                                       */
  /*     clazz        :: A pointer to the font driver's class.  Note that  */
  /*                     this is NOT root.clazz.  `class' wasn't used      */
  /*                     as it is a reserved word in C++.                  */
  /*                                                                       */
  /*     faces_list   :: The list of faces currently opened by this        */
  /*                     driver.                                           */
  /*                                                                       */
  /*     extensions   :: A typeless pointer to the driver's extensions     */
  /*                     registry, if they are supported through the       */
  /*                     configuration macro FT_CONFIG_OPTION_EXTENSIONS.  */
  /*                                                                       */
  /*     glyph_loader :: The glyph loader for all faces managed by this    */
  /*                     driver.  This object isn't defined for unscalable */
  /*                     formats.                                          */
  /*                                                                       */
  typedef struct  FT_DriverRec_
  {
    FT_ModuleRec     root;
    FT_Driver_Class  clazz;

    FT_ListRec       faces_list;
    void*            extensions;

    FT_GlyphLoader   glyph_loader;

  } FT_DriverRec;


  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/
  /****                                                                 ****/
  /****                                                                 ****/
  /****                       L I B R A R I E S                         ****/
  /****                                                                 ****/
  /****                                                                 ****/
  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/


#define FT_DEBUG_HOOK_TRUETYPE  0
#define FT_DEBUG_HOOK_TYPE1     1


  /*************************************************************************/
  /*                                                                       */
  /* <Struct>                                                              */
  /*    FT_LibraryRec                                                      */
  /*                                                                       */
  /* <Description>                                                         */
  /*    The FreeType library class.  This is the root of all FreeType      */
  /*    data.  Use FT_New_Library() to create a library object, and        */
  /*    FT_Done_Library() to discard it and all child objects.             */
  /*                                                                       */
  /* <Fields>                                                              */
  /*    memory           :: The library's memory object.  Manages memory   */
  /*                        allocation.                                    */
  /*                                                                       */
  /*    generic          :: Client data variable.  Used to extend the      */
  /*                        Library class by higher levels and clients.    */
  /*                                                                       */
  /*    num_modules      :: The number of modules currently registered     */
  /*                        within this library.  This is set to 0 for new */
  /*                        libraries.  New modules are added through the  */
  /*                        FT_Add_Module() API function.                  */
  /*                                                                       */
  /*    modules          :: A table used to store handles to the currently */
  /*                        registered modules. Note that each font driver */
  /*                        contains a list of its opened faces.           */
  /*                                                                       */
  /*    renderers        :: The list of renderers currently registered     */
  /*                        within the library.                            */
  /*                                                                       */
  /*    cur_renderer     :: The current outline renderer.  This is a       */
  /*                        shortcut used to avoid parsing the list on     */
  /*                        each call to FT_Outline_Render().  It is a     */
  /*                        handle to the current renderer for the         */
  /*                        FT_GLYPH_FORMAT_OUTLINE format.                */
  /*                                                                       */
  /*    auto_hinter      :: XXX                                            */
  /*                                                                       */
  /*    raster_pool      :: The raster object's render pool.  This can     */
  /*                        ideally be changed dynamically at run-time.    */
  /*                                                                       */
  /*    raster_pool_size :: The size of the render pool in bytes.          */
  /*                                                                       */
  /*    debug_hooks      :: XXX                                            */
  /*                                                                       */
  typedef struct  FT_LibraryRec_
  {
    FT_Memory          memory;           /* library's memory manager */

    FT_Generic         generic;

    FT_Int             version_major;
    FT_Int             version_minor;
    FT_Int             version_patch;

    FT_UInt            num_modules;
    FT_Module          modules[FT_MAX_MODULES];  /* module objects  */

    FT_ListRec         renderers;        /* list of renderers        */
    FT_Renderer        cur_renderer;     /* current outline renderer */
    FT_Module          auto_hinter;

    FT_Byte*           raster_pool;      /* scan-line conversion */
                                         /* render pool          */
    FT_ULong           raster_pool_size; /* size of render pool in bytes */

    FT_DebugHook_Func  debug_hooks[4];

    FT_MetaClassRec    meta_class;

  } FT_LibraryRec;


  FT_BASE( FT_Renderer )
  FT_Lookup_Renderer( FT_Library       library,
                      FT_Glyph_Format  format,
                      FT_ListNode*     node );

  FT_BASE( FT_Error )
  FT_Render_Glyph_Internal( FT_Library      library,
                            FT_GlyphSlot    slot,
                            FT_Render_Mode  render_mode );

  typedef const char*
  (*FT_Face_GetPostscriptNameFunc)( FT_Face  face );

  typedef FT_Error
  (*FT_Face_GetGlyphNameFunc)( FT_Face     face,
                               FT_UInt     glyph_index,
                               FT_Pointer  buffer,
                               FT_UInt     buffer_max );

  typedef FT_UInt
  (*FT_Face_GetGlyphNameIndexFunc)( FT_Face     face,
                                    FT_String*  glyph_name );


#ifndef FT_CONFIG_OPTION_NO_DEFAULT_SYSTEM

  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    FT_New_Memory                                                      */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Creates a new memory object.                                       */
  /*                                                                       */
  /* <Return>                                                              */
  /*    A pointer to the new memory object.  0 in case of error.           */
  /*                                                                       */
  FT_EXPORT( FT_Memory )
  FT_New_Memory( void );


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    FT_Done_Memory                                                     */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Discards memory manager.                                           */
  /*                                                                       */
  /* <Input>                                                               */
  /*    memory :: A handle to the memory manager.                          */
  /*                                                                       */
  FT_EXPORT( void )
  FT_Done_Memory( FT_Memory  memory );

#endif /* !FT_CONFIG_OPTION_NO_DEFAULT_SYSTEM */


  /* Define default raster's interface.  The default raster is located in  */
  /* `src/base/ftraster.c'.                                                */
  /*                                                                       */
  /* Client applications can register new rasters through the              */
  /* FT_Set_Raster() API.                                                  */

#ifndef FT_NO_DEFAULT_RASTER
  FT_EXPORT_VAR( FT_Raster_Funcs )  ft_default_raster;
#endif


FT_END_HEADER

#endif /* __FTOBJS_H__ */


/* END */
