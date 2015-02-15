#ifndef __CAIRO_VIEW_H__
#define __CAIRO_VIEW_H__

#define _GNU_SOURCE
#include <cairo.h>

cairo_surface_t *_cairo_surface_create(int type, int w, int h, void *closure_key);
cairo_t *_cairo_create(cairo_surface_t *surface, unsigned int br, unsigned int bg, unsigned int bb, unsigned int ba);

#endif
