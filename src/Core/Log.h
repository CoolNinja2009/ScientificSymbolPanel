#pragma once

// Zero-cost logging: completely compiled out in Release builds.

#ifdef SSP_DEBUG

#include <cstdio>
#include <cstdarg>
#include <string>

namespace ssp::log {

inline void debug(const wchar_t* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::fwprintf(stderr, L"[SSP] ");
    std::vfwprintf(stderr, fmt, args);
    std::fwprintf(stderr, L"\n");
    va_end(args);
}

inline void debug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::fprintf(stderr, "[SSP] ");
    std::vfprintf(stderr, fmt, args);
    std::fprintf(stderr, "\n");
    va_end(args);
}

} // namespace ssp::log

#define SSP_LOG_DEBUG(fmt, ...) ssp::log::debug(fmt, ##__VA_ARGS__)

#else // Release: strip all logging

#define SSP_LOG_DEBUG(fmt, ...) ((void)0)

#endif
