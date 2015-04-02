#include <nemotool.h>
#include <nemocanvas.h>
#include <nemotale.h>
#include <talenode.h>
#include <talepixman.h>

#include <math.h>
#include <pathpaint.h>
#include "talehelper.h"

#include "util.h"
#include "nemohelper.h"

struct Context {
    struct nemocanvas *canvas;
    struct talenode *node;

    struct pathone *group, *c_group;
    struct pathone *one_bg;
    struct pathone *top, *bottom;
    struct pathone *c1, *b1;
    struct pathone *c2, *b2;
    int w, h;
    int prev_event_y;
    int prev_event_x;
    int zoom_state;
    double prev_x, prev_y;
};

static void
_canvas_resize(struct nemocanvas *canvas, int32_t w, int32_t h)
{
    struct nemotale *tale = nemocanvas_get_userdata(canvas);
    struct Context *ctx = nemotale_get_userdata(tale);
    struct talenode *node = ctx->node;

    nemocanvas_set_size(canvas, w, h);
    nemotale_handle_canvas_update_event(NULL, canvas, tale);

    nemotale_node_resize_pixman(node, w, h);

    double sx = (double)w/ctx->w;
    double sy = (double)h/ctx->h;
    cairo_matrix_t matrix;
    cairo_matrix_init_scale(&matrix, sx, sy);

    nemotale_node_clear_path(node);
    nemotale_node_damage_all(node);

    nemotale_path_update_one(ctx->group);
    nemotale_node_render_path(node, ctx->group);


    nemotale_composite(tale, NULL);
    nemotale_handle_canvas_flush_event(NULL, canvas, NULL);
}

static void
_move(struct Context *ctx)
{
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    struct nemotale *tale =nemocanvas_get_userdata(canvas);
    struct taletransition *trans1, *trans2;

    trans1 = _transit_create(canvas, 0, 1000, NEMOEASE_CUBIC_IN_TYPE);
    _transit_transform_path(trans1, ctx->c_group);
    _transit_damage_path(trans1, ctx->node, ctx->group);
    _transit_go(trans1, canvas);

    struct pathone *one;
    one = nemotale_path_create_rect(300, 350);
    nemotale_path_set_id(one, "c2");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0.0, 0.0, 1.0, 0.0);
    nemotale_path_translate(one, 0, 450);
    nemotale_path_attach_one(ctx->c_group, one);
    ctx->c2 = one;

    one = nemotale_path_create_rect(300, 50);
    nemotale_path_set_id(one, "b2");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1.0, 1.0, 0.0, 0.5);
    nemotale_path_translate(one, 0, 850);
    nemotale_path_attach_one(ctx->c_group, one);
    ctx->b2 = one;

    nemotale_path_transform_enable(ctx->c_group);
    nemotale_path_transform_enable(ctx->b2);
    nemotale_transition_attach_signal(trans1,
            NTPATH_DESTROY_SIGNAL(ctx->b2));
    nemotale_transition_attach_signal(trans1,
            NTPATH_DESTROY_SIGNAL(ctx->c_group));
    /*
    nemotale_transition_attach_dattr(trans1,
            NTPATH_TRANSFORM_ATY(ctx->b2), 1.0f, -300);
            */
    nemotale_transition_attach_dattr(trans1,
            NTPATH_TRANSFORM_ATY(ctx->c_group), 1.0f, -500);
    nemotale_transition_attach_signal(trans1,
            NTPATH_DESTROY_SIGNAL(ctx->b1));
    nemotale_transition_attach_dattrs(trans1,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->b1)),
            4.0f, 1.0f, 1.0, 1.0, 0.0, 0.0);
    nemotale_transition_attach_dattrs(trans1,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->c2)),
            4.0f, 1.0f, 0.0, 0.0, 1.0, 0.5);

}

