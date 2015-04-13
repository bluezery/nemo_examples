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

    struct pathone *group, *icon_group;
    struct pathone *one_bg;
    double one_stroke_w;
    struct pathone *one1, *one2, *one3, *one4, *one5;
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

    nemotale_path_set_stroke_width(
            NTPATH_STYLE(ctx->one_bg), ctx->one_stroke_w/sx);
    nemotale_path_set_parent_transform(ctx->group, &matrix);
    nemotale_path_update_one(ctx->group);
    nemotale_node_render_path(node, ctx->group);


    nemotale_composite(tale, NULL);
    nemotale_handle_canvas_flush_event(NULL, canvas, NULL);
}

static void
_zoom_begin(struct Context *ctx, double x, double y, double zoom)
{
    struct nemocanvas *canvas = ctx->canvas;
    struct taletransition *trans1;

    trans1 = _transit_create(canvas, 0, 1000, NEMOEASE_CUBIC_IN_TYPE);
    _transit_transform_path(trans1, ctx->icon_group);
    _transit_damage_path(trans1, ctx->node, ctx->group);
    _transit_go(trans1, canvas);

    nemotale_path_transform_enable(ctx->icon_group);
    nemotale_transition_attach_signal(trans1,
            NTPATH_DESTROY_SIGNAL(ctx->icon_group));
    nemotale_transition_attach_dattr(trans1,
            NTPATH_TRANSFORM_ATSX(ctx->icon_group), 1.0f, zoom);
    nemotale_transition_attach_dattr(trans1,
            NTPATH_TRANSFORM_ATSY(ctx->icon_group), 1.0f, zoom);
    nemotale_transition_attach_dattr(trans1,
            NTPATH_TRANSFORM_ATX(ctx->icon_group), 1.0f, x);
    nemotale_transition_attach_dattr(trans1,
            NTPATH_TRANSFORM_ATY(ctx->icon_group), 1.0f, y);
}

static void
_tale_event(struct nemotale *tale, struct talenode *node, uint32_t type, struct taleevent *event)
{
    struct Context *ctx = nemotale_get_userdata(tale);
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotool *tool = nemocanvas_get_tool(canvas);

    struct taletap *taps[16];
    int ntaps;
    struct taletap *tap;

    ntaps = nemotale_get_node_taps(tale, node, taps, type);
    tap = nemotale_get_tap(tale, event->device, type);

    if ((type & NEMOTALE_DOWN_EVENT) ||
        (type & NEMOTALE_UP_EVENT)) {
        ctx->prev_event_y = -9999;  // reset
        ctx->prev_event_x = -9999;  // reset
    }

    if (type & NEMOTALE_DOWN_EVENT) {
        tap->item = nemotale_path_pick_one(
                ctx->group, event->x, event->y);
        if (ntaps == 1) {
            //nemocanvas_move(canvas, taps[0]->serial);
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
    } else if (nemotale_is_single_click(tale, event, type)) {
        if (ntaps == 1) {
            struct pathone *one = tap->item;
            if (one) LOG("%s", NTPATH_ID(one));
            if (one && NTPATH_ID(one)) {
                if (ctx->zoom_state == 0) {
                    _zoom_begin(ctx,
                        -NTPATH_TRANSFORM_X(one), -NTPATH_TRANSFORM_Y(one), 1.0);
                    ctx->zoom_state = 1;
                    ctx->prev_x = NTPATH_TRANSFORM_X(ctx->icon_group);
                    ctx->prev_y = NTPATH_TRANSFORM_Y(ctx->icon_group);
                } else {
                    _zoom_begin(ctx,
                        ctx->prev_x, ctx->prev_y, 0.3);
                    ctx->zoom_state = 0;
                }
            }
        }
    } else if (type & NEMOTALE_MOTION_EVENT) {
        if (ntaps == 1) {
            // scrolling
            if ((ctx->prev_event_y == -9999) ||
                (ctx->prev_event_x == -9999)) {
                ctx->prev_event_x = event->x; // reset
                ctx->prev_event_y = event->y; // reset
                return;
            }
            double pos_x = event->x - ctx->prev_event_x;
            double pos_y = event->y - ctx->prev_event_y;
            ctx->prev_event_x = event->x;
            ctx->prev_event_y = event->y;

            nemotale_handle_canvas_update_event(NULL, canvas, tale);
            nemotale_node_damage_all(ctx->node);

            nemotale_path_translate(ctx->icon_group,
                NTPATH_TRANSFORM_X(ctx->icon_group) + pos_x,
                NTPATH_TRANSFORM_Y(ctx->icon_group) + pos_y);

            nemotale_path_update_one(ctx->group);
            nemotale_node_render_path(ctx->node, ctx->group);
            nemotale_composite(tale, NULL);
            nemotale_handle_canvas_flush_event(NULL, canvas, NULL);
        }
    }
}

int main()
{
    int w = 300, h = 300;
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
    nemotale_path_set_stroke_color(NTPATH_STYLE(one), 1, 1, 1, 1);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1, 1, 1, 0.5);
    nemotale_path_set_stroke_width(NTPATH_STYLE(one), 3);
    nemotale_path_attach_one(group, one);
    ctx->one_stroke_w = 3;
    ctx->one_bg = one;

    struct pathone *icon_group;
    icon_group = nemotale_path_create_group();
    nemotale_path_attach_one(group, icon_group);
    ctx->icon_group = icon_group;

    // 1
    one = nemotale_path_create_rect(300, 300);
    nemotale_path_set_id(one, "one1");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0.4, 0.4, 0.4, 0.5);
    nemotale_path_translate(one, -500, -600);
    nemotale_path_attach_one(icon_group, one);
    ctx->one1 = one;

    // 2
    one = nemotale_path_create_rect(300, 300);
    nemotale_path_set_id(one, "one2");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0.4, 0, 0, 0.5);
    nemotale_path_translate(one, 500, 600);
    nemotale_path_attach_one(icon_group, one);
    ctx->one2 = one;

    // 3
    one = nemotale_path_create_rect(300, 300);
    nemotale_path_set_id(one, "one3");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 0.4, 0, 0.5);
    nemotale_path_attach_one(icon_group, one);
    ctx->one3 = one;

    // 4
    one = nemotale_path_create_rect(300, 300);
    nemotale_path_set_id(one, "one4");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 0, 0.4, 0.5);
    nemotale_path_translate(one, -500, 100);
    nemotale_path_attach_one(icon_group, one);
    ctx->one4 = one;

    // 5
    one = nemotale_path_create_rect(300, 300);
    nemotale_path_set_id(one, "one5");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0.4, 0.4, 0, 0.5);
    nemotale_path_translate(one, 600, 100);
    nemotale_path_attach_one(icon_group, one);
    ctx->one5 = one;

    nemotale_path_scale(icon_group, 0.3, 0.3);

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
