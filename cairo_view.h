#ifndef __CAIRO_VIEW_H__
#define __CAIRO_VIEW_H__

#define _GNU_SOURCE
#include <cairo.h>

typedef struct __View View;

View *view_create(int type, int w, int h, unsigned int br, unsigned int bg, unsigned int bb, unsigned int ba);
cairo_t *view_get_cairo(View *view);
void view_do(View *view);
void view_destroy(View *view);

#endif