static void
_tale_event(struct nemotale *tale, struct talenode *node, uint32_t type, struct taleevent *event)
{
    struct Context *ctx = nemotale_get_userdata(tale);
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotool *tool = nemocanvas_get_tool(canvas);

    struct taletap *taps[16];
    int ntaps;
    uint32_t id;
    struct taletap *tap;

    ntaps = nemotale_get_node_taps(tale, node, taps, type);
    tap = nemotale_get_tap(tale, event->device, type);
    id = nemotale_node_get_id(node);

    if ((type & NEMOTALE_DOWN_EVENT) ||
        (type & NEMOTALE_UP_EVENT)) {
        ctx->prev_event_y = -9999;  // reset
        ctx->prev_event_x = -9999;  // reset
    }

    if (type & NEMOTALE_DOWN_EVENT) {
        tap->item = nemotale_path_pick_one(
                ctx->group, event->x, event->y);
        if (ntaps == 1) {
            nemocanvas_move(canvas, taps[0]->serial);
            struct pathone *one = tap->item;
            if (one && NTPATH_ID(one)) {
                if (!strcmp(NTPATH_ID(one), "b1")) {
                    ERR("Down");
                    nemotale_handle_canvas_update_event(NULL, canvas, tale);
                    nemotale_node_clear_path(ctx->node);
                    nemotale_node_damage_all(ctx->node);

                    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1.0, 1.0, 0.5, 0.5);
                    nemotale_path_update_one(ctx->group);
                    nemotale_node_render_path(ctx->node, ctx->group);
                    nemotale_composite(tale, NULL);
                    nemotale_handle_canvas_flush_event(NULL, canvas, NULL);
                }
            }
        } else if (ntaps == 2) {
            nemocanvas_pick(canvas,
                    taps[0]->serial,
                    taps[1]->serial,
                    (1 << NEMO_SURFACE_PICK_TYPE_SCALE));
        } else if (ntaps == 3) {
            // disable resize & rotate
            nemocanvas_pick(canvas,
                    taps[0]->serial,
                    taps[1]->serial,
                    0);
            if (nemotale_is_single_click(tale, event, type)) {
                nemotool_exit(tool);
            }
        }
    } else if (type & NEMOTALE_UP_EVENT) {
        struct pathone *one = tap->item;
        if (!strcmp(NTPATH_ID(one), "b1")) {
            ERR("UP");
            nemotale_handle_canvas_update_event(NULL, canvas, tale);
            nemotale_node_clear_path(ctx->node);
            nemotale_node_damage_all(ctx->node);

            nemotale_path_set_fill_color(NTPATH_STYLE(one), 1.0, 1.0, 0.0, 0.5);
            nemotale_path_update_one(ctx->group);
            nemotale_node_render_path(ctx->node, ctx->group);
            nemotale_composite(tale, NULL);
            nemotale_handle_canvas_flush_event(NULL, canvas, NULL);
        }
        if (nemotale_is_single_click(tale, event, type)) {
            if (ntaps == 1) {
                struct pathone *one = tap->item;
                if (one) LOG("%s", NTPATH_ID(one));
                if (one && NTPATH_ID(one)) {
                    if (!strcmp(NTPATH_ID(one), "b1")) {
                        ERR("CLICK");
                        nemotale_handle_canvas_update_event(NULL, canvas, tale);
                        nemotale_node_clear_path(ctx->node);
                        nemotale_node_damage_all(ctx->node);

                        nemotale_path_set_fill_color(NTPATH_STYLE(one), 0.0, 1.0, 1.0, 0.5);
                        nemotale_path_update_one(ctx->group);
                        nemotale_node_render_path(ctx->node, ctx->group);
                        nemotale_composite(tale, NULL);
                        nemotale_handle_canvas_flush_event(NULL, canvas, NULL);
                        _move(ctx);
                    }
                }
            }
        }
    }
}

int main()
{
    int w = 300, h = 600;
    struct Context *ctx = malloc(sizeof(struct Context));
    ctx->w = w;
    ctx->h = h;

    struct nemotool *tool;
    tool = nemotool_create();
    nemotool_connect_wayland(tool, NULL);

    struct nemocanvas *canvas;
    canvas = nemocanvas_create(tool);
    nemocanvas_set_size(canvas, w, h);
    nemocanvas_set_nemosurface(canvas, NEMO_SHELL_SURFACE_TYPE_NORMAL);
    nemocanvas_set_anchor(canvas, -0.5f, -0.5f);
    nemocanvas_set_dispatch_resize(canvas, _canvas_resize);

    nemocanvas_flip(canvas);
    nemocanvas_clear(canvas);
    ctx->canvas = canvas;

    struct nemotale *tale;
    tale = nemotale_create_pixman();
    nemotale_attach_pixman(tale,
            nemocanvas_get_data(canvas),
            nemocanvas_get_width(canvas),
            nemocanvas_get_height(canvas),
            nemocanvas_get_stride(canvas));
    nemotale_attach_canvas(tale, canvas, _tale_event);

    nemotale_set_userdata(tale, ctx);

    struct talenode *node;
    node = nemotale_node_create_pixman(w, h);
    nemotale_node_set_id(node, 1);
    nemotale_attach_node(tale, node);
    ctx->node = node;

    struct pathone *group;
    group = nemotale_path_create_group();
    ctx->group = group;

    struct pathone *one;
    one = nemotale_path_create_rect(w, h);
    nemotale_path_set_id(one, "bg");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_operator(one, CAIRO_OPERATOR_SOURCE);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1, 1, 1, 0.5);
    nemotale_path_attach_one(group, one);
    ctx->one_bg = one;

    struct pathone *c_group;
    c_group = nemotale_path_create_group();
    nemotale_path_attach_one(group, c_group);
    ctx->c_group = c_group;

    // top
    one = nemotale_path_create_rect(300, 100);
    nemotale_path_set_id(one, "top");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_operator(one, CAIRO_OPERATOR_SOURCE);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0.0, 1.0, 0.0, 0.5);
    nemotale_path_attach_one(group, one);
    ctx->top = one;

    // bottom
    one = nemotale_path_create_rect(300, 100);
    nemotale_path_set_id(one, "bottom");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_operator(one, CAIRO_OPERATOR_SOURCE);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0.5, 0.5, 1.0, 0.5);
    nemotale_path_translate(one, 0, 500);
    nemotale_path_attach_one(group, one);
    ctx->bottom = one;

    // contents
    one = nemotale_path_create_rect(300, 350);
    nemotale_path_set_id(one, "c1");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1.0, 0.0, 0.0, 0.5);
    nemotale_path_translate(one, 0, 100);
    nemotale_path_attach_one(c_group, one);
    ctx->c1 = one;

    // Button
    one = nemotale_path_create_rect(300, 50);
    nemotale_path_set_id(one, "b1");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1.0, 1.0, 0.0, 0.5);
    nemotale_path_translate(one, 0, 450);
    nemotale_path_attach_one(c_group, one);
    ctx->b1 = one;

    nemotale_path_update_one(group);
    nemotale_node_render_path(node, group);
    nemotale_composite(tale, NULL);

    nemocanvas_damage(canvas, 0, 0, 0, 0);
    nemocanvas_commit(canvas);

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
