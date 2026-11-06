#pragma once

#include <windows.h>

#define VER_FILEVERSION              1,0,0,0
#define VER_FILEVERSION_STR         "1.0.0.0"

#define VER_PRODUCTVERSION           1,0,0,0
#define VER_PRODUCTVERSION_STR      "1.0.0.0"

#define VER_COMPANYNAME_STR       "Universal Manager project"
#define VER_FILEDESCRIPTION_STR   "UE4.23-UE5.7"
#define VER_INTERNALNAME_STR      "dwmapi"
#define VER_LEGALCOPYRIGHT_STR    "Copyright (C) 2026 Universal Manager project."
#define VER_ORIGINALFILENAME_STR  "dwmapi.dll"
#define VER_PRODUCTNAME_STR       "dwmapi+Avalonium"
#define VER_LEGALTRADEMARKS_STR   "Unreal Engine is a trademark of Epic Games, Inc."
#define VER_COMMENTS_STR          "Avalonium loader"
#ifndef DEBUG
#define VER_DEBUG                   0
#else
#define VER_DEBUG                   VS_FF_DEBUG
#endif

#ifndef VOS_NT_WINDOWS32
#define VOS_NT_WINDOWS32            0x00040004L
#endif
