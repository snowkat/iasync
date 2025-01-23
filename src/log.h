// log.h: Logging routines

#ifndef __IASYNC_LOG_H
#define __IASYNC_LOG_H

/// @brief Dies loudly (writes the error to stdout and exits with return value
/// 2).
#define die(...)                                                               \
    {                                                                          \
        log_printf(IA_CRITICAL, __VA_ARGS__);                                  \
        exit(2);                                                               \
    }

enum log_level
{
    IA_CRITICAL = 0,
    IA_ERROR = 1,
    IA_WARNING = 2,
    IA_INFO = 3,
    IA_VERBOSE = 4,
    IA_DEBUG = 5,
    IA_TRACE = 6,
};

/// @brief Writes a `printf`-like formatted string to stdout, based on the given
/// verbosity.
/// @param level The desired log level for the text.
void
log_printf(enum log_level level, const char* fmt, ...);

/// @brief Sets the highest permitted log level.
void
set_log_level(enum log_level level);

#endif /* !__IASYNC_LOG_H */