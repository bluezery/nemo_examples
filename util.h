#ifndef __UTIL_H__
#define __UTIL_H__

#include <cairo.h>
#include <stdbool.h>   // bool

// String
char* _strdup_printf(const char *fmt, ...);

// File
bool _file_is_dir(const char *file);
bool _file_exist(const char *file);
bool _file_mkdir(const char *dir, int mode);
bool _file_mkdir_recursive(const char *file, int mode);

// Timer (implemented by signal)
typedef bool (*SigTimerCb)(void *data);

typedef struct __SigTimer SigTimer;
bool sigtimer_init();
void sigtimer_shutdown();
SigTimer *sig_timer_create(unsigned int mseconds, SigTimerCb callback, void *data);
void sigtimer_destroy(SigTimer *timer);

// Image
typedef struct _Image Image;
cairo_surface_t *image_get_surface(Image *img);
Image *image_create(const char *path);
void image_destroy(Image *img);

#endif
