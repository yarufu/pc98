#include "debug.h"

#include <stdarg.h>
#include <stdio.h>

void debug_log_init(void)
{
    remove("debug.txt");
}

void debug_log(const char *fmt, ...)
{
    FILE *fp;
    va_list args;

    fp = fopen("debug.txt", "a");

    if (fp == 0) {
        return;
    }

    va_start(args, fmt);

    vfprintf(fp, fmt, args);

    fprintf(fp, "\n");

    va_end(args);

    fclose(fp);
}
