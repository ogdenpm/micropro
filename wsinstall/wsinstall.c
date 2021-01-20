//
//	Last mod:	10 January 2021 11:35:24
//

#include <stdio.h>
#ifndef BDSC
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "wsinstall.h"
#include "fileio.h"
#endif


#if _DEBUG
#define TRACE(fmt,...)   fprintf(stderr, fmt, __VA_ARGS__)
#else
#define TRACE(fmt, ...)
#endif




#define MENU_ITEMS_PER_PAGE 12

// parseFspec error codes
#define FNAMEOK     0
#define INVALIDDISK 1
#define MULTIPLEDOT 2
#define NAMETOOSHORT 3
#define NAMETOOLONG 4
#define EXTTOOLONG  5
#define NONAME      6
#define NOTFOUND    7


// reserved integer registers for WS.INS
#define _patchStartLoc  iVar[1]
#define _patchLoc       iVar[2]
#define _patchLen       iVar[3]
#define _curInt         iVar[4]
#define _targetType     iVar[7]
#define _change         iVar[8]
#define _startPatchArea iVar[10]
#define _endPatchArea   iVar[11]
#define _menuAction     iVar[18]
// reserved error variables
#define _Error          iVar[50]
#define _CreateError    iVar[51]
#define _OpenError      iVar[52]
#define _ReadError      iVar[53]
#define _WriteError     iVar[54]
#define _DiskFull       iVar[55]
#define _SelectionDone    iVar[56]
#define _NameError      iVar[57]
#define _UnlinkError    iVar[58]
#define _RenameError    iVar[59]

// reserved string variables for WS.INS
#define _tmpfile        sVar[1]
#define _baseFile       sVar[2]
#define _targetFile     sVar[3]
#define _curStr         sVar[4]
#define _drive          sVar[5]
#define _file           sVar[6]
#define _ext            sVar[7]
#define _exeExt         sVar[8]
#define _spaces         sVar[9]
#define _usrDrive       sVar[12]




// system vars including _base
#ifdef BDSC
#define bool    int
#define true    1
#define false   0
#define uint8_t unsigned char
#define uint16_t unsigned

char *_allocp;   // used by alloc
#endif

char sVar[21][40];      // string variables each 40 chars
int iVar[60];           // integer variables, >= 50 are error codes

#ifdef BDSC
char *ctrlChar[32];
char *pConfirm;
#else
  // modern C can iniitalise inline
char *ctrlChar[32] = {
    "NUL", "SOH", "STX", "ETX",  "EOT",  "ENQ", "ACK", "BEL",
    "BS", "HT", "LF", "VT", "FF", "CR", "SO", "SI",
    "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
    "CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US",
};

char *pConfirm = "\n   Confirm  ";
#endif


static char userListSelection;
static char listSelector;  // op16 & iGetItem
static int locPatchICode;          // points to patch code type 14 used for the patch name field
static uint16_t locPatchEncoding;  // points to start of the code type 17, 18 & 7 used to parse patch data
static int menus[18 * MENU_ITEMS_PER_PAGE];  // op1d & showMenu
static uint16_t nMenuItems;
static uint16_t nPages, pageNo;
static bool done;
static uint16_t nCurMenuItems, locCurMenuText;

#ifdef BDSC
MPFILE patchfp;
MPFILE insfp;
MPFILE datafp;
#endif
char userIn[258];

static bool patchFileOpen;
static int lowPatchRange;       // range selectors, set in op06_patch, which is not used for ws.ins
static int highPatchRange;
static PatchNode node;
static PatchNode *pTree;
static int locDiff;             // used to determine whether to create left, right node or replace
static int patchFlagBit;        // index of bit used to determine if patch still needs to be applied
static char fspecDrv[3];
static char fspecFile[9];
static char fspecExt[4];
static char fileName[15];
int iByte;
int dByte;
static int patchBias;       // adjustment applied to patch location to make it relative to start of file
                            // for .COM this is -0x100, for .CMD this is + 0x80
int inputMode;              // current input mode
#define HEX     1
#define DECIMAL 2
#define ASCII   3

static int debug;           // set if DEBUG is entered on cmd line. Shows additional interpreter info
static int w8C50, w8C52, w8C54; // set to 0 in op2_copy, but otherwise not used




