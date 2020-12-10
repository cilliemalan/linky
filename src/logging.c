
#include <stdarg.h>
#include <stdio.h>

#include "logging.h"

const char *cCriticalError = "\033[1;5;91m[ CRITICAL ]\033[0m ";
const char *cError = "\033[0;31m[ ERROR ]\033[0m ";
const char *cWarning = "\033[0;33m[ WARN ]\033[0m ";
const char *cInfo = "\033[0;36m[ INFO ]\033[0m ";
const char *cDebug = "[ DEBUG ] ";

void log_printf(const char *color, const char *format, ...)
{
    printf("%s", color);
    va_list va;
    va_start(va, format);
    vprintf(format, va);
    va_end(va);
    printf("\n");
}
