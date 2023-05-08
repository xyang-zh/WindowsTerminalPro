/*++
Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:
- precomp.h

Abstract:
- Contains external headers to include in the precompile phase of console build process.
- Avoid including internal project headers. Instead include them only in the classes that need them (helps with test project building).
--*/

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#define NOMINMAX

#include <windows.h>

#include <cstdlib>
#include <cstdio>

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"
