#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <nemotool.h>
#include <nemoegl.h>

#include <nemotale.h>
#include <nemocanvas.h>
#include <talegl.h>
#include <talemisc.h>
#include <taleevent.h>
#include <talegesture.h>
#include <talenode.h>

#include <pathshape.h>
#include <pathstyle.h>

#include "util.h"

#define EGL 0

struct Context {
#if EGL
    struct eglcanvas *canvas;
#else
    struct nemocavas *canvas;
#endif
    struct talenode *node;
    struct pathone *group;
    double width, height;
    cairo_matrix_t matrix;
};

static void
_canvas_resize(struct nemocanvas *canvas, int32_t width, int32_t height)
{
    struct nemotale *tale = nemocanvas_get_userdata(canvas);
    struct Context *ctx = nemotale_get_userdata(tale);
    struct talenode *node = ctx->node;
    struct pathone *group = ctx->group;
    cairo_matrix_t matrix = ctx->matrix;

#if EGL
    nemotool_resize_egl_canvas(ctx->canvas, width, height);
	nemotale_resize_egl(tale, width, height);
	nemotale_node_resize_pixman(node, width, height);
#else
    nemocanvas_set_size(ctx->canvas, width, height);
    nemocanvas_flip(canvas);
    nemocanvas_clear(canvas);
    nemotale_detach_pixman(tale);
    nemotale_attach_pixman(tale,
            nemocanvas_get_data(canvas),
            nemocanvas_get_width(canvas),
            nemocanvas_get_height(canvas),
            nemocanvas_get_stride(canvas));
    nemotale_node_resize_pixman(node, width, height);
#endif


	cairo_matrix_init_scale(&matrix,
            (double)width / ctx->width,
            (double)height / ctx->height);

    nemotale_node_clear_path(node);
    nemotale_path_set_parent_transform(group, &matrix);
    nemotale_path_update_one(group);
    nemotale_node_render_path(node, group);
    nemotale_node_damage_all(node);
    nemotale_composite(tale, NULL);

#if !EGL
    nemocanvas_damage(canvas, 0, 0, 0, 0);
    nemocanvas_commit(canvas);
#endif
}

static void
_tale_event(struct nemotale *tale, struct talenode *node, uint32_t type, struct taleevent *event)
{
    struct taletap *taps[16];
    int ntaps;
    uint32_t id;

    struct Context *ctx = nemotale_get_userdata(tale);
    struct nemocanvas *canvas;
#if EGL
    struct eglcanvas *eglcanvas = ctx->canvas;
    canvas = NTEGL_CANVAS(eglcanvas);
#else
    canvas = ctx->canvas;
#endif

    id = nemotale_node_get_id(node);
    ntaps = nemotale_get_node_taps(tale, node, taps, type);
    if (id == 1) {
        if (type & NEMOTALE_DOWN_EVENT) {
            if (ntaps == 1) {
                nemocanvas_move(canvas, taps[0]->serial);
            } else if (ntaps == 2) {
                nemocanvas_pick(canvas,
                        taps[0]->serial,
                        taps[1]->serial,
                        (1 << NEMO_SURFACE_PICK_TYPE_SCALE));
            } else if (!nemotale_is_single_click(tale, event, type)) {
                if (ntaps == 3) {
                    struct nemotool *tool = nemocanvas_get_tool(canvas);
                    nemotool_exit(tool);
                }
            }

        }
    }
}

int main()
{
    double  width = 320, height = 320;

    struct Context *ctx = malloc(sizeof(struct Context));
    ctx->width = width;
    ctx->height = height;

    struct nemotool *tool;
    tool = nemotool_create();
    nemotool_connect_wayland(tool, NULL);

#if EGL
    struct eglcanvas *canvas;
    struct eglcontext *egl;
    egl = nemotool_create_egl(tool);
    canvas = nemotool_create_egl_canvas(egl, width, height);
    nemocanvas_set_nemosurface(NTEGL_CANVAS(canvas), NEMO_SHELL_SURFACE_TYPE_NORMAL);
    nemocanvas_set_anchor(NTEGL_CANVAS(canvas), -0.5f, -0.5f);
    nemocanvas_set_layer(NTEGL_CANVAS(canvas), NEMO_SURFACE_LAYER_TYPE_OVERLAY);
    nemocanvas_set_dispatch_resize(NTEGL_CANVAS(canvas), _canvas_resize);
    ctx->canvas = canvas;
#else
    struct nemocanvas *canvas;
    canvas = nemocanvas_create(tool);
    nemocanvas_set_size(canvas, width, height);
    nemocanvas_set_nemosurface(canvas, NEMO_SHELL_SURFACE_TYPE_NORMAL);
    nemocanvas_set_anchor(canvas, -0.5, -0.5f);
    nemocanvas_set_dispatch_resize(canvas, _canvas_resize);
    //nemocanvas_set_layer(canvas, NEMO_SURFACE_LAYER_TYPE_OVERLAY);
    //nemocanvas_set_pivot(canvas, width/2., height/2.);
    nemocanvas_flip(canvas);
    nemocanvas_clear(canvas);
    ctx->canvas = canvas;
#endif

#if EGL
    struct nemotale *tale;
    tale = nemotale_create_egl(
            NTEGL_DISPLAY(egl),
            NTEGL_CONTEXT(egl),
            NTEGL_CONFIG(egl));
    nemotale_set_composite_type(tale, NEMOTALE_COMPOSITE_TYPE_CLEAR);
    nemotale_attach_egl(tale, (EGLNativeWindowType)NTEGL_WINDOW(canvas));
    nemotale_resize_egl(tale, width, height);

    nemotale_attach_canvas(tale, NTEGL_CANVAS(canvas), _tale_event);
    nemotale_set_userdata(tale, ctx);
#else
    struct nemotale *tale;
    tale = nemotale_create_pixman();
    nemotale_attach_pixman(tale,
            nemocanvas_get_data(canvas),
            nemocanvas_get_width(canvas),
            nemocanvas_get_height(canvas),
            nemocanvas_get_stride(canvas));

    nemotale_attach_canvas(tale, canvas, _tale_event);
    nemotale_set_userdata(tale, ctx);
#endif

    struct talenode *node;
    node = nemotale_node_create_pixman(width, height);
    nemotale_node_set_id(node, 1);
    nemotale_attach_node(tale, node);
    ctx->node = node;

    struct pathone *group, *one;
    group = nemotale_path_create_group();

    one = nemotale_path_create_circle(width/2.);
    //one = nemotale_path_create_rect(width, height);
	nemotale_path_attach_style(one, NULL);
    nemotale_path_attach_one(group, one);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0.0f, 0.5f, 0.5f, 0.5f);
    ctx->group = group;

    nemotale_path_update_one(group);
    nemotale_node_render_path(node, group);
    nemotale_composite(tale, NULL);

#if !EGL
	nemocanvas_damage(canvas, 0, 0, 0, 0);
	nemocanvas_commit(canvas);
#endif

    cairo_matrix_t matrix;
	cairo_matrix_init_identity(&matrix);
    ctx->matrix = matrix;

    nemotool_run(tool);

    nemotale_path_destroy_one(one);
    nemotale_path_destroy_one(group);
    nemotale_node_destroy(node);

    nemotale_destroy(tale);
    nemocanvas_destroy(canvas);

    nemotool_disconnect_wayland(tool);
    nemotool_destroy(tool);

    return 0;
}
