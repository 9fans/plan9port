/*
 * zlib header fields
 */
enum
{
	ZlibMeth	= 0x0f,			/* mask of compression methods */
	ZlibDeflate	= 0x08,

	ZlibCInfo	= 0xf0,			/* mask of compression aux. info */
	ZlibWin32k	= 0x70,			/* 32k history window */
};
