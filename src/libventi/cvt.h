/*
 * integer conversion routines
 */
#define	U8GET(p)	((p)[0])
#define	U16GET(p)	(((p)[0]<<8)|(p)[1])
#define	U32GET(p)	((u32int)(((p)[0]<<24)|((p)[1]<<16)|((p)[2]<<8)|(p)[3]))
#define	U48GET(p)	(((vlong)U16GET(p)<<32)|(vlong)U32GET((p)+2))
#define	U64GET(p)	(((vlong)U32GET(p)<<32)|(vlong)U32GET((p)+4))

#define	U8PUT(p,v)	(p)[0]=(v)
#define	U16PUT(p,v)	(p)[0]=(v)>>8;(p)[1]=(v)
#define	U32PUT(p,v)	(p)[0]=(v)>>24;(p)[1]=(v)>>16;(p)[2]=(v)>>8;(p)[3]=(v)
#define	U48PUT(p,v,t32)	t32=(v)>>32;U16PUT(p,t32);t32=(v);U32PUT((p)+2,t32)
#define	U64PUT(p,v,t32)	t32=(v)>>32;U32PUT(p,t32);t32=(v);U32PUT((p)+4,t32)
