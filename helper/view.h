#ifndef __VIEW_H__
#define __VIEW_H__

#define _GNU_SOURCE
#include <stdint.h>
#include <stdbool.h>
#include <cairo.h>
#include <sys/epoll.h>

// View
typedef struct __View View;
View *view_create(int w, int h, unsigned int br, unsigned int bg, unsigned int bb, unsigned int ba);
cairo_t *view_get_cairo(View *view);
cairo_surface_t *view_get_surface(View *view);
void view_do(View *view);
void view_destroy(View *view);
void view_update(View *view);
bool view_init();
void view_shutdown();

typedef bool (*FdCallback)(uint32_t event, void *data);
typedef bool (*Callback)(void *data);
// Fd Handler
typedef struct _FdHandler FdHandler;
bool fd_handler_init();
void fd_handler_destroy(FdHandler *fdh);
void fd_handler_shutdown();
FdHandler *fd_handler_attach(View *view, unsigned int fd, uint32_t events, FdCallback callback, void *data);
bool fd_handler_call(int epfd, int fd, uint32_t events); // for window

typedef struct _Timer Timer;
bool timer_init();
void timer_shutdown();
void timer_destroy(Timer *timer);
Timer *timer_attach(View *view, unsigned int mseconds, Callback callback, void *data);
#endif
