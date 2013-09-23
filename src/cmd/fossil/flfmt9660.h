void iso9660init(int fd, Header *h, char*, int);
void iso9660labels(Disk*, uchar*, void(*write)(int, u32int));
void iso9660copy(Fs*);
