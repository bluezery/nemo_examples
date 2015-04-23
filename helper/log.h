#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>

#ifdef __GNUC__
    #define __UNUSED__ __attribute__((__unused__))
#else
    #define __UNUSED__
#endif

#define LOG_COLOR_BLUE "\x1b[34m"
#define LOG_COLOR_RED "\x1b[31m"
#define LOG_COLOR_GREEN "\x1b[32m"
#define LOG_COLOR_END "\x1b[0m"

#define LOGFMT(COLOR, fmt, ...) do { fprintf(stderr, "[%s:%d][%s] " fmt "\n", __FILE__,  __LINE__, __func__, ##__VA_ARGS__); } while (0);
#define ERR(...) LOGFMT(LOG_COLOR_RED, ##__VA_ARGS__)
#define LOG(...) LOGFMT(LOG_COLOR_GREEN, ##__VA_ARGS__)

#define RET_IF(VAL, ...) do { if (VAL) { ERR(#VAL " is false\n"); return __VA_ARGS__; } } while (0);

#endif
