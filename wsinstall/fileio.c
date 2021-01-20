#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#include <Windows.h>
#include <conio.h>
#include <io.h>
#endif
#include "wsinstall.h"
#include "fileio.h"





static uint8_t iCode[0x10000];      // where the .INS is loaded
static unsigned pc;         // current location pc in .INS
static unsigned rpc;        // current location of rpc in .INS
static unsigned codeLen;    // length of .INS
FILE *patchfp;



/*
    initConsole and resetConsole are used to setup and cleanup of the console I/O for raw input
*/
#ifdef _MSC_VER

static DWORD consoleMode;
static HANDLE consoleHw;

void resetConsole(void) {
    SetConsoleMode(consoleHw, consoleMode);
}

void initConsole() {
    consoleHw = (HANDLE)_get_osfhandle(_fileno(stdin));

    GetConsoleMode(consoleHw, &consoleMode);
    SetConsoleMode(consoleHw, ENABLE_VIRTUAL_TERMINAL_INPUT);
    atexit(resetConsole);
}

/*
    implementation of getch with detection of control C to exit,
    unless in raw mode
*/

int mpGetch() { // 425E
    int c;

    if ((c = _getch()) == 3 && inputMode != 3) { // control C & not rawmode
        printStr("\nReturning you to the system.\n");
        exit(1);
    }

    return c;
}
#else
#error portable console I/O needs code for this compiler/os
#endif



/*
* this code replaces the openIn function in the BDSC version
* mapping the CP/M file name so that @: and A: map to the current directory
* and B: through P: map to ./B through ./P
* the whole file is loaded into memory  and the location pointers for the
* instruction and data read copies are set up together
* this simplifies a lot of the code around seek and read and allows
* for later tracing to be added to effectively disassemble the .INS file
* 
* for now the patching file uses normal FILE operations as it is read/write
*/

int openIn(const char *fname) {
    FILE *fp;
    codeLen = 0;
    if (fp = cpmFopen(fname, "rb")) {
        codeLen = (unsigned)fread(iCode, 1, 0x10000, fp);
        fclose(fp);

    }
    pc = rpc = 0;
    return codeLen > 0 ? 1 : -1;

}

unsigned itell() {
    return pc;
}

unsigned dtell() {
    return rpc;
}


int iRdByte() {
    if (pc >= codeLen)
        errorLog(1);    // fileName fetch
    return iByte = iCode[pc++];
}


int iRdWord() { // 2db9
    int v = iRdByte();
    return v + (iRdByte() << 8);
}

int dRdByte() {
    if (rpc >= codeLen)
        errorLog(2);    // data fetch
    return iByte = iCode[rpc++];
}

int pRdByte(FILE *fp) {
    int c = getc(fp);
    if (ferror(fp))
        errorLog(8); // patch write error (could be other error but map to this)
    return c;
}

int iUngetc() {
    return pc != 0 && iByte == iCode[--pc] ? 0 : -1;
}

int dUngetc() {
    return rpc != 0 && dByte == iCode[--rpc] ? 0 : -1;
}

void iSeek(unsigned n) { // 42C7
    if (n >= codeLen)
        errorLog(3);    // token seek
    pc = n;
}


void dSeek(unsigned n) { // 42FB
    if (n >= codeLen)
        errorLog(4);    // data seek
    rpc = n;
}



void pSeek(unsigned n) {
    if (fseek(patchfp, n, 0) != 0)
        errorLog(7);    // patch seek
}

int iRdPeekByte() {
    if (pc >= codeLen)
        errorLog(1);
    return iCode[pc];
}

int iRdPeekWord() {
    if (pc + 1 >= codeLen)
        errorLog(1);
    return iCode[pc] + (iCode[pc + 1] << 8);
}

FILE *cpmFopen(const char *fname, const char *mode) {
    char osName[15];

    strcpy(osName, fname);
    if (osName[1] == ':') {         // have a disk specified so fix for windows/unix
        osName[1] = '/';
        if (osName[0] <= 'A')
            osName[0] = '.';

    }
    return fopen(osName, mode);

}