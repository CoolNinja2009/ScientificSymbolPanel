#pragma once

// Zero-cost logging: completely compiled out in Release builds.

#ifdef SSP_DEBUG

#include <windows.h>
#include <cstdio>
#include <string>

namespace ssp::log {

inline void debug(const wchar_t* fmt, ...) {
    wchar_t buf[1024];
    va_list args;
    va_start(args, fmt);
    int len = _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, args);
    va_end(args);
    if (len > 0) {
        OutputDebugStringW(buf);
        OutputDebugStringW(L"\n");
    }
}

inline void debug(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int len = _vsnprintf_s_l(buf, _countof(buf), _TRUNCATE, fmt, nullptr, args);
    va_end(args);
    if (len > 0) {
        OutputDebugStringA(buf);
        OutputDebugStringA("\n");
    }
}

} // namespace ssp::log

#define SSP_LOG_DEBUG(fmt, ...) ssp::log::debug(fmt, ##__VA_ARGS__)

#else // Release: strip all logging

#define SSP_LOG_DEBUG(fmt, ...) ((void)0)

#endif
