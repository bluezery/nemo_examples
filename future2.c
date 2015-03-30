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

    struct pathone *group, *icon_group, *zoom_group;
    struct pathone *one_bg;
    double one_stroke_w;
    struct pathone *one1, *one11, *one2, *one3, *one31, *one32, *one33, *one34;
    struct pathone *one4, *one41, *one42, *one43, *one44, *one45;
    int w, h;
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
_fade_end(struct taletransition *trans, void *_ctx, void *data)
{
    LOG("fade end");
}

static void
_fade_begin(struct Context *ctx)
{
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    struct nemotale *tale =nemocanvas_get_userdata(canvas);
    struct taletransition *trans1, *trans2, *trans3, *trans4;

    trans1 = _transit_create(canvas, 1000, 3000, NEMOEASE_CUBIC_OUT_TYPE);
    _transit_damage_path(trans1, ctx->node, ctx->group);
    _transit_transform_path(trans1, ctx->zoom_group);
    _transit_go(trans1, canvas);

    nemotale_path_transform_enable(ctx->zoom_group);
    nemotale_transition_attach_signal(trans1,
            NTPATH_DESTROY_SIGNAL(ctx->zoom_group));
    nemotale_transition_attach_dattr(trans1,
            NTPATH_TRANSFORM_ATSX(ctx->zoom_group), 1.0f, 1.0f);
    nemotale_transition_attach_dattr(trans1,
            NTPATH_TRANSFORM_ATSY(ctx->zoom_group), 1.0f, 1.0f);
    nemotale_transition_attach_dattr(trans1,
            NTPATH_TRANSFORM_ATX(ctx->zoom_group), 1.0f, 0);
    nemotale_transition_attach_dattr(trans1,
            NTPATH_TRANSFORM_ATY(ctx->zoom_group), 1.0f, 0);

    trans2 = _transit_create(canvas, 2000, 2000, NEMOEASE_CUBIC_IN_TYPE);
    _transit_damage_path(trans2, ctx->node, ctx->group);
    _transit_go(trans2, canvas);

    nemotale_transition_attach_signal(trans2,
            NTPATH_DESTROY_SIGNAL(ctx->one4));
    nemotale_transition_attach_dattrs(trans2,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one4)),
            4.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);

    trans3 = _transit_create(canvas, 4000, 2000, NEMOEASE_CUBIC_OUT_TYPE);
    _transit_damage_path(trans3, ctx->node, ctx->group);
    _transit_transform_path(trans3, ctx->zoom_group);
    _transit_go(trans3, canvas);

    nemotale_path_transform_enable(ctx->zoom_group);
    nemotale_transition_attach_signal(trans3,
            NTPATH_DESTROY_SIGNAL(ctx->zoom_group));
    nemotale_transition_attach_dattr(trans3,
            NTPATH_TRANSFORM_ATY(ctx->zoom_group), 1.0f, 230);

    nemotale_transition_attach_dattrs(trans3,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one41)),
            4.0f, 1.0f, 0.5f, 0.0f, 0.0f, 0.0f);
    nemotale_transition_attach_dattrs(trans3,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one42)),
            4.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.0f);
    nemotale_transition_attach_dattrs(trans3,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one43)),
            4.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.0f);
    nemotale_transition_attach_dattrs(trans3,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one44)),
            4.0f, 1.0f, 0.5f, 0.0f, 0.5f, 0.0f);
    nemotale_transition_attach_dattrs(trans3,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one45)),
            4.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.0f);

    trans4 = _transit_create(canvas, 4000, 2000, NEMOEASE_CUBIC_OUT_TYPE);
    _transit_damage_path(trans4, ctx->node, ctx->group);
    _transit_transform_path(trans4, ctx->icon_group);
    _transit_transform_path(trans4, ctx->one3);
    _transit_transform_path(trans4, ctx->one31);
    _transit_transform_path(trans4, ctx->one32);
    _transit_transform_path(trans4, ctx->one33);
    _transit_transform_path(trans4, ctx->one34);
    _transit_go(trans4, canvas);

    nemotale_path_transform_enable(ctx->icon_group);
    nemotale_transition_attach_signal(trans4,
            NTPATH_DESTROY_SIGNAL(ctx->icon_group));
    nemotale_transition_attach_dattr(trans4,
            NTPATH_TRANSFORM_ATY(ctx->icon_group), 1.0f, 300);

    nemotale_path_transform_enable(ctx->one3);
    nemotale_transition_attach_signal(trans4,
            NTPATH_DESTROY_SIGNAL(ctx->one3));
    nemotale_transition_attach_dattr(trans4,
            NTPATH_TRANSFORM_ATX(ctx->one3), 1.0f, 0);
    nemotale_transition_attach_dattrs(trans4,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one3)),
            4.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);

    nemotale_path_transform_enable(ctx->one31);
    nemotale_transition_attach_signal(trans4,
            NTPATH_DESTROY_SIGNAL(ctx->one31));
    nemotale_transition_attach_dattr(trans4,
            NTPATH_TRANSFORM_ATX(ctx->one31), 1.0f, 200);
    nemotale_transition_attach_dattrs(trans4,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one31)),
            4.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);

    nemotale_path_transform_enable(ctx->one32);
    nemotale_transition_attach_signal(trans4,
            NTPATH_DESTROY_SIGNAL(ctx->one32));
    nemotale_transition_attach_dattr(trans4,
            NTPATH_TRANSFORM_ATX(ctx->one32), 1.0f, 400);
    nemotale_transition_attach_dattrs(trans4,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one32)),
            4.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f);

    nemotale_path_transform_enable(ctx->one33);
    nemotale_transition_attach_signal(trans4,
            NTPATH_DESTROY_SIGNAL(ctx->one33));
    nemotale_transition_attach_dattr(trans4,
            NTPATH_TRANSFORM_ATX(ctx->one33), 1.0f, 600);
    nemotale_transition_attach_dattrs(trans4,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one33)),
            4.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);

    nemotale_path_transform_enable(ctx->one34);
    nemotale_transition_attach_signal(trans4,
            NTPATH_DESTROY_SIGNAL(ctx->one34));
    nemotale_transition_attach_dattr(trans4,
            NTPATH_TRANSFORM_ATX(ctx->one34), 1.0f, 800);
    nemotale_transition_attach_dattrs(trans4,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one34)),
            4.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
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

    if (type & NEMOTALE_DOWN_EVENT) {
        tap->item = nemotale_path_pick_one(
                ctx->group, event->x, event->y);
        if (ntaps == 1) {
            nemocanvas_move(canvas, taps[0]->serial);
        } else if (ntaps == 2) {
            nemocanvas_pick(canvas,
                    taps[0]->serial,
                    taps[1]->serial,
                    (1 << NEMO_SURFACE_PICK_TYPE_SCALE));
        } else if (ntaps == 3) {
            if (nemotale_is_single_click(tale, event, type)) {
                nemotool_exit(tool);
            }
        }
    } else if (nemotale_is_single_click(tale, event, type)) {
        if (ntaps == 1) {
            struct pathone *one = tap->item;
            if (one) LOG("%s", NTPATH_ID(one));
        }
    }
}

