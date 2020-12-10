#pragma once

#include <stdbool.h>

#include "config.h"

const char *cCriticalError;
const char *cError;
const char *cWarning;
const char *cInfo;
const char *cDebug;

void log_printf(const char *color, const char *format, ...);

static inline bool logging_debug_enabled()
{
    const config_t *config = config_get();
    return config && config->logging;
}

#define critical_error(message) log_printf(cCriticalError, "%s", message)
#define error(message) log_printf(cError, "%s", message)
#define warn(message) log_printf(cWarning, "%s", message)
#define info(message) log_printf(cInfo, "%s", message)
#define debug(message) log_printf(cDebug, "%s", message)

#define critical_errorf(...) log_printf(cCriticalError, __VA_ARGS__)
#define errorf(...) log_printf(cError, __VA_ARGS__)
#define warnf(...) log_printf(cWarning, __VA_ARGS__)
#define infof(...) log_printf(cInfo, __VA_ARGS__)
#define debugf(...)              \
    if (logging_debug_enabled()) \
    log_printf(cDebug, __VA_ARGS__)



#define critical_errorp() perror(cCriticalError)
#define errorp() perror(cError)
#define warnp() perror(cWarning)
#define infop() perror(cInfo)
#define debugp() perror(cDebug)