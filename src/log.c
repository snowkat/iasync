// log.c

#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static enum log_level max_level = IA_INFO;

void
log_printf(enum log_level level, const char* fmt, ...)
{
    if (level <= max_level) {
        va_list ap;

        fprintf(stderr, "%s: ", getprogname());

        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
}

void
set_log_level(enum log_level level)
{
    max_level = level;
}