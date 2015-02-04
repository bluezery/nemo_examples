#ifndef __UTIL_H__
#define __UTIL_H__

#include <unistd.h>    // stat
#include <sys/stat.h>  // mkdir, stat
#include <sys/types.h> // mkdir, stat
#include <libgen.h>    // dirname
#include <stdbool.h>   // bool
#include "log.h"

static char*
_strdup_printf(const char *fmt, ...)
{
    va_list ap;
    char *str = NULL;
    unsigned int size = 0;

    while (1) {
        int n;
        va_start(ap, fmt);
        n = vsnprintf(str, size, fmt, ap);
        va_end(ap);

        if (n < 0) {
            ERR("vsnprintf failed");
            if (str) free(str);
            break;
        }
        if (n < size) break;

        size = n + 1;
        str = realloc(str, size);
    }
    return str;
}

static bool
_file_is_dir(const char *file)
{
    struct stat st;
    int r = stat(file, &st);
    if (r) {
        LOG("%s:%s", strerror(errno), file);
        return false;
    }
    return S_ISDIR(st.st_mode);
}

static bool
_file_exist(const char *file)
{
    struct stat st;
    int r = stat(file, &st);
    if (r) return false;
    return true;
}

static bool
_file_mkdir(const char *dir, int mode)
{
    RET_IF(!dir, false);
    bool ret;
    char *parent;

    if (_file_is_dir(dir)) {
        LOG("already exists: %s", dir);
        return true;
    }

    char *path = strdup(dir);
    parent = dirname(path);
    if (parent && strcmp(parent, ".") && strcmp(parent, "/")) {
        ret = _file_mkdir(parent, mode | 0300);
        if (!ret) {
            free(path);
            return false;
        }
    }
    free(path);

    ret = mkdir(dir, 0777);
    if (ret) {
        ERR("mkdir failed (%s): %s", dir, strerror(errno));
        return false;
    }

    if (mode != -1) {
        ret = chmod(dir, mode);
        if (ret) {
            ERR("chmod failed: %s: %s(%d)", strerror(errno), dir, mode);
            return false;
        }
    }
    return true;
}

static bool
_file_mkdir_recursive(const char *file, int mode)
{
    RET_IF(!file, false);
    struct stat st;
    int r;

    r = stat(file, &st);
    if (r < 0 && errno == ENOENT) {
        bool ret;
        char *parent;

        char *path = strdup(file);
        parent = dirname(path);
        if (!parent) {
            free(path);
            return false;
        }

        if (strcmp(parent, ".") && strcmp(parent, "/")) {
            free(path);
            return true;
        }
        ret = _file_mkdir_recursive(parent, mode | 0300);

        if (!ret) {
            free(path);
            return false;
        }
        r = mkdir(path, 0777);
        if (r) {
            ERR("mkdir failed (%s): %s", path, strerror(errno));
            free(path);
            return false;
        }

        if (mode != -1) {
            r = chmod(path, mode);
            if (r) {
                ERR("chmod failed (%s) (%d): %s", path, mode, strerror(errno));
                free(path);
                return false;
            }
        }
    }
    return true;
}
#endif
