void	xmlamap(Hio *hout, AMap *v, char *tag, int indent);
void	xmlarena(Hio *hout, Arena *v, char *tag, int indent);
void	xmlindex(Hio *hout, Index *v, char *tag, int indent);

void	xmlaname(Hio *hout, char *v, char *tag);
void	xmlscore(Hio *hout, u8int *v, char *tag);
void	xmlsealed(Hio *hout, int v, char *tag);
void	xmlu32int(Hio *hout, u32int v, char *tag);
void	xmlu64int(Hio *hout, u64int v, char *tag);

void	xmlindent(Hio *hout, int indent);
