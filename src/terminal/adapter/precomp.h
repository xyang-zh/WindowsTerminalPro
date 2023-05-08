/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- precomp.h

Abstract:
- Contains external headers to include in the precompile phase of console build process.
- Avoid including internal project headers. Instead include them only in the classes that need them (helps with test project building).
--*/

// This includes support libraries from the CRT, STL, WIL, and GSL
#include "LibraryIncludes.h"

#include <cmath>
#define ENABLE_INTSAFE_SIGNED_FUNCTIONS
#include <intsafe.h>

#include <sal.h>

#include "telemetry.hpp"
#include "tracing.hpp"

#include "../../inc/conattrs.hpp"
