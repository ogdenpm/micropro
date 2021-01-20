#pragma once
#include <stdbool.h>

void showVersion(FILE *fp, bool full);

typedef struct _x {
    struct _x *left;
    struct _x *right;
    uint8_t flags;
    uint16_t loc;
    uint16_t len;
    char bytes[0];
} PatchNode;

#ifdef BDSC
typedef struct {
    int _fd;
    unsigned _hiMark;
    int _nsecs;
    int _nleft;
    char *_nextp;
    char _buf[512];
} MPFILE;
#endif




void NoOrChoose();
void op0f_pause();
void printStr(char *s);
int parseFspec(char *s);
void skipToMarker();
int YNReturn();
bool isFileChar(int ch);
void getUserFileName(char *s);
void selectProduct();
void errorLog(int n);
void getDrive(char *s);
int execute();
bool relTest();
PatchNode *addNode(PatchNode *p, PatchNode *q);
void walkPatchTree();
void patchNode(PatchNode *p);
int iGetItem();
void putStringList();
void showMenu();
int iGetRFld(int n);
void processPatches(int n);
void putVisible(int ch);
void badRadix();
void getValue(char *s, int flag);
int strtoi(char *s, int radix);
int ch2i(int ch, int radix);
unsigned strtou(char *s, int radix);
void erase(int n);
int iCmp(char *s);
int namedLocOffset(char *s);
void getAscii(char *s, int n);
void getUserString(char *s);
int getChVal();
int usrY();
void patchSubTree(PatchNode *p);


void op02_copy();
void op03_icall();
void op04_callIf();
void op05_exit();
void op06_patch();
void op08_getUserFileName();
void op09_getUserString();
void op0a_getDecimalValue();
void op0b_exec();
void op0c_pRdVal();
void op0d_jmpIf();
void op0e_jmp();
void op0f_pause();
void op10_callUsrNo();
void op11_jmpUsrNo();
void op12_callUsrYes();
void op13_jmpUsrYes();
void op15_doExpr();
void op16_list();
void op17_addPatch();
void op18_iPrintStr();
void op19_printSVar();
void op1a_printInt();
void op1d_menu();
void op1e_iRdSVar();
void op1f_openPatchFile();
void op20_pClose();
void op21_getHexValue();
void op22_unlink();
void op23_rename();
void op24_getChVal();
void op25_jmpIfNotCorrect();
void op26_getFnameSVar();
void op27_mkFname();
void op28_jmpIfCorrect();
void op29_chkAppSig();
void op2a_getLocation();
void op2b_iRdInputMode();
void op2c_printNL();
void op2d_getAsciiChar();
void op2e_readNthCh();
void op2f_testRdPatchFile();
void op30_getDriveSVar();

extern int inputMode;
extern int iByte;
extern int dByte;
