#pragma once
#include <windows.h>
#define VER_FILEVERSION             1,0,0,0
#define VER_FILEVERSION_STR         "1.0.0.0"
#define VER_PRODUCTVERSION          1,0,0,0
#define VER_PRODUCTVERSION_STR      "1.0.0.0"
#define VER_COMPANYNAME_STR       "Universal Manager Project"
#define VER_FILEDESCRIPTION_STR   "Unreal Engine Version Detector"
#define VER_INTERNALNAME_STR      "UE_Detector"
#define VER_LEGALCOPYRIGHT_STR    "Copyright (C) 2026 Universal Manager Project."
#define VER_ORIGINALFILENAME_STR  "UE_Detector.exe"
#define VER_PRODUCTNAME_STR       "Version Detector"
#ifndef DEBUG
#define VER_DEBUG                   0
#else
#define VER_DEBUG                   VS_FF_DEBUG
#endif
#ifndef VOS_NT_WINDOWS32
#define VOS_NT_WINDOWS32            0x00040004L
#endif