int main()
{
    int w = 300, h = 400;
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
    nemotale_path_translate(icon_group, -1, 0);
    nemotale_path_translate(icon_group, 1, 0);
    ctx->icon_group = icon_group;

    double sy = -300;
    // 1
    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "one1");
    nemotale_path_attach_style(one, NULL);
	nemotale_path_set_font_family(NTPATH_STYLE(one), "LiberationMono");
	nemotale_path_set_font_size(NTPATH_STYLE(one), 80);
	nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 1, 0, 0.8);
    nemotale_path_translate(one, 0, sy);
	nemotale_path_attach_one(icon_group, one);
	nemotale_path_load_text(one, "1600", 4);
    ctx->one1 = one;

    one = nemotale_path_create_rect(150, 150);
    nemotale_path_set_id(one, "one11");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0.4, 0.4, 0.4, 1.0);
    nemotale_path_translate(one, 150, sy);
    nemotale_path_attach_one(icon_group, one);
    ctx->one11 = one;

    // 2
    one = nemotale_path_create_rect(300, 100);
    nemotale_path_set_id(one, "one2");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 1, 0, 1.0);
    nemotale_path_translate(one, 0, 50 + sy);
    nemotale_path_attach_one(icon_group, one);
    ctx->one2 = one;

    // 3
    one = nemotale_path_create_rect(200, 150);
    nemotale_path_set_id(one, "one3");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1, 0, 0, 0.0);
    nemotale_path_translate(one, -300, 150 + sy);
    nemotale_path_attach_one(icon_group, one);
    ctx->one3 = one;

    one = nemotale_path_create_rect(200, 150);
    nemotale_path_set_id(one, "one31");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1, 1, 0, 0.0);
    nemotale_path_translate(one, -100, 150 + sy);
    nemotale_path_attach_one(icon_group, one);
    ctx->one31 = one;

    one = nemotale_path_create_rect(200, 150);
    nemotale_path_set_id(one, "one32");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 1, 1, 0.0);
    nemotale_path_translate(one, 0, 150 + sy);
    nemotale_path_attach_one(icon_group, one);
    ctx->one32 = one;

    one = nemotale_path_create_rect(200, 150);
    nemotale_path_set_id(one, "one33");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 1, 0, 0.0);
    nemotale_path_translate(one, 100, 150 + sy);
    nemotale_path_attach_one(icon_group, one);
    ctx->one33 = one;

    one = nemotale_path_create_rect(200, 150);
    nemotale_path_set_id(one, "one34");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 0, 1, 0.0);
    nemotale_path_translate(one, 200, 150 + sy);
    nemotale_path_attach_one(icon_group, one);
    ctx->one34 = one;

    // 4
    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "one4");
    nemotale_path_attach_style(one, NULL);
	nemotale_path_set_font_family(NTPATH_STYLE(one), "LiberationMono");
	nemotale_path_set_font_size(NTPATH_STYLE(one), 80);
	nemotale_path_set_fill_color(NTPATH_STYLE(one), 1, 1, 1, 0.0);
	nemotale_path_load_text(one, "KITCHEN", 7);
    nemotale_path_translate(one, 0, 300 + sy);
	nemotale_path_attach_one(icon_group, one);
    ctx->one4 = one;

    struct pathone *zoom_group;
    zoom_group = nemotale_path_create_group();
    nemotale_path_attach_one(group, zoom_group);
    nemotale_path_scale(zoom_group, 2, 2);
    nemotale_path_translate(zoom_group, -500, -200);
    ctx->zoom_group = zoom_group;

    // 5
    one = nemotale_path_create_rect(100, 150);
    nemotale_path_set_id(one, "one41");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0.5, 0, 0, 1.0);
    nemotale_path_translate(one, 120, 400 + sy);
    nemotale_path_attach_one(zoom_group, one);
    ctx->one41 = one;

    one = nemotale_path_create_rect(150, 100);
    nemotale_path_set_id(one, "one42");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 0, 0.5, 1.0);
    nemotale_path_translate(one, 0, 500 + sy);
    nemotale_path_attach_one(zoom_group, one);
    ctx->one42 = one;

    one = nemotale_path_create_rect(150, 100);
    nemotale_path_set_id(one, "one43");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 0, 0.5, 1.0);
    nemotale_path_translate(one, 0, 500 + sy);
    nemotale_path_attach_one(zoom_group, one);
    ctx->one43 = one;

    one = nemotale_path_create_rect(100, 150);
    nemotale_path_set_id(one, "one44");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0.5, 0, 0.5, 1.0);
    nemotale_path_translate(one, 60, 580 + sy);
    nemotale_path_attach_one(zoom_group, one);
    ctx->one44 = one;

    one = nemotale_path_create_rect(100, 150);
    nemotale_path_set_id(one, "one45");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0.5, 0, 1, 1.0);
    nemotale_path_translate(one, 260, 430 + sy);
    nemotale_path_attach_one(zoom_group, one);
    ctx->one45 = one;

    nemotale_path_update_one(group);
    nemotale_node_render_path(node, group);
    nemotale_composite(tale, NULL);

    nemocanvas_damage(canvas, 0, 0, 0, 0);
    nemocanvas_commit(canvas);

    _fade_begin(ctx);
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
