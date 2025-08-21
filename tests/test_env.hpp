#pragma once

// Test-only cross-platform environment setter shim
// Many tests use Windows-specific _putenv("NAME=VALUE").
// On non-Windows, we provide a local definition of _putenv in test_env.cpp.
// Declare it here so every TU has a prototype when compiling on non-Windows.

#ifndef _WIN32
extern "C" int _putenv(const char* assignment);
#endif
