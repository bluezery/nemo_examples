#ifndef __CAIRO_WAYLAND_H__
#define __CAIRO_WAYLAND_H__

#define _GNU_SOURCE
#include <wayland-client.h>
#include <cairo.h>
//#include <xkbcommon/xkbcommon.h>

#include <stdlib.h>  // free, calloc, mkostemp, malloc, getenv
#include <errno.h>
#include <string.h>   // strerror
#include <unistd.h> // close, unlink
#include <sys/mman.h>  // mmap
#include <fcntl.h>      // posix_fallocate
#include <sys/epoll.h>  // epoll

#include "log.h"
#include "util.h"
#include "nemolist.h"

// Window
typedef struct _Wl_Window Wl_Window;
void wl_window_init();
void wl_window_shutdown();
Wl_Window *wl_window_create(unsigned int w, unsigned int h, unsigned int stride);
void wl_window_destroy(Wl_Window *win);
void wl_window_loop(Wl_Window *win);
void wl_window_set_buffer(Wl_Window *win, unsigned char *data, unsigned int size);

// Fd Handler
typedef void (*FdHandlerCallback)(void *data);
typedef struct _FdHandler FdHandler;
bool fd_handler_init();
void fd_handler_destroy(FdHandler *fh);
void fd_handler_shutdown();
FdHandler * fd_hanlder_add(int fd, FdHandlerCallback callback, uint32_t events, void *data);


#endif
