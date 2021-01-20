#pragma once
void initConsole();
void resetConsole();
int mpGetch();
int openIn(const char *fname);
unsigned itell();
unsigned dtell();
int iRdByte();
int iRdWord();
int dRdByte();
int pRdByte(FILE *fp);

int iUngetc();
int dUngetc();
void iSeek(unsigned where);
void dSeek(unsigned where);
void pSeek(unsigned where);

int iRdPeekByte();
int iRdPeekWord();


FILE *cpmFopen(const char *fname, const char *mode);

extern FILE *patchfp;

#define alloc   malloc
#define pPutc(n)    putc(n, patchfp)
#define vexit() exit(0)
#define movmem(src, dst, cnt)   memmove(dst, src, cnt)
#define setmem(buf, cnt, val)   memset(buf, val, cnt)
#define gets(buf)   gets_s(buf, 136)