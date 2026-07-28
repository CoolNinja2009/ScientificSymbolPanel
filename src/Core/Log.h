#pragma once

// Zero-cost logging: completely compiled out in Release builds.

#ifdef SSP_DEBUG

#include <cstdio>
#include <cstdarg>
#include <string>

namespace ssp::log {

inline void debug(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) {
        fputs(buf, stderr);
        fputc('\n', stderr);
    }
}

} // namespace ssp::log

#define SSP_LOG_DEBUG(fmt, ...) ssp::log::debug(fmt, ##__VA_ARGS__)

#else // Release

#define SSP_LOG_DEBUG(fmt, ...) ((void)0)

#endif
