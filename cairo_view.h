#ifndef __CAIRO_VIEW_H__
#define __CAIRO_VIEW_H__

// fwrite
#include <stdio.h>

// errno type
#include <errno.h>

#include <cairo.h>
#include <cairo-svg.h>
#include <cairo-pdf.h>
#include <cairo-ps.h>

#include <Ecore.h>
#include <Evas.h>
#include <Ecore_Evas.h>

#include "log.h"

typedef void (*Render_Func)(cairo_t *cr, int w_in_pt, int h_in_pt);

static void
_cairo_render_evas(cairo_t *cr, int w, int h)
{
    Ecore_Evas *ee;
    Evas_Object *img;

    RET_IF(!cr);
    RET_IF(!ecore_evas_init());

    cairo_surface_t *surface = cairo_get_target(cr);
    unsigned char *data = cairo_image_surface_get_data(surface);

    ee = ecore_evas_new(NULL, 0, 0, w, h, NULL);
    ecore_evas_show(ee);

    img = evas_object_image_filled_add(ecore_evas_get(ee));
    evas_object_image_colorspace_set(img, EVAS_COLORSPACE_ARGB8888);
    evas_object_image_size_set(img, w, h);
    evas_object_image_data_set(img, data);
    evas_object_resize(img, w, h);
    evas_object_show(img);

    ecore_main_loop_begin();

    evas_object_del(img);
    ecore_evas_free(ee);
    ecore_evas_shutdown();
}

static cairo_status_t
_cairo_stdio_write (void *closure, const unsigned char *data, unsigned int size)
{
    FILE *fp = stdout;
    while (size) {
        size_t ret = fwrite (data, 1, size, fp);
        size -= ret;
        data += ret;
        if (size && ferror (fp)) {
            ERR("Failed to write output: %s", strerror (errno));
            return CAIRO_STATUS_WRITE_ERROR;
        }
    }

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_render_png(cairo_t *cr, int w __UNUSED__, int h __UNUSED__)
{
    RET_IF(!cr);
    cairo_status_t status;

    cairo_surface_t *surface = cairo_get_target(cr);
    if (!surface) ERR("cairo get target failed");

    status = cairo_surface_write_to_png_stream(surface, _cairo_stdio_write, NULL);
    if (status != CAIRO_STATUS_SUCCESS)
        ERR("error:%s", cairo_status_to_string(status));
}

static cairo_surface_t *
_cairo_img_surface_create(Render_Func func, void *key, int w, int h)
{
    RET_IF(!func || !key, NULL);
    cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
    cairo_status_t status = cairo_surface_status(surface);\

    if (status != CAIRO_STATUS_SUCCESS) ERR("error:%s", cairo_status_to_string(status));
    if (cairo_surface_set_user_data(surface, key, func, NULL))
        ERR("set user data failure");

    return surface;
}

static cairo_surface_t *
_cairo_surface_create(int type, int w, int h, void *closure_key)
{
    cairo_surface_t *surface;

    switch(type)
    {
        default:
        case 0:
            surface = _cairo_img_surface_create
                (_cairo_render_evas, closure_key, w, h);
            break;
        case 1:
            surface = _cairo_img_surface_create
                (_cairo_render_png, closure_key, w, h);
            break;
        case 2:
            surface = cairo_svg_surface_create_for_stream
                (_cairo_stdio_write, NULL, w, h);
            break;
        case 3:
            surface = cairo_pdf_surface_create_for_stream
                (_cairo_stdio_write, NULL, w, h);
            break;
        case 4:
            surface = cairo_ps_surface_create_for_stream
                (_cairo_stdio_write, NULL, w, h);
            break;
        case 5:
            surface = cairo_ps_surface_create_for_stream
                (_cairo_stdio_write, NULL, w, h);
            cairo_ps_surface_set_eps(surface, 1);
            break;
    }
    return surface;
}

cairo_t *
_cairo_create(cairo_surface_t *surface)
{
    unsigned int fr, fg, fb, fa, br, bg, bb, ba;
    br = bg = bb = ba = 255;
    fr = fg = fb = 0; fa = 255;

    cairo_t *cr = cairo_create(surface);
    cairo_content_t content = cairo_surface_get_content(surface);

    switch (content) {
        case CAIRO_CONTENT_ALPHA:
            cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
            cairo_set_source_rgba(cr, 1., 1., 1., br / 255.);
            cairo_paint(cr);
            cairo_set_source_rgba(cr, 1., 1., 1.,
                    (fr / 255.) * (fa / 255.) + (br / 255) * (1 - (fa / 255.)));
            break;
        default:
        case CAIRO_CONTENT_COLOR:
        case CAIRO_CONTENT_COLOR_ALPHA:
            cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
            cairo_set_source_rgba(cr, br / 255., bg / 255., bb / 255., ba / 255.);
            cairo_paint(cr);
            cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
            cairo_set_source_rgba(cr, fr / 255., fg / 255., fb / 255., fa / 255.);
            break;
    }

    return cr;
}
#endif
