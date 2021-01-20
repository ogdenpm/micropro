# WordStar
These are my attempts at reverse engineering the micropro WS family

So far only WINSTALL and the WS.INS  have been reverse engineered.

The subdirectories contain the following:

### wsinstall

Contains my port of the WINSTALL.COM file (version 3.3). This is effectively an interpreter that processes the *.INS files.

The code is based closely on my decompilation of the file into BDSC. I have however modified some of the code to run on modern systems and avoid some of the BDSC specials. My original work is however left in the file surrounded by #ifdef BDSC / #endif so if you wish to attempt a true BDSC reconstruction there  there is enough information to help you.

Note I believe the code was compiled using BDSC v1.46.

### ws.ins

This contains my decompilation of the WS.INS script file, which I did through a bespoke perl script, currently not shared.

Looking through this you should be able to see the patch code for all directly supported terminals and printers.

### original

Reference WINSTALL.COM, WS.COM and WSU.COM files

### Scripts

A couple of bespoke command scripts I use as part of my standard build setup. For details see  [my versionTools repository on (github.com)](https://github.com/ogdenpm/versionTools)



------

```
Updated 20-Jan-2021 - Mark Ogden
```

