#ifndef __WL_WINDOW_H__
#define __WL_WINDOW_H__

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

// Window
typedef struct _Wl_Window Wl_Window;
bool wl_window_init();
void wl_window_shutdown();
Wl_Window *wl_window_create(unsigned int w, unsigned int h, unsigned int stride);
void wl_window_destroy(Wl_Window *win);
void wl_window_loop(Wl_Window *win);
void wl_window_set_buffer(Wl_Window *win, unsigned char *data, unsigned int size, unsigned int w, unsigned int h);
int wl_window_get_epoll_fd(Wl_Window *win);

#endif