int main(int argc, char **argv) { // 7D4
    char *installMsg, *continueMsg, *cmdErrMsg;     // could be done in line
    char *errMsg;
    int err;
#ifdef BDSC 
    unsigned *pCPMBase;
#endif
    int i;

#ifdef BDSC
    _allocp = 0;
    putchar(0);
#else
    // my standard show version infor for git projects
    if (argc == 2 && _stricmp(argv[1], "-v") == 0) {
        showVersion(stdout, argv[1][1] == 'V');
        exit(0);
    }
    // setup raw IO for console
    initConsole();
#endif
    printStr("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
        "            GP INSTALL Release 2.00         \r\n"
        "    Copyright (c) 1983 MicroPro International Corporation\n\n"
        "           All rights reserved\n\n");
    printStr(" This software has been provided pursuant to a License       \n"
        " Agreement containing restrictions on its use.  The software \n"
        " contains valuable trade secrets and proprietary information \n"
        " of MicroPro International Corporation and is protected by   \n");
    printStr(" federal copyright law.  It may not be copied or distributed \n"
        " in any form or medium, disclosed to third parties, or used  \n"
        " in any manner not provided for in said License Agreement    \n"
        " except with prior written authorization from MicroPro.      \n\n");
    op0f_pause();
    printStr("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
#ifdef BDSC
    pCPMBase = 6;
    if (*pCPMBase < 0x2800) {
        printf("FATAL ERROR:\n");
        printf("  There isn't enough memory available\n");
        printf("  to run INSTALL.\n");
        vexit();
    }


    pTree = 0;          // not needed for std C initialisation
    patchFileOpen = 0;
    w8C14 = 0;
    w8C16 = 0;
    initCtrlStr();
    setmem(iVar, 60 * sizeof(int), 0);
    setmem(sVar, 40 * 20, 0);

    pConfirm = "\n   Confirm  ";
#endif
    cmdErrMsg = "  Command line error:\n\n           ";
    installMsg = "With INSTALL you can set up your terminal and printer\n"
        "for use with MicroPro programs.  You can also change\n"
        "certain features of the program with INSTALL.\n\n";
    continueMsg = "    Would you like to continue?\n"
        "                     Enter Y or <RETURN> for Yes.\n"
        "                     Enter N for No.  ";

    for (i = 3; i <= argc; i++)
        if (strcmp("-DEBUG", argv[i - 1]) == 0)
            debug = 1;
    err = argc >= 2 ? parseFspec(argv[1]) : NONAME;
    // 935
getFile:
    if (err == 0) {
        strcpy(fileName, fspecDrv);
        strcat(fileName, fspecFile);
        strcat(fileName, ".INS");
    } else {
        switch (err) {
        case 1: errMsg = "Bad drive code."; break;
        case 2: case 3: case 4: case 5:
            errMsg = "File name not properly formed."; break;
        case 6: errMsg = "No file name specified."; break;
        case 7: errMsg = " not found"; break;
#ifndef BDSC
        default: errMsg = " internal error"; break;     //  prevent uninitialised error
#endif
        }

        if (err != NONAME) {
            printStr(cmdErrMsg);
            if (err == NOTFOUND)
                printStr(fileName);
            printStr(errMsg);
        }

        printStr("\n\n");
        printStr(installMsg);
        printStr(continueMsg);
        NoOrChoose();
    }

    /*
    * to simplify the file access and to allow for tagging of the use of bytes
    * in the INS file, the modified version loads the file into memory
    * and the corresponding access functions use this in memory copy
    * the details are in fileio.c
    */
#ifdef BDSC
    while (openIn(fileName, &insfp) == -1 || openIn(fileName, &datafp) == -1) {
#else
    while (openIn(fileName) == -1) {
#endif
        if (err == 0) {
            err = NOTFOUND;
            goto getFile;
        }

        printf("   The file %s cannot be found.\n"
            "   This definition file contains control information used\n", fileName);
        printf("   by the INSTALL program and must be specified in order\n"
            "   to install the product %s.\n\n", fspecFile);
        printStr("   Do you want to re-enter the product name?\n"
            "                     Enter Y or <RETURN> for Yes.\n"
            "                     Enter N for No.  ");
        NoOrChoose();
    }

    skipToMarker();
    if (iRdByte() != 0x1A || iRdWord() != 8) {
        printStr("\n\nVersion mismatch in INS file.\n");
        vexit();
    }

    return execute();

}

/// <summary>
/// Ask to continue and if so get product to process, otherwise exit to system
/// </summary>
void NoOrChoose() {  // 10F6
    if (YNReturn()) {
        printStr("\n\n"
            "           Which MicroPro product would you like to Install?\n"
            "           Enter -  WS for WordStar\n"
            "                 -  WM for WordMaster\n"
            "                 -  DS for DataStar\n"
            "                 -  RS for ReportStar\n"
            "           then press <RETURN>.\n\n"
            " Product? ");
        selectProduct();
    } else {
        printStr("\n Returning you to the system...\n");
        vexit();
        // no return
    }

}


/// <summary>
/// Core intepreter
/// </summary>
/// <returns></returns>
int execute() { // 12C5
      while (1) {
        switch (iRdByte()) {
        case 0x1c:                                  // return 0 from sub
        case 7:                                     // end of list
        case 0x14:   return 0;                      // list break 
        case 0x1b: return 1;                        // return 1 from sub
        case 2: op02_copy(); break;                 // copy file
        case 0x2c: op2c_printNL(); break;           // print n new lines
        case 3: op03_icall(); break;                // invoke a subroutine
        case 4: op04_callIf(); break;               // conditionally invoke subroutine
        case 5: op05_exit(); break;                 // exit to OS
        case 6: op06_patch(); break;                // apply patches in range - not used for WS.INS
        case 8: op08_getUserFileName(); break;      // get user entered filename
        case 9: op09_getUserString(); break;        // get user entered string
        case 0xa: op0a_getDecimalValue(); break;    // get user entered decimal value
        case 0xb: op0b_exec(); break;               // warning broken as it should pass the file to exec
        case 0xc: op0c_pRdVal(); break;             // read a value from the patch file
        case 0xd: op0d_jmpIf(); break;              // conditionally jmp
        case 0xe: op0e_jmp(); break;                // jmp
        case 0xf: op0f_pause(); break;              // pause
        case 0x10: op10_callUsrNo(); break;         // conditionally call on user entering N
        case 0x11: op11_jmpUsrNo(); break;          // conditionally jmp on user entering N
        case 0x12: op12_callUsrYes(); break;        // conditionally call on user entering Y or <RETURN>
        case 0x13: op13_jmpUsrYes(); break;         // conditionally jmp on user entering Y or <RETURN>
        case 0x15: op15_doExpr(); break;            // evaluate simple arithmetic expression
        case 0x16: op16_list(); break;              // start a selection list
        case 0x17: op17_addPatch(); break;          // apply a patch
        case 0x18: op18_iPrintStr(); break;         // print an inline string
        case 0x19: op19_printSVar(); break;         // print a string variable
        case 0x1a: op1a_printInt(); break;          // print an integer variable
        case 0x1d: op1d_menu(); break;              // start a menu
        case 0x1e: op1e_iRdSVar(); break;           // read inline string
        case 0x1f: op1f_openPatchFile(); break;     // open the patch file
        case 0x20: op20_pClose(); break;            // close the patch file
        case 0x21: op21_getHexValue(); break;       // get user entered hex value
        case 0x22: op22_unlink(); break;            // unlink file
        case 0x23: op23_rename(); break;            // rename file
        case 0x24: op24_getChVal(); break;          // get user entered character value -- not used by WS.INS
        case 0x25: op25_jmpIfNotCorrect(); break;   // conditional jmp if not correct
        case 0x26: op26_getFnameSVar(); break;      // get user entered filename & parse into drive, file and ext
        case 0x27: op27_mkFname(); break;           // create filename from component drive, file and ext
        case 0x28: op28_jmpIfCorrect(); break;      // conditional jmp if not correct
        case 0x29: op29_chkAppSig(); break;         // check file signature is at sensible location in patch file
        case 0x2a: op2a_getLocation(); break;       // get user entered location, can use name lookup
        case 0x2b: op2b_iRdInputMode(); break;      // read inputmode
        case 0x2d: op2d_getAsciiChar(); break;          // get user entered ASCII char
        case 0x2e: op2e_readNthCh(); break;         // reads nth char from a string var, with default char if string too short -- not used by WS.INS
        case 0x2f: op2f_testRdPatchFile(); break;   // attempt to read 161 bytes from the patch file at a given location -- not used by WS.INS
        case 0x30: op30_getDriveSVar(); break;      // get user entered drive in X: format
        default: errorLog(8); break;                // bad fileName xcute
        }

    }

}



/// <summary>
/// Get product filename and generate the .INS filename
/// </summary>
void selectProduct() { // 1631
    getUserFileName(userIn);
    parseFspec(userIn);
    strcpy(fileName, fspecDrv);
    strcat(fileName, fspecFile);
    strcat(fileName, ".INS");
    printStr("\n\n");

}



/// <summary>
/// Show a list of options and continue based on the user
/// selection
/// </summary>
void op16_list() { // 16A2
    int startIp;
    uint8_t showList;

    startIp = itell();
    showList = iRdByte() ? 1 : 0;
    iUngetc();
    while (1) {
        if (showList) {
            putStringList();
            putchar('\n');
            while (iGetItem())
                putStringList();
        }

        done = false;
        while (!done) { //1780
            userListSelection = mpGetch();      // what user entered
            iSeek(startIp);                     // search from start of code
            skipToMarker();                     // skip header text
            if (userListSelection == '\r') {    // <RETURN> is default option
                if (showList == 0)
                    printStr("<RETURN>\n\n");
                break;
            } else {
                while ((listSelector = iGetItem())) {   // skip code block to next list option
                    skipToMarker();                     // skip its header text
                    if (toupper(userListSelection) == listSelector) {   // do we have a match?
                        if (showList) {                 // echo the response
                            putchar(listSelector);
                            printStr("\n\n");
                        }

                        done = 1;
                        break;
                    }

                }

            }

        }

        if (execute())                          // invoke the selected code
            iSeek(startIp);                     // if it fails retry
        else {
            iUngetc();                          // skip the rest of the selections
            while (iGetItem())
                skipToMarker();
            return;                             // all done
        }

    }


}


/// <summary>
/// Show a menu, allowing navigation between pages
/// When selected, show any additional information
/// and if all ok, apply the patches
/// </summary>
void op1d_menu() { // 17FE
    int8_t ch, userCh;
    unsigned pInfoLoc;
    int i;

    _SelectionDone = false;
    dSeek(iRdWord());
    locPatchICode = itell();
    skipToMarker();
    locPatchEncoding = itell();
    nMenuItems = 0;
    while (nMenuItems < 108) {          // max 108 menu options
        menus[nMenuItems++] = dtell();  // record where info for current item starts
        iSeek(locPatchEncoding);        // set ip to start of patch encoding info
        iGetRFld(7);                    // skip to start of next info
        if (dRdByte() == 0x1A)          // end of menu options
            break;
        dUngetc();
    }

    nPages = nMenuItems / MENU_ITEMS_PER_PAGE;
    if (nMenuItems % MENU_ITEMS_PER_PAGE)
        nPages++;
    pageNo = 1;
    done = 0;
    while (done == 0) { // 1ABB (189f)
        showMenu();                     // display the current menu
        while (1) { // (18AA
            if (isdigit(userCh = mpGetch())) { // 1946  page select?
                if ((userCh -= '0') > 0 && userCh <= nPages && userCh != pageNo) { // 1943 - select if page changed
                    putchar((pageNo = userCh) + '0');
                    for (i = 0; i < 20; i++)
                        putchar('\n');
                } else
                    continue;
            } else {
                userCh = toupper(userCh);       // check for letter select
                if (isalpha(userCh)) { // 1A79
                    if ((userCh -= 'A') < nCurMenuItems) { // 1A76    - valid letter?
                        printf("%c\n\n", userCh + 'A');
                        printf("You have chosen : ");
                        dSeek(pInfoLoc = menus[(pageNo - 1) * MENU_ITEMS_PER_PAGE + userCh]);
                        while (ch = dRdByte())        // print the menu item text
                            putchar(ch);
                        printStr("\n");
                        iSeek(locPatchEncoding);        // ip = start of encoding info
                        dSeek(pInfoLoc);                // rip = start of the patch info
                        while (iGetRFld(0x18) != -1) {  // look for supplementary info
                            if (dRdByte()) {            // and show it if not 0 length
                                dUngetc();
                                while (ch = dRdByte())
                                    putchar(ch);
                                putchar('\n');
                            }

                        }

                        putchar('\n');
                        printStr("    If this is correct, enter Y or <RETURN>.  ");
                        printStr("If not, enter N. ");
                        if (YNReturn()) {
                            processPatches(pInfoLoc);
                            _SelectionDone = true;
                            done = true;
                        }

                        printStr("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
                        break;
                    } else
                        continue;
                } else if (userCh == '\r') {
                    printStr("<RETURN>\n\n");
                    printStr("    If this is correct, enter Y or <RETURN>.  ");
                    printStr("If not, enter N. ");
                    if (YNReturn())
                        done = true;
                    printStr("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
                } else
                    continue;
            }

            break;
        }

    }

    dSeek(locCurMenuText);      // rip -> start of patch info
    iSeek(locPatchEncoding);    // ip -> to start of menu code
    iGetRFld(7);                // skip to end of encoding info

}



/// <summary>
/// skip code instructions until op14 or op07 seen
/// </summary>
/// <returns>for op14 returns first non blank char inline
/// for op7 returns 0</returns>
int iGetItem() { // 1BCD
    int c;
    while (1) {
        switch (iRdByte()) {
        case 0x16:
            skipToMarker();
            while (iGetItem())
                ;
            skipToMarker();
            break;
        case 4: case 0xd:
            iRdByte();
        case 0x15:
            iRdByte();
        case 0xc:
            iRdWord();
            iRdWord();
        case 3: case 0xe: case 0x11: case 0x13: case 0x10:
        case 0x12: case 2: case 0x1A: case 0x23: case 0x25:
        case 0x28:
            iRdByte();
        case 8: case 0x30: case 0x19: case 0xa: case 9:
        case 0xb: case 0x1f: case 0x22: case 0x24:
        case 0x2b: case 0x21: case 0x2c: case 0x2d: case 0x2f:
            iRdByte();
        case 0x1b: case 5: case 0xf: case 0x1c: case 0x20:
            break;
        case 0x2a:
            iRdByte();
            iRdWord();
            break;
        case 6: case 0x26: case 0x27: case 0x2e:
            iRdWord();
            iRdWord();
            break;
        case 0x29:
            iRdWord();
            iRdWord();
            iRdWord();
            iRdWord();
            break;
        case 0x1e:
            iRdByte();
            iRdByte();
        case 1: case 0x18:
            skipToMarker();
            break;
        case 0x17:
            iRdWord();
            iRdWord();
            iRdByte();
            if ((c = iRdByte()) == 0)
                c = 1;
            while (c > 0) {
                iRdByte();
                c--;
            }

            break;
        case 0x14:
            c = itell();
            while ((listSelector = iRdByte()) == ' ')
                ;
            iSeek(c);
            return (toupper(listSelector));
        case 7:
            return 0;
        default:
            errorLog(10);   // bad fileName getitem
            break;
        }

    }

}


/// <summary>
/// skip to next field in the patch info
/// </summary>
/// <param name="n">field type to skip to</param>
/// <returns>0 if found else -1</returns>
int iGetRFld(int n) {   // 1EFA
    uint8_t var3;
    int var2;

    while ((var3 = iRdByte()) != n) {
        if (var3 == 0x18)
            while (dRdByte())
                ;
        else if (var3 == 0x17) {
            iRdWord();
            iRdByte();
            iRdByte();

            if ((var2 = iRdByte()) == 0)
                var2 = dRdByte();
            while (var2) {
                dRdByte();
                var2--;
            }

        } else if (var3 == 0x14) {
            iRdWord();
            iRdByte();
            iRdByte();
            while (dRdByte())
                ;

        } else if (var3 == 7)
            return -1;
        else
            errorLog(0xb);  // bad fileName getrfld
    }

    return 0;
}



/// <summary>
/// show the menu in two column format
/// </summary>
void showMenu() {  // 1FDD
    int n, row, var7, col2Start;
    int *pMenu;
    uint8_t var1;

    iSeek(locPatchICode);
    putStringList();
    if (nPages > 1) {
        printf(" #%d of %d;", pageNo, nPages);
        printf(" to view another menu press the\n");
        printf("appropriate number.");
    }

    printStr("\n");
    nCurMenuItems = (pageNo == nPages && nMenuItems % MENU_ITEMS_PER_PAGE) ? nMenuItems % MENU_ITEMS_PER_PAGE : MENU_ITEMS_PER_PAGE;
    pMenu = &menus[(pageNo - 1) * MENU_ITEMS_PER_PAGE]; 
    col2Start = nCurMenuItems / 2 + nCurMenuItems % 2;

    for (n = row = 0; n < nCurMenuItems;  n++) {
        row = n / 2;
        if (n % 2) {                                // show the selection letter
            putchar(' ');
            locCurMenuText = pMenu[row + col2Start];    // where the patch info is
            var1 = row + 'A' + col2Start;
        } else {
            printStr("\n");
            locCurMenuText = pMenu[row];            // where the patch info is
            var1 = row + 'A';
        }

        printf("%c ", var1);
        dSeek(locCurMenuText);                      // show the selection text
        for (var7 = 27; var7 > 0 && (var1 = dRdByte()); var7--)
            putchar(var1);
        for (; var7; var7--)
            putchar(' ');
    }

    printStr("\n\n");
    printf("   Enter the letter of  your choice,\n");
    if (nPages > 1)
        printf("   or enter the appropriate menu number,\n");
    printf("   or press <RETURN> to leave uncahnged. ");
}



/// <summary>
/// patch within a range
/// </summary>
void op06_patch() {   // 22C4 patch - not used for ws.ins
    lowPatchRange = iRdWord();
    highPatchRange = iRdWord();
    patchSubTree(pTree);
    lowPatchRange = 0;
    highPatchRange = 0;
}


/// <summary>
/// add a patch and apply
/// </summary>
void op17_addPatch() { // 22FC
    int j, i, patchLen, type;

    type = iRdByte();
    node.loc = iRdWord();
    patchLen = iRdByte();
    node.flags = iRdByte();
    node.len = iRdByte();
    if (type & 1) { // 240E
        j = iRdByte();
        switch (node.len) {
        case 2:
            userIn[1] = iVar[j] >> 8;
        case 1:
            userIn[0] = iVar[j];
            break;
        case 0:
            for (i = 0; i < 40; i++)
                userIn[i] = ' ';
            node.len = (uint16_t)strlen(sVar[j]);
            strcpy(userIn, sVar[j]);
            userIn[node.len] = ' ';
            node.len = patchLen;
        }
    } else { // 2472
        for (i = 0; i < 40; i++)
            userIn[i] = ' ';
        for (j = 0; j < node.len; j++)
            userIn[j] = iRdByte();
        node.len = patchLen;
    }
    


    if (type & 2)
        node.loc = iVar[node.loc];

    node.left = node.right = 0;
    pTree = addNode(pTree, &node);
    walkPatchTree();
}


/// <summary>
/// apply all the patches for a menu selection
/// </summary>
/// <param name="n"></param>
void processPatches(int n) { // 24E2
    int i, unused;
    char *s;
    char buf[258];

    iSeek(locPatchEncoding);
    dSeek(n);
    iRdByte();
    node.loc = iRdWord();
    node.flags = iRdByte();
    s = buf;
    while (*s++ = dRdByte())                        // get header text
        ;
    if (node.len = iRdByte()) { // 2586             // centre in field
        setmem(userIn, node.len, ' ');
        printf("strlen(buf) = %d, node.len = %d\n", (unsigned)strlen(buf), node.len);
        movmem(buf, userIn + (node.len - strlen(buf)) / 2, strlen(buf));
    } else {
        node.len = (uint16_t)strlen(buf) + 1;
        strcpy(userIn, buf);
    }

    node.left = node.right = 0;
    if (node.loc)                                   // add name patch
        pTree = addNode(pTree, &node);
    iSeek(locPatchEncoding);
    dSeek(n);
    while (iGetRFld(0x17) != -1) { // 265b          process all of the patch fields (ignore msg fields)
        node.loc = iRdWord();
        unused = iRdByte();
        node.flags = iRdByte();
        if ((node.len = iRdByte()) == 0) {          // pickup length from patch info if variable
            node.len = dRdByte() + 1;               // adjust for the length byte itself
            dUngetc();
        }

        for (i = 0; i < node.len; i++)
            userIn[i] = dRdByte();
        node.left = node.right = 0;
        pTree = addNode(pTree, &node);
    }

    walkPatchTree();
}


void patchSubTree(PatchNode *p) { // 2670
    if (p) {
        patchSubTree(p->left);
        patchNode(p);
        patchSubTree(p->right);
    }

}


/// <summary>
/// create a new patch node
/// </summary>
/// <param name="p">target subtree</param>
/// <param name="q">source subtree</param>
/// <returns></returns>
PatchNode *addNode(PatchNode *p, PatchNode *q) { // 26BA


    if (p == NULL) { // 2737
        if ((p = alloc(q->len + sizeof(PatchNode))) == NULL)
            errorLog(6);    // out of memory in treeptch
        movmem(q, p, sizeof(PatchNode));
        movmem(userIn, p->bytes, p->len);
        return p;
    }

    if ((locDiff = q->loc - p->loc) == 0) {
        q->left = p->left;                  // replace existing node
        q->right = p->right;
        free(p);
        return addNode(0, q);
    }

    if (locDiff < 0)
        p->left = addNode(p->left, q);      // add to left node
    else
        p->right = addNode(p->right, q);    // add to right node
    return p;
}



/// <summary>
/// execute outstanding patches
/// walks the patch tree so that patches are done in order
/// which will reduce disk seeks
/// </summary>
void walkPatchTree() { // 27F8
    patchSubTree(pTree);
}


/// <summary>
/// apply a patch, if loc != and it is in range (or no range set) and not already applied
/// </summary>
/// <param name="p"></param>
void patchNode(PatchNode *p) { // 280C
    int patchLen;
    uint8_t *patchBytes;

    if (p->loc &&         // loc valid
          (highPatchRange == 0
            || ((lowPatchRange <= p->loc) & (p->loc <= highPatchRange))) &&   // loc in range
          (p->flags & (1 << (patchFlagBit - 1)))) {                           // check if patch needs applying

        p->flags &= ~(1 << (patchFlagBit - 1));     // clear the flag for next time
        pSeek(p->loc + patchBias);                  // find location in file
        patchBytes = p->bytes;                            // write the patch
        for (patchLen = p->len; patchLen > 0; patchLen--)
            pPutc(*patchBytes++);
    }
}

#ifdef BSDC

void pSeek(unsigned n) {    // 293C
    if ((n >= patchfp._hiMark || n < patchfp._hiMark - patchfp._nsecs * 128) && patchfp._nsecs) {
        if (seek(patchfp._fd, patchfp._hiMark / 128 - patchfp._nsecs, 0) == -1)
            errorLog(7);    // patch seek
        if (write(patchfp._fd, patchfp._buf, patchfp._nsecs) == -1)
            errorLog(8);    // patch write
    }

    if (Fseek(patchfp, n, 0) == -1)
        errorLog(7);        // patch seek
}



uint16_t pPutc(uint8_t n) { // 29F5
    if (patchfp._nleft--) {
        *patchfp._nextp++ = n;
        return n;
    }

    pSeek(patchfp._hiMark + 1);
    return pPutc(n);
}

#endif


void op0f_pause() { // 2a42
    int i;

    printStr("\nType any key to continue... ");
    mpGetch();
    for (i = 0; i < 28; i++)    // erase the message
        printStr("\b \b");
}

#ifdef BDSC
void initCtrlStr() { //2AA7
    ctrlChar[0] = "NUL";
    ctrlChar[1] = "SOH";
    ctrlChar[2] = "STX";
    ctrlChar[3] = "ETX";
    ctrlChar[4] = "EOT";
    ctrlChar[5] = "ENQ";
    ctrlChar[6] = "ACK";
    ctrlChar[7] = "BEL";
    ctrlChar[8] = "BS";
    ctrlChar[9] = "HT";
    ctrlChar[0xa] = "LF";
    ctrlChar[0xb] = "VT";
    ctrlChar[0xc] = "FF";
    ctrlChar[0xd] = "CR";
    ctrlChar[0xe] = "SO";
    ctrlChar[0xf] = "SI";
    ctrlChar[0x10] = "DLE";
    ctrlChar[0x11] = "DC1";
    ctrlChar[0x12] = "DC2";
    ctrlChar[0x13] = "DC3";
    ctrlChar[0x14] = "DC4";
    ctrlChar[0x15] = "NAK";
    ctrlChar[0x16] = "SYN";
    ctrlChar[0x17] = "ETB";
    ctrlChar[0x18] = "CAN";
    ctrlChar[0x19] = "EM";
    ctrlChar[0x1a] = "SUB";
    ctrlChar[0x1b] = "ESC";
    ctrlChar[0x1c] = "FS";
    ctrlChar[0x1d] = "GS";
    ctrlChar[0x1e] = "RS";
    ctrlChar[0x1f] = "US";
}

#endif

/// <summary>
/// Parse a CP/M file spec
/// X:8.3 format
/// </summary>
/// <param name="s">string to parse</param>
/// <returns>0 or error code</returns>
int parseFspec(char *s) { // 2be6
    int extLen, nameLen;
    bool haveDot;

    nameLen = extLen = haveDot = 0;
    if (s[1] == ':') { // 2c64
        if (toupper(*s) - '@' >= 17)
            return INVALIDDISK;
        fspecDrv[0] = *s;
        fspecDrv[1] = ':';
        fspecDrv[2] = 0;
        s += 2;
    } else
        fspecDrv[0] = 0;

    do {
        if (!isFileChar(*s)) {
            if (*s == '.') {
                if (!haveDot)
                    haveDot = true;
                else
                    return MULTIPLEDOT;
            }

        } else if (haveDot) {
            if (extLen >= 3)
                return EXTTOOLONG;
            fspecExt[extLen++] = *s;
        } else if (nameLen >= 8)
            return NAMETOOLONG;
        else
            fspecFile[nameLen++] = *s;

    } while (*++s);
    if (nameLen < 1)
        return NAMETOOSHORT;
    fspecExt[extLen] = fspecFile[nameLen] = 0;
    return FNAMEOK;
}


/// <summary>
/// skip strings in code
/// strings end in 0 and subsequent strings start with 1
/// </summary>
void skipToMarker() { // 2d5d
    char c;
    while ((c = iRdByte()) != 0 || iRdByte() == 1)
        ;
    iUngetc();
}

#ifdef BDSC

int iRdByte() { // 2d92
    if ((iByte = getc(insfp)) == -1)
        errorLog(1);    // fileName fetch
    return iByte;
}


int iRdWord() { // 2db9
    int var2 = iRdByte();
    return var2 + (iRdByte() << 8);
}


#endif

/// <summary>
/// wait for Y, N or <return>, echo what is pressed.
/// </summary>
/// <returns>1 if Y or <return> else 0</returns>
int YNReturn() { // 2df2
    char c;
    while (1) {

        if ((c = toupper(mpGetch())) == 'Y') {
            printStr("Y\n\n");
            return 1;
        }

        if (c == '\r') {
            printStr(" <RETURN>\n\n");
            return 1;
        }

        if (c == 'N') {
            printStr("N\n\n");
            return 0;
        }

    }



}


/// <summary>
/// create a copy of a file
/// used to create working patch file
/// unless BDSC, this version uses cpmFopen, fread, fwrite and fclose rather than the
/// non-standard BDSC read/write
/// Note cpmFopen maps drive X: to directory X/ except for A: (and @:) which map to ./
/// </summary>
void op02_copy() {       // copy 2E89
    int dstStrIdx = iRdByte();
    int srcStrIdx = iRdByte();


    int nblk;
#ifdef BDSC
    int dst, src;
#else
    FILE *dst, *src;
#endif
    int rdCnt, wrCnt;
    uint8_t *buf;

    for (nblk = 192; nblk; nblk -= 8)       // check for largest free spece (CP/M limited)
        if (buf = alloc(nblk * 128))        // mapped to malloc in fileio.h
            break;
    if (nblk == 0)
        errorLog(5);    // out of memory in copy
    _Error = true;
    _CreateError = _OpenError = _NameError = _ReadError = _WriteError = _DiskFull = false;
    if (parseFspec(sVar[dstStrIdx]) != 0 || parseFspec(sVar[srcStrIdx]) != 0) {
        _NameError = true;
        free(buf);
    }
#ifdef BDSC
    else if ((dst = creat(sVar[dstStrIdx])) == 0) {
        iVar[R_CREATEFAIL] = true;
        free(buf);
    } else if ((src = open(sVar[srcStrIdx], 0)) == 0) {
        iVar[R_OPENFAIL] = true;
        free(buf);
    }
#else
    else if ((dst = cpmFopen(sVar[dstStrIdx], "wb")) == NULL) {
        _CreateError = true;
        free(buf);
    } else if ((src = cpmFopen(sVar[srcStrIdx], "rb")) == NULL) {
        _OpenError = true;
        free(buf);
    }
#endif
    else {
        w8C50 = w8C52 = w8C54 = 0;          // not used anywhere else
        while (1) {
#ifdef BDSC
            if ((rdCnt = read(src, buf, nblk)) > 0) {
                if ((wrCnt = write(dst, buf, rdCnt)) == 0) {
#else
            if ((rdCnt = (int)fread(buf, 1, nblk * 128, src)) > 0) {
                if ((wrCnt = (int)fwrite(buf, 1, rdCnt, dst)) == 0) {

#endif
                    _WriteError = true;
                    break;
                } else if (wrCnt != rdCnt) {
                    _DiskFull = true;
                    break;
                }

            } else {
#ifdef BDSC
                if (rdCnt == -1)
#else
                if (ferror(src))
#endif
                    _ReadError = true;
                else
                    _Error = false;
                break;
            }

        }

        free(buf);
#ifdef BDSC
        close(src);
        close(dst);
#else
        fclose(src);
        fclose(dst);
#endif
    }

}

/// <summary>
/// Invoke a subroutine
/// </summary>
void op03_icall() { // icall 3087
    unsigned ret = itell() + 2;

    iSeek(iRdWord());
    execute();
    iSeek(ret);
}


/// <summary>
/// call conditionally based on relTest
/// </summary>
void op04_callIf() {    // 30C3

    if (relTest())
        op03_icall();
    else
        iRdWord();  // skip call location
}

/// <summary>
/// exit
/// Note vexit() is mapped to exit(0)
/// </summary>
void op05_exit() {            // 30DD
    vexit();
}


/// <summary>
/// get user filename into string variable
/// </summary>
void op08_getUserFileName() {       // 30EC -- not used for ws.ins
    getUserFileName(sVar[iRdByte()]);
}


/// <summary>
/// get user string into string variable
/// </summary>
void op09_getUserString() {       // 310A
    getUserString(sVar[iRdByte()]);
}



/// <summary>
/// get a value info variable, defaults to DECIMAL
/// </summary>
void op0a_getDecimalValue() {       // 3125
    getValue(userIn, DECIMAL);
}


/// <summary>
/// read a value from the patch file
/// </summary>
void op0c_pRdVal() {   // 3144
    int i;  //e
    int ch; //c
    int type; //a
    int rFlag; //8
    int reg; //6
    int unused; //4
    int offset; //2

    _Error = true;
    _NameError = _OpenError = _ReadError = false;
    type = iRdByte();
    rFlag = iRdByte();
    reg = iRdByte();
    offset = iRdWord();
    unused = iRdByte();

    if (rFlag)
        offset = iVar[offset];
    pSeek(patchBias + offset);
    if (type == 0) {
        for (i = 0; i < 39; i++) {
            if ((ch = pRdByte(patchfp)) == 0) {
                _ReadError = 1;
                return;
            }

            if (ch >= 0x20 && ch < 0x7f || ch == '\n' || ch == '\r' || ch == '\t')
                sVar[reg][i] = ch;
            else
                break;
        }

        sVar[reg][i] = 0;
    } else if ((iVar[reg] = pRdByte(patchfp)) == -1) {
        _ReadError = 1;
        return;
    } else {
        if (type == 2) {
            if ((ch = pRdByte(patchfp)) == -1) {
                _ReadError = 1;
                return;
            } else
                iVar[reg] += ch << 8;
        }
    }


    _Error = false;
}



/// <summary>
/// jmp conditionally based on relTest
/// </summary>
void op0d_jmpIf() { // 330F
    if (relTest())
        op0e_jmp();
    else
        iRdWord();
}


/// <summary>
/// unconditional jmp
/// </summary>
void op0e_jmp() { // 332C
    iSeek(iRdWord());
}


/// <summary>
/// call if user selects N -- not used by ws.ins
/// </summary>
void op10_callUsrNo() { // 3346
    printStr(pConfirm);
    if (usrY())
        iRdWord();
    else
        op03_icall();
}

/// <summary>
/// jmp if user selects N -- not used by ws.ins
/// </summary>
void op11_jmpUsrNo() { // 3371
    printStr(pConfirm);
    if (usrY())
        iRdWord();
    else
        iSeek(iRdWord());
}

/// <summary>
/// call if user selects Y or <RETURN> - not used by ws.ins
/// </summary>
void op12_callUsrYes() { // 33A1
    printStr(pConfirm);
    if (usrY())
        op03_icall();
    else
        iRdWord();
}


/// <summary>
/// jmp if user selects Y or <RETURN> - not used by ws.ins
/// </summary>
void op13_jmpUsrYes() { // 33CC
    printStr(pConfirm);
    if (usrY())
        iSeek(iRdWord());
    else
        iRdWord();
}


/// <summary>
/// evaluate asimple expression into variable
/// note, WS.INS contains many examples of iVar[x] = val + 0
/// as this appears to be a common way of loading inline constants
/// </summary>
void op15_doExpr() { // 33F6
    int reg, lhs, rhs, op, rflag;

    reg = iRdByte();
    rflag = iRdByte();
    op = iRdByte();
    lhs = iRdWord();
    rhs = iRdWord();

    if (rflag & 1)
        lhs = iVar[lhs];
    if (rflag & 2)
        rhs = iVar[rhs];
    switch (op) {
    case '+': iVar[reg] = lhs + rhs; break;
    case '*': iVar[reg] = lhs * rhs; break;
    case '-': iVar[reg] = lhs - rhs; break;
    case '/': iVar[reg] = lhs / rhs; break;
    }

}


/// <summary>
/// print an inline string
/// </summary>
void op18_iPrintStr() { // 3542
    unsigned char c;
    while (c = iRdByte())
        putchar(c);
}


/// <summary>
/// print a string variable
/// </summary>
void op19_printSVar() { // 3575
    printStr(sVar[iRdByte()]);
}



/// <summary>
/// read an inline string into a string variable
/// </summary>
void op1e_iRdSVar() { // 3593
    int i, dstIdx, isInline;
    dstIdx = iRdByte();
    isInline = iRdByte();

    if (isInline == 1)
        for (i = 0; i < 40 && (sVar[dstIdx][i] = iRdByte()); i++)
            ;
    else {
        strcpy(sVar[dstIdx], sVar[iRdByte()]);
        iRdByte();      // ignore byte
    }
}


/// <summary>
/// open the file named by string variable for patching 
/// </summary>
void op1f_openPatchFile() { // 363C
    int nameIdx;

    _Error = true;
    _NameError = _OpenError = 0;
    patchFlagBit = nameIdx = iRdByte();
    if (patchFileOpen == true)
        _OpenError = 1;
    else if (parseFspec(sVar[nameIdx]))
        _NameError = 1;
#ifdef BDSC
    else if ((patchfp._fd = open(sVar[var2]), 0) == -1)
        iVar[R_OPENFAIL] = 1;
    else {
        patchfp._hiMark = patchfp._nleft = patchfp._nsecs = 0;
        patchFileOpen = 1;
        _Error = false;
    }
#else
    else if ((patchfp = cpmFopen(sVar[nameIdx], "rb+")) == NULL)
        _OpenError = 1;
    else {
        patchFileOpen = true;
        _Error = false;
    }
#endif



}

/// <summary>
/// close the patchfile
/// </summary>
void op20_pClose() {   // 36EA
    _Error = true;
    _ReadError = _WriteError = 0;
    if (!patchFileOpen) {
        _OpenError = 1;
        return;
    }
#ifdef BDSC
    if (patchfp._nsecs) {
        if (seek(patchfp._fd, patchfp._hiMark / 128 - patchfp._nsecs, 0) == -1) {
            iVar[R_FILEERROR] = 1;
            return;
        }

        if (write(patchfp_.fd, patchfp._buf, patchfp._nsects) == -1) {
            iVar[R_WRITEFAIL] = 1;
            return;
        }

    }
    _Error = false;
    close(patchfp._fd);
#else
    if (fclose(patchfp)) {
        _WriteError = true;
        return;
    }
    _Error = false;
#endif

    patchFileOpen = false;
}


/// <summary>
/// get a value info variable, defaults to HEX
/// </summary>
void op21_getHexValue() {   // 3778
    getValue(userIn, HEX);
}


/// <summary>
/// unlink file named by string variable
/// </summary>
void op22_unlink() {   // 378E
    int i;

    _Error = true;
    _NameError = _UnlinkError = false;
    i = iRdByte();
    if (parseFspec(sVar[i]))
        _NameError = true;
    else if (unlink(sVar[i]) < 0)
        _UnlinkError = true;
    else
        _Error = false;
}


/// <summary>
/// rename file based on names in string variables
/// </summary>
void op23_rename() {   // 380B
    int i, j;
    _Error = true;
    _NameError = _RenameError = false;
    i = iRdByte();
    j = iRdByte();
    if (parseFspec(sVar[i]) != 0 || parseFspec(sVar[j]) != 0)
        _NameError = true;
    unlink(sVar[j]);
    if (rename(sVar[i], sVar[j]) < 0)
        _RenameError = true;
    else
        _Error = false;
}

/// <summary>
/// get an ascii char value into an integer variable
/// </summary>
void op24_getChVal() {   // 38D1 -- not used by ws.ins
    gets(userIn);
    iVar[iRdByte()] = userIn[0] == '-' ? 0 : getChVal();
}


/// <summary>
/// conditional jmp if user says not correct
/// </summary>
void op25_jmpIfNotCorrect() {   // 3911 jmpIfNotCorrect
    printStr("    If this is correct, enter Y or <RETURN>. ");
    printStr("If not, enter N. ");
    if (!YNReturn())
        iSeek(iRdWord());
    else
        iRdWord();
}


/// <summary>
/// get user entered filename into string variable
/// parse the string and set individual string variables
/// for drive, file and ext
/// </summary>
void op26_getFnameSVar() {   // 3998 getAndParseFname
    int i1, i2, i3, i4;
    i1 = iRdByte();
    i2 = iRdByte();
    i3 = iRdByte();
    i4 = iRdByte();
    _Error = true;
    _NameError = false;
    getUserFileName(sVar[i1]);
    if (parseFspec(sVar[i1]))
        _NameError = true;
    else {
        strcpy(sVar[i2], fspecDrv);
        strcpy(sVar[i3], fspecFile);
        strcpy(sVar[i4], fspecExt);
        _Error = false;
    }

}


/// <summary>
/// concat 3 string variables to form a new filename
/// </summary>
void op27_mkFname() {       // 3A6D mkFname
    int i1, i2, i3, i4;
    i1 = iRdByte();
    i2 = iRdByte();
    i3 = iRdByte();
    i4 = iRdByte();
    strcpy(sVar[i1], sVar[i2]);
    strcat(sVar[i1], sVar[i3]);
    strcat(sVar[i1], sVar[i4]);
}


/// <summary>
/// conditional jmp if user says correct or enters return
/// </summary>
void op28_jmpIfCorrect() {       // 3B20 jmpIfCorrect
    printStr("    If this is correct, enter Y or <RETURN>. ");
    printStr("If not, enter N. ");
    if (YNReturn())
        iSeek(iRdWord());
    else
        iRdWord();       // junk target address
}

/// <summary>
/// simple error log and exit
/// if debug set then display a little more info
/// </summary>
/// <param name="n">error code</param>
void errorLog(int n) { // 3B98
    char *errStr;

    switch (n) {
    case 1: errStr = "Token fetch."; break;
    case 2: errStr = "Data fetch."; break;
    case 3: errStr = "Token seek."; break;
    case 4: errStr = "Data seek."; break;
    case 5: errStr = "Out of memory in copy."; break;
    case 6: errStr = "Out of memory in treeptch."; break;
    case 7: errStr = "Patch seek"; break;
    case 8: errStr = "Patch write."; break;
    case 9: errStr = "Bad  fileName xcute"; break;
    case 10: errStr = "Bad fileName getitem"; break;
    case 11: errStr = "Bad fileName getrfld"; break;
    default: errStr = "";
    }

    printf("\n\n  A fatal error occured while processing %s", fileName);
    printf("\n  This error may be due to a disk problem, or perhaps");
    printf("\n  the device definition  file you  are using has been");
    printf("\n  corrupted. In any case this session cannot continue");
    printf("\n  so you are being returned to the operating system.");
    if (debug) {
        printf("  Error: %s\n", errStr);
        printf("   PC  = %04xh *PC  = %02xh\n", itell(), iByte);
        printf("   RPC = %04xh *RPC = %02xh\n\n", dtell(), dByte);
    }

    vexit();
}


/// <summary>
/// managed user entry of a CP/M filename
/// </summary>
/// <param name="s">where the name is stored</param>
void getUserFileName(char *s) { // 3F45
    int ch, fspecLen, extLen, nameLen;
    bool haveDot;
    char *bsb;

    fspecLen = extLen = nameLen = haveDot = 0;
    bsb = "\b \b";
    while (1) {
        switch (ch = mpGetch()) {
        case ':':
            if (fspecLen != 1)
                goto bell;
            else if (userIn[0] <= 'P')
                putchar(userIn[fspecLen++] = ':');
            else {
                printStr(bsb);
                userIn[--fspecLen] = 0;
                goto bell;
            }

            break;
        case '.':
            if (nameLen == 0)       // bug? should be nameLen == 0 || haveDot
                goto bell;
            putchar(userIn[fspecLen++] = '.');
            haveDot = true;
            extLen = 0;
            break;
        case '\b': case 0x7f:
            if (fspecLen == 0)
                break;
            printStr(bsb);
            switch (userIn[--fspecLen]) {
            case ':': nameLen = 1; break;
            default:
                if (haveDot)
                    extLen--;
                else
                    nameLen--;
                break;
            case '.':
                haveDot = false;
                extLen = 0;
                break;
            }

            break;
        case 0x19: case 0x18:
            while (fspecLen-- != 0)
                printStr(bsb);
            fspecLen = nameLen = extLen = haveDot = 0;
            break;
        case '\r':
            if (fspecLen == 0)
                printStr("<RETURN>\n");
            else
                strcpy(s, userIn);
            return;
        default:
            if (isFileChar(ch = toupper(ch))) {
                if (haveDot) {
                    if (extLen >= 3)
                        goto bell;
                    else
                        extLen++;
                } else if (nameLen >= 8)
                    goto bell;
                else
                    nameLen++;
                putchar(userIn[fspecLen++] = ch);
                break;
            }

        bell:           putchar('\a');
            break;
        }

        userIn[fspecLen] = 0;
    }

}

#ifdef BDSC

unsigned itell() {    // 4206
    return Ftell(insfp);
}


void iUngetc() { // 4219
    ungetc(iByte, insfp);
}

#endif


/// <summary>
/// print a list of strings
/// </summary>
void putStringList() { // 4238
    do {
        putchar('\n');
        op18_iPrintStr();
    } while (iRdByte() == 1);
    iUngetc();
}

#ifdef BDSC

int mpGetch() { // 425E - reimplemented in fileio.c
    char c;

    if ((c = bios(3)) == 3 && inputMode != 3) { // control C & not rawmode
        printStr("\nReturning you to the system.\n");
        vexit();
    }

    return c;
}



void iSeek(unsigned n) { // 42C7
    if (Fseek(n, insfp) == 0)
        errorLog(3);    // token seek
}


void dSeek(unsigned n) { // 42FB
    if (Fseek(n, datafp) == 0)
        errorLog(4);    // data seek
}



unsigned dtell() { // 432C
    call Ftell(datafp);
}



int dRdByte() { // 4342
    if ((dataChar = getc(datafp)) == -1)
        errorLog(2);    // data fetch
    return dataChar;
}


void dUngetc() { // 4369
    ungetc(dataChar, datafp);
}

#endif

bool isFileChar(int ch) { // 4379
    char *s;
    if (ch <= ' ')
        return false;
    s = "_<>.,;:=?*[]";
    do {
        if (ch == *s)
            return 0;
    } while (*s++);      // allows '\0' as a valid char
    return true;
}



/// <summary>
/// evaluate a simple relational test
/// </summary>
/// <returns></returns>
bool relTest() { // 43ED
    int lhsVal;
    int rhsVal;
    int condFlag;
    int varFlag;
    bool result;

    varFlag = iRdByte();
    lhsVal = iRdWord();
    rhsVal = iRdWord();
    condFlag = iRdByte();
    result = 0;

    if (varFlag & 1)
        lhsVal = iVar[lhsVal];
    if (varFlag & 2)
        rhsVal = iVar[rhsVal];
    if (condFlag & 1)
        result |= lhsVal == rhsVal;
    if (condFlag & 2)
        result |= lhsVal < rhsVal;
    if (condFlag & 4)
        result |= lhsVal > rhsVal;
    return result;
}


/// <summary>
/// get user string (up to 39 chars saved)
/// </summary>
/// <param name="s"></param>

void getUserString(char *s) { // 4531
    int i;
    gets(userIn);
    for (i = 0; i < 39 && (*s++ = userIn[i]); i++)
        ;
    *s = 0;
}


/// <summary>
/// print Y/N and get user response
/// </summary>
/// <returns></returns>
int usrY() {   // 459A
    printStr("(Y/N)? ");
    return YNReturn();
}

/// <summary>
/// parse userIn to get char value
/// options include
/// non digit
/// ^ character
/// ctrl char string
/// decimal or hex (H) number
/// </summary>
/// <returns></returns>
int getChVal() {    // 45C4 -- not used by ws.ins
    int i, j, k;

    switch (j = (int)strlen(userIn)) {
    case 0:
        return 0;
    case 1:
        if (!isdigit(userIn[0]))
            return userIn[0];
        break;
    case 2:
        if (userIn[0] == '^')
            return userIn[1] & 0x1f;
        break;
    }

    for (i = 0; i < j; i++)
        userIn[i] = toupper(userIn[i]);

    for (i = 0; i < 32; i++)
        if (strcmp(userIn, ctrlChar[i]) == 0)
            return i;
    if (userIn[j - 1] == 'H')
        k = 16;
    else
        k = 10;
    i = strtoi(userIn, k);
    return i;
}



/// <summary>
/// parse string to integer
/// </summary>
/// <param name="s"></param>
/// <param name="radix"></param>
/// <returns></returns>
int strtoi(char *s, int radix) { //470f
    int digit, val;

    val = 0;
    while ((digit = ch2i(*s++, radix)) != -1)
        val = val * radix + digit;
    return val;
}


/// <summary>
/// convert ascii nibble to integer
/// </summary>
/// <param name="ch">ascii nibble</param>
/// <param name="radix">numeric base</param>
/// <returns>value or -1 if invalid</returns>
int ch2i(int ch, int radix) { //4781
    ch = toupper(ch);
    if (isalpha(ch))
        ch -= '7';
    else if (isdigit(ch))
        ch -= '0';
    else
        return -1;
    if (ch >= radix)
        return -1;
    return ch;
}

/// <summary>
/// print n newlines
/// </summary>
void op2c_printNL() { // 4814
    int i, cnt;
    cnt = iRdByte();
    for (i = 0; i < cnt; i++)
        userIn[i] = '\n';
    userIn[i] = 0;
    printStr(userIn);
}



/// <summary>
/// print formatted integer variable
/// </summary>
void op1a_printInt() {   // 4880
    int var4, var2;
    var2 = iRdByte();
    if (var2 == 0)
        var2 = inputMode;
    switch (var2) {
    case HEX:
        printf("%x", iVar[iRdByte()]);
        putchar('h');
        break;
    case DECIMAL:
        printf("%d", iVar[iRdByte()]);
        break;
    case ASCII:
        var4 = iVar[iRdByte()];
        if (var4 >= 0x100) {
            putVisible(var4 / 0x100);
            var4 &= 0xff;
        }

        putVisible(var4);
        break;
    default:
        badRadix();
        break;
    }

}



// look for a seqence of 4 characters in the first 1024 bytes of the file
// sets iVar[reg to result
// 0 -> not a valid file
// 9 -> version of not supported by installer
// 2 -> .COM file
// 1 -> .CMD file
void op29_chkAppSig() {       // 497E
    int sig1, sig2, sig3, sig4, expectedLoc, reg, var8, loc, cnt, ch;
    _Error = true;
    _ReadError = 0;
    sig1 = iRdByte();
    sig2 = iRdByte();
    sig3 = iRdByte();
    sig4 = iRdByte();
    expectedLoc = iRdWord();
    reg = iRdByte();
    var8 = iRdByte();
    iVar[reg] = cnt = loc = 0;   // assume not a valid file
    pSeek(loc);

    while (cnt < 1024) {
        if ((ch = getc(patchfp)) == -1) {
            _ReadError = 1;
            return;
        }

        cnt++;
        if (ch != sig1)
            continue;
        if ((ch = getc(patchfp)) == -1) {
            _ReadError = 1;
            return;
        }

        cnt++;
        if (ch != sig2) {
            ungetc(ch, patchfp);
            cnt--;
            continue;
        }

        if ((ch = getc(patchfp)) == -1) {
            _ReadError = 1;
            return;
        }

        cnt++;
        if (ch != sig3) {
            ungetc(ch, patchfp);
            cnt--;
            continue;
        }

        if ((ch = getc(patchfp)) == -1) {
            _ReadError = 1;
            return;
        }

        cnt++;
        if (ch != sig4) {
            ungetc(ch, patchfp);
            cnt--;
            continue;
        }

        iVar[reg] = 9;         // assume wrong version
        patchBias = cnt - 4 - expectedLoc;
        if (patchBias == -0x100)
            iVar[reg] = 2;     // its a .COM file
        else if (patchBias == 128)
            iVar[reg] = 1;     // its a .CMD file
        cnt = 1024;
    }

    _Error = false;
}


/// <summary>
/// Get a user entered location
/// allows for entery of named location
/// </summary>
void op2a_getLocation() { ///4bd5
    char *buf = userIn;
    int var2;

    getValue(buf, ASCII);       // get a user entered text string
    if (inputMode != ASCII)     // user switch from ASCII mode so already have result
#ifdef BDSC
        var4 = iRdWord();
#else
        iRdWord();          // iVar already assigned junk the location of the lookup table 
#endif
    else {                  // look up string as a named  offset
        var2 = iRdByte();
        iVar[var2] = namedLocOffset(buf);
    }

}


/// <summary>
/// set the input mode from inline value
/// </summary>
void op2b_iRdInputMode() { // 4C3C setInputMode
    inputMode = iRdByte();
}



/// <summary>
/// get a single ASCII char value
/// </summary>
void op2d_getAsciiChar() {   // 4c4b
    getAscii(userIn, 1);

}


/// <summary>
/// Read the nth char from sVar(sReg) to iVar(iReg)
/// default value is defCh if string too short
/// </summary>
void op2e_readNthCh() { // 4C64
    int sReg, iReg;
    unsigned n;
    uint8_t defCh;

    sReg = iRdByte();
    iReg = iRdByte();
    n = iRdByte();
    defCh = iRdByte();
    iVar[iReg] = defCh;             // assume default char
    if (n <= strlen(sVar[sReg]))    // use string char if long enough
        iVar[iReg] = sVar[sReg][n - 1];
}


/// <summary>
/// Try to read 161 chars from patch file at loc
/// </summary>

void op2f_testRdPatchFile() { // 4D07
    int i, loc;
    uint8_t varA1[161];
    loc = iRdByte();
    loc = iVar[loc] + patchBias;
    pSeek(loc);
    for (i = 0; i < 161; i++) {
        if (1)
            varA1[i] = pRdByte(patchfp);
        else {  // unreachable
            _ReadError = true;
            return;
        }

    }

    _Error = 0;

}


/// <summary>
/// get a drive spec into string variable
/// </summary>
void op30_getDriveSVar() {   // 4D91 getDriveToSlotN
    getDrive(sVar[iRdByte()]);
}


/// <summary>
/// get user entered value
/// allows overriding the default format
/// </summary>
/// <param name="s">buffer used to store entered characters</param>
/// <param name="flag"></param>
void getValue(char *s, int nBytes) {  // 4DD3
    int maxch;
    int oIdx;           // output index
    int col;            // current display column
    bool done; 
    int savedInputMode;
    bool hexRadix, haveType;
    int savedMaxch;
    int ch;         
    int val;        
    char *bsb = "\b \b";

    done = oIdx = col = hexRadix = haveType = 0;
    savedInputMode = inputMode;
    switch (inputMode) {
    case HEX:
        maxch = nBytes == 1 ? 2 : 4;
        break;
    case DECIMAL:
        maxch = nBytes == 1 ? 3 : 5;
        break;
    case ASCII:
        maxch = nBytes == 1 ? 1 : nBytes == 2 ? 2 : 255;
        break;
    default:
        badRadix();
        break;
    }

    savedMaxch = maxch;
    while (!done) {
        ch = mpGetch();
        if (col == 0) {
            switch (ch) {
            case ',':
                inputMode = 1;
                maxch = nBytes == 1 ? 2 : 4;
                goto shared;
            case '#':
                inputMode = 2;
                maxch = nBytes == 1 ? 3 : 5;
                goto shared;
            case ':':
                inputMode = 1;
                maxch = nBytes == 1 ? 1 : nBytes == 2 ? 2 : 255;
                goto shared;
            case '.':
                hexRadix = 1;
        shared:
                col = 1;
                putchar(ch);
                ch = mpGetch();
                break;
            }

        }

        switch (inputMode) {
        case 1:
            switch (ch) {
            case '\b':
            case 0x7f:
                if (col == 0)
                    putchar('\a');
                else {
                    if (--col == 0) {
                        inputMode = savedInputMode;
                        maxch = savedMaxch;
                    }

                    if (haveType == 0) {
                        if (oIdx)
                            oIdx--;
                    } else
                        haveType = 0;
                    if (hexRadix == 1)
                        hexRadix = 0;
                    printStr(bsb);
                }

                break;
            case '\r':
                s[oIdx] = 0;
                if (oIdx == 0) {
                    if (hexRadix == 0) {
                        iVar[iRdByte()] = -1;
                        done = 1;
                    } else {
                        iVar[iRdByte()] = 0x101;
                        done = 1;
                        printStr("\n");
                    }

                } else {
                    val = strtoi(s, 16);
                    iVar[iRdByte()] = val;
                    done = 1;
                    printStr("\n");
                }

                break;
            case 'H':
            case 'h':
                ch = 'H';
                if (haveType == 1 || oIdx == 0)
                    putchar('\a');
                else {
                    putchar(ch);
                    col++;
                    haveType = 1;
                }

                break;
            default:
                if (oIdx == maxch || hexRadix == 1 || haveType == 1)
                    putchar('\a');
                else {
                    if (!('0' <= ch && ch <= '9' ||
                        'A' <= ch && ch <= 'F'))
                        if ('a' <= ch && ch <= 'f')
                            ch = toupper(ch);
                        else {
                            putchar('\a');
                            break;
                        }

                    s[oIdx] = ch;
                    putchar(ch);
                    oIdx++;
                    col++;
                }

                break;
            }

            break;
        case 2: //526D
            switch (ch) {
            case '\b': case 0x7f:
                if (col == 0)
                    putchar('\a');
                else {
                    col--;
                    if (col == 0) {
                        inputMode = savedInputMode;
                        maxch = savedMaxch;
                    }

                    if (oIdx)
                        oIdx--;
                    if (hexRadix == 1)
                        hexRadix = 0;
                    printStr(bsb);
                }

                break;
            case '\r':
                s[oIdx] = 0;
                if (oIdx == 0) {
                    if (hexRadix == 0) {
                        iVar[iRdByte()] = -1;
                        done = 1;
                    } else {
                        iVar[iRdByte()] = 0x101;
                        done = 1;
                        printStr("\n");
                    }
                } else {
                    switch (nBytes) {
                    case 1:
                        val = strtoi(s, 10);
                        if (val <= 255) {
                            iVar[iRdByte()] = val;
                            done = 1;
                            printStr("\n");
                        } else {
                            erase(3);
                            col -= 3;
                            oIdx -= 3;
                        }

                        break;
                    case 2: case 3:
                        if (oIdx >= 5) {
                            if (strcmp(s, "65535") <= 0) {
                                iVar[iRdByte()] = strtou(s, 10);
                                done = 1;
                                printStr("\n");
                            } else {
                                erase(5);
                                col -= 5;
                                oIdx -= 5;
                            }

                        }

                        break;
                    }

                }

                break;
            default:
                if (oIdx == maxch || hexRadix == 1)
                    putchar('\a');
                else {
                    if (ch < '0' || '9' < ch)
                        putchar('\a');
                    else {
                        s[oIdx] = ch;
                        putchar(ch);
                        oIdx++;
                        col++;
                    }

                }

                break;
            }

            break;
        case 3:
            switch (ch) {
            case '\b': case 0x7f:
                if (col == 0)
                    putchar('\a');
                else if (oIdx != 0) {
                    if (s[oIdx - 1] < 0x20) {
                        printStr(bsb);
                        col--;
                    }

                    if (s[oIdx - 1] < 0x80) {   // minor bug as >= 0x80 displays . - test should be removed
                        printStr(bsb);
                        col--;
                    }

                    oIdx--;
                } else {
                    printStr(bsb);
                    col--;
                    if (col == 0) {
                        inputMode = savedInputMode;
                        maxch = savedMaxch;
                    }

                    if (hexRadix == 1)
                        hexRadix = 0;
                }

                break;
            case '\r':
                s[oIdx] = 0;
                done = 1;
                if (oIdx == 0) {
                    if (hexRadix == 0)
                        iVar[iRdByte()] = -1;
                    else {
                        iVar[iRdByte()] = 0x101;
                        printStr("\n");
                    }

                } else {
                    if (maxch == 255)
                        printStr("\n");
                    else {
                        val = *s;
                        if (nBytes == 2 && oIdx >= 2)
                            val = (val << 8) + userIn[1];
                        iVar[iRdByte()] = val;
                        printStr("\n");
                    }

                }

                break;
            default:
                if (oIdx == maxch || hexRadix == 1)
                    putchar('\a');
                else {
                    s[oIdx] = ch;
                    oIdx++;
                    putVisible(ch);
                    if (ch < 0x20)
                        col++;
                    if (ch < 0x80)      // minor bug because >= 0x80 displays . -- test should be removed
                        col++;
                }

                break;
            }

            break;
        default:
            badRadix();
            break;
        }
    }
    // of while
}



/// <summary>
/// display character in visible format
/// control chars display as ^x where x is char + '@'
/// >= 0x80 display as .
/// </summary>
/// <param name="ch">char to display</param>
void putVisible(int ch) {   // 5788
    if (ch < 0x20) {
        putchar('^');
        putchar(ch + 0x40);
    } else if (ch < 0x80)
        putchar(ch);
    else
        putchar('.');
}


/// <summary>
/// error for invalid input mode
/// </summary>
void badRadix() {   // 57E5
    printStr("\n");
    printStr("\n");
    printStr("Invalid radix value - corurpted file. \n");
    printStr("\n");
    vexit();
}



/// <summary>
/// look up string to get associated location value
/// allows format of name+offset
/// </summary>
/// <param name="s">named location</param>
/// <returns>the location or 0</returns>
int namedLocOffset(char *s) {    // 5854
    int i, loc;
    int tableLoc;
    int delta, curLoc;
    char *t;
    int nameStart;
#ifdef BDSC
    char var1;              // not used, omitted to avoid compiler warning
#endif
    tableLoc = iRdWord();
    t = s;
    delta = 0;
    for (i = 0; i < 10 && *t; i++, t++) { // 58E3
        if (*t == '+') {
            *t = 0;
            delta = strtoi(++t, 16);
            break;
        }

    }

    t = s;
    while (*t) {
        *t = toupper(*t);
        t++;
    }

    curLoc = itell();
    iSeek(tableLoc);
    while (iRdByte() != ' ') { //5998
        iUngetc();
        nameStart = itell();
        if (iCmp(s) == 0) {
            iSeek(nameStart + 6);       // locate value
            loc = iRdWord() + delta;    // for result
            iSeek(curLoc);              // back to main code
            return loc;                 // give result
        }

        iSeek(nameStart + 6 + 2);       // skip to next name
    }

    iSeek(curLoc);                      // back to main code
    return 0;                           // nothing found
}



void getAscii(char *s, int n) {
    int maxCh, oIdx, col;
    bool done;
    int savedInputMode;
    bool haveRadix, haveType;           // not really used, seems clone of getValue code !!!
    int savedMaxCh, ch, val;
    char *bsb = "\b \b";

    done = oIdx = col = haveRadix = haveType = 0;
    savedInputMode = inputMode = ASCII;
    savedMaxCh = maxCh = (n == 1) ? 1 : (n == 2) ? 2 : 255;
    while (!done) {    // 5c5b
        switch (ch = mpGetch()) {
        case '\b': case 0x7f:
            if (col == 0)
                putchar('\a');
            else {
                if (oIdx) { //5b15
                    if (s[oIdx - 1] < 0x20) {
                        printStr(bsb);
                        col--;
                    }

                    if (s[oIdx - 1] < 0x80) {   // minor bug >= 0x80 displays a . -- test should be removed
                        printStr(bsb);
                        col--;
                    }

                    oIdx--;
                } else {
                    printStr(bsb);
                    col--;
                }

                if (col == 0) {
                    inputMode = savedInputMode;
                    maxCh = savedMaxCh;
                }

            }

            break;
        case '\r':
            if (oIdx == 0)
                putchar('\a');
            else { //5b5a
                s[oIdx] = 0;
                done = true;
                val = *s;
                if (n == 2 && oIdx >= 2)
                    val = (val << 8) + userIn[1];
                iVar[iRdByte()] = val;
                printStr("\n");
            }

            break;
        default:
            if (oIdx == maxCh || haveRadix == 1)
                putchar('\a');
            else {
                s[oIdx] = ch;
                oIdx++;
                putVisible(ch);
                if (ch < ' ')
                    col++;
                if (ch < 0x80)    // minor bug >= 0x80 puts . -- test should be removed
                    col++;
            }
        }


    }

}

/// <summary>
/// get drive into specified string
/// </summary>
/// <param name="s">string for result</param>
void getDrive(char *s) { // 5C7C
    int ch, chCnt;
    char *bsb;

    chCnt = 0;
    bsb = "\b \b";
    userIn[0] = 0;

    while (1) {
        switch (ch = mpGetch()) {
        case ':':
            if (chCnt != 1)
                goto bell;
            putchar(userIn[chCnt++] = ':');
            break;
        case '\b': case 0x7f:
            if (chCnt == 0)
                break;
            printStr(bsb);
            chCnt--;
            break;
        case '\r':
            if (chCnt == 1)
                goto bell;
            else if (chCnt == 0)
                printStr("<RETURN>\n");
            strcpy(s, userIn);
            return;
        default:
            if (chCnt != 0)
                goto bell;
            else {

                ch = toupper(ch);
                if (ch < 'A' || ch >= 'Q')
                    goto bell;
                putchar(userIn[chCnt++] = ch);
                break;
            }
        bell:	putchar('\a');
            break;
        }

        userIn[chCnt] = 0;
    }

}







/// <summary>
/// Parse an unsigned integer
/// </summary>
/// <param name="s">Ascii integer to parse</param>
/// <param name="radix">The radix to use</param>
/// <returns></returns>
unsigned strtou(char *s, int radix) { // 5DE5
    int digit;
    unsigned val;

    val = 0;
    while ((digit = ch2i(*s++, radix)) != -1)
        val = val * radix + digit;
    return val;
}



/// <summary>
/// Erase n chars using "\b \b"
/// </summary>
/// <param name="n">number of chars to erase</param>
void erase(int n) {
    int i;
    char *bsb = "\b \b";

    for (i = 1; i < n; i++)
        printStr(bsb);
}

/// <summary>
/// compare string to inline literal
/// </summary>
/// <param name="s">string to compare</param>
/// <returns>0 if s is equal to null or space terminated inline string, else -1</returns>
int iCmp(char *s) { // 5EA3
    int i;
    char var1;
    for (i = 0; i < 6; i++, s++) {
        if (*s == (var1 = iRdByte()))
            continue;
        return *s == 0 && var1 == ' ' ? 0 : -1;
    }

    return 0;
}

#ifdef BDSC
int openin(char *name, MPFILE *fp) {
    if ((fp->_fd = open(name, 0)) == 0)
        return -1;
    fp->_nleft = = fp->_hiMark = fp->_nsecs = 0;
    return fp->_fd;
}


int Fseek(File *fp, unsigned n, int rel) {
    int var4, var2;

    if (rel)
        n += fp->_hiMark - fp->_nleft;
    if (n >= fp->_hiMark || n < fp->_hiMark - fp->_nsecs & 128) { // 6381
        var4 = n / 128;
        if (close(fp->_fd) == -1)
            return -1;
        if ((var2 = read(fp->_fd, fp->_buf, 4)) <= 0)  // read 4 sectors
            return -1;
        fp->_nsecs = var2;
        fp->_hiMark = (n + var2) * 128;
    }

    fp->_nleft = fp->_hiMark - n;
    fp->_nextp = fp->_buf + (fp->_nsecs * 128 - fp->_nleft);
    return 0;
}


int getc(MPFILE *fp) { // 63F5
    if (*fp->_nleft--)
        return *(fp->_nextp++) & 0xff;
    if ((fp->_nsecs = read(fp->_fd, fp->_buf, 4)) <= 0)
        return -1;
    fp->_hiMark = 128 * fp->_nsecs;
    fp->nleft = fp->_nsecs * 128 - 1;
    return *(fp->_nextp++) & 0xff;
}


unsigned Ftell(MPFILE *fp) { // 64E6
    return fp->_hiMark - fp->_nleft;
}

int ungetc(int ch, MPFILE *fp) { // 650E
    if (fp->_nleft = 512)   // nothing read so far
        return -1;
    *--(fp->_nextp) = ch;
    fp->_nleft++;
    return 0;
}

void putchar(char ch) { // 6564
    if (ch == '\n') {
        bdos(C_RAWIO, '\r');
        bdos(C_RAWIO, ch);

    }
    bdos(C_RAWIO, 'ch');
}

#endif

/// <summary>
/// simple printStr
/// BSDC uses puts but standard puts adds a '\n'
/// </summary>
/// <param name="s"></param>
void printStr(char *s) { // 65B1
    while (*s)
        putchar(*s++);
}


#ifdef BDSC

printf() { // 65E6
}

strcmp() { // 660B
}

strcpy() { // 669A
}

strcat() { // 66E2
}

toupper() { // 674A
}

isdigit() { // 677E
}

isalpha() { // 67B6
}

strlen() { // 67E7
}

_spr() { // 6833
}

islower() { // 6B93
}

isupper() { // 6BC2
}

_uspr() { // 6BF7
}

_gv2() { // 6C9F
}

setmem() { // 6CFF
}

#endif
#ifdef BDSC
// op0b_exec is called with no arg !!
// fortunately not used by ws.com
// note is in assembler in the original code

void op0b_exec() { // 6D22
    execl(???, 0);
// based on iGetItem my guess would be that the code should be
// execl(sVar[iRdByte()], 0);}
#else
void op0b_exec() {
    fprintf(stderr, "fatal error, call to execl with no arg\n");
    fprintf(stderr, "possibly could be execl(%s, 0)", sVar[iRdByte()]);
    exit(1);
}
#endif

#ifdef BDSC
movmem() { // 6D30
}

seek() { // 6D87
}

write() { // 6DEC
}

open() { // 6E5A
}

read() { // 6EC9
}

unlink() { // 6F42
}

rename() { // 6F57
}

gets() { // 6FB6
}

bios() { // 6FE7
}

sbrk() { // 700B
}

cmpdh() { // 7035
}

_bdos() { // 703C
}

execl() { // 704D
}

mpuc() { // 7135
}

code0() { // 713E
}

tell() { // 7168
}

pRdByte(MPFILE *fp) { // 719D
    if (patchfp._nleft == 0) { // 71FD
        if (seek(patchfp._fd, patchfp._hiMark / 128 - patchfp._nsecs) == 0)
            errorLog(7); // patch seek error
        if (write(patchfp._fd, patchfp._buf, patchfp._nsecs) == 0)
            errorLog(8); // patch write error
        getc(fp);
    }
}



#endif
