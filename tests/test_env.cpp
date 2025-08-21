// Cross-platform shim for tests that call Windows-specific _putenv("NAME=VALUE").
// On POSIX (macOS/Linux) define a local _putenv that maps to setenv/unsetenv.

#include <cstdlib>
#include <cstring>
#include <string>

#if !defined(_WIN32)
extern "C" int _putenv(const char* assignment)
{
    if (!assignment) return -1;
    const char* eq = std::strchr(assignment, '=');
    if (!eq) {
        // Not in NAME=VALUE form
        return -1;
    }
    std::string name(assignment, static_cast<size_t>(eq - assignment));
    const char* value = eq + 1;
    if (!*value) {
        // Unset when empty after '='
        return ::unsetenv(name.c_str());
    }
    return ::setenv(name.c_str(), value, 1);
}
#endif
