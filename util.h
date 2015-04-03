#ifndef __UTIL_H__
#define __UTIL_H__

#include <cairo.h>
#include <stdbool.h>   // bool
#include "log.h"

// String
char* _strdup_printf(const char *fmt, ...);

// File
bool _file_is_dir(const char *file);
bool _file_exist(const char *file);
bool _file_mkdir(const char *dir, int mode);
bool _file_mkdir_recursive(const char *file, int mode);
char **_file_load(const char *filename, int *line_len);

// Timer (implemented by signal)
typedef bool (*SigTimerCb)(void *data);

typedef struct __SigTimer SigTimer;
bool sigtimer_init();
void sigtimer_shutdown();
SigTimer *sigtimer_create(unsigned int mseconds, SigTimerCb callback, void *data);
void sigtimer_destroy(SigTimer *timer);

// Image
typedef struct _Image Image;
cairo_surface_t *image_get_surface(Image *img);
Image *image_create(const char *path);
void image_destroy(Image *img);

/****************************************************/
/* List */
/***************************************************/
typedef struct _List List;
struct _ListInfo
{
    int cnt;
    List *first;
};

struct _List
{
    List *prev;
    List *next;
    void *data;
    struct _ListInfo *info;
};

#define LIST_FOR_EACH_REVERSE(list, l, d) for (l = list, d = (l ? l->data : NULL) ; l ; l = l->prev, d = (l ? l->data : NULL ))
#define LIST_FOR_EACH(list, l, d) for (l = (list ? list->info->first : NULL), d = (l ? l->data : NULL) ; l ; l = l->next, d = (l ? l->data : NULL ))
#define LIST_FOR_EACH_REVERSE_SAFE(list, l, tmp, d) for (l = list, d = (l ? l->data : NULL), tmp = l->prev ; l ; l = tmp, d = (l ? l->data : NULL ), tmp = (l ? l->prev : NULL))
#define LIST_FOR_EACH_SAFE(list, l, tmp, d) for (l = (list ? list->info->first : NULL), d = (l ? l->data : NULL), tmp = (l ? l->next : NULL) ; l ; l = tmp, d = (l ? l->data : NULL ), tmp = (l ? l->next : NULL))

static inline List *
list_data_insert(List *l, void *data)
{
    if (!l) {
        l = calloc(sizeof(List), 1);
        l->data = data;
        l->info = calloc(sizeof(struct _ListInfo), 1);
        l->info->cnt = 1;
        l->info->first = l;
        return l;
    } else {
        l->next = calloc(sizeof(List), 1);
        l->next->prev = l;
        l->next->data = data;
        l->info->cnt++;
        l->next->info = l->info;
        return l->next;
    }
}

static inline List *
list_data_remove(List *l, void *data)
{
    List *tmp = l;
    List *ret = l;
    while (tmp) {
        if (tmp->data == data) {
            if (tmp->prev)
                tmp->prev->next = tmp->next;
            if (tmp->next)
                tmp->next->prev = tmp->prev;
            tmp->info->cnt--;
            if (tmp == l) ret = tmp->prev;
            if (tmp == tmp->info->first)
                tmp->info->first = tmp->next;

            if (!tmp->prev && !tmp->next) {
                free(tmp->info);
            }
            tmp->prev = NULL;
            tmp->next = NULL;
            tmp->info = NULL;
            tmp->data = NULL;
            free(tmp);
            break;
        }
        tmp = tmp->prev;
    }
    return ret;
}

static inline List *
list_data_find(List *l, void *data)
{
    while (l) {
        if (l->data == data) break;
        l = l->prev;
    }
    return l;
}

static inline List *
list_clear(List *l)
{
    List *tmp;
    if (l) free(l->info);
    while (l) {
        tmp = l->prev;
        free(l);
        l = tmp;
    }
    return l;
}

static inline unsigned int
list_count(List *l)
{
    if (!l) return 0;
    return l->info->cnt;
}

// if -1. it fails
static inline int
list_data_get_idx(List *l, void *data)
{
    if (!l) return -1;

    l = l->info->first;
    int idx = 0;
    while (l) {
        if (l->data == data) break;
        l = l->next;
        idx++;
    }
    return idx;
}

static inline void *
list_idx_get_data(List *l, unsigned int idx)
{
    if (!l) return NULL;

    l = l->info->first;
    while (l && idx) {
        l = l->next;
        idx--;
    }
    if (!l) return NULL;
    return l->data;
}

#endif
