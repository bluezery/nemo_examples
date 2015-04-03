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

    struct pathone *group;
    struct pathone *one_bg;
    double one_stroke_w;
    struct pathone *one1, *one1_txt, *one1_txt2;
    struct pathone *one2, *one2_txt, *one2_txt2;
    struct pathone *one3, *one3_txt, *one3_txt2;
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
    struct taletransition *trans1, *trans2, *trans3;

    LOG("fade start");

    trans1 = _transit_create(canvas, 0, 500, NEMOEASE_CUBIC_OUT_TYPE);
    //_transit_transform_path(trans, ctx->group);
    _transit_damage_path(trans1, ctx->node, ctx->group);
    _transit_go(trans1, canvas);

    trans2 = _transit_create(canvas, 1000, 500, NEMOEASE_CUBIC_OUT_TYPE);
    _transit_damage_path(trans2, ctx->node, ctx->group);
    _transit_go(trans2, canvas);

    trans3 = _transit_create(canvas, 2000, 500, NEMOEASE_CUBIC_OUT_TYPE);
    _transit_damage_path(trans3, ctx->node, ctx->group);
    _transit_go(trans3, canvas);

    // 1
    nemotale_transition_attach_dattrs(trans1,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one1)),
            4, 1.0f, 0.0f, 1.0f, 0.0f, 0.5f);
    nemotale_transition_attach_dattrs(trans1,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one1_txt)),
            4, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    nemotale_transition_attach_dattrs(trans1,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one1_txt2)),
            4, 1.0f, 0.0f, 1.0f, 0.0f, 0.5f);
    // 2
    nemotale_transition_attach_dattrs(trans2,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one2)),
            4, 1.0f, 1.0f, 1.0f, 0.0f, 0.5f);
    nemotale_transition_attach_dattrs(trans2,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one2_txt)),
            4, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    nemotale_transition_attach_dattrs(trans2,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one2_txt2)),
            4, 1.0f, 1.0f, 1.0f, 0.0f, 0.5f);
    // 3
    nemotale_transition_attach_dattrs(trans3,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one3)),
            4, 1.0f, 0.0f, 0.0f, 1.0f, 0.5f);
    nemotale_transition_attach_dattrs(trans3,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one3_txt)),
            4, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    nemotale_transition_attach_dattrs(trans3,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one3_txt2)),
            4, 1.0f, 0.0f, 0.0f, 1.0f, 0.5f);
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
    int w = 320, h = 190;
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
    nemotale_path_set_stroke_color(NTPATH_STYLE(one), 1, 1, 1, 1);
    nemotale_path_set_stroke_width(NTPATH_STYLE(one), 3);
    nemotale_path_attach_one(group, one);
    ctx->one_stroke_w = 3;
    ctx->one_bg = one;

    // 1
    one = nemotale_path_create_rect(50, 50);
    nemotale_path_set_id(one, "A-rect");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 1, 0, 0);
    nemotale_path_translate(one, 10, 10);
    nemotale_path_attach_one(group, one);
    ctx->one1 = one;

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "A-text");
    nemotale_path_attach_style(one, NULL);
	nemotale_path_set_font_family(NTPATH_STYLE(one),
            "LiberationMono");
	nemotale_path_set_font_size(NTPATH_STYLE(one), 50);
	nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 0, 0, 0);
    nemotale_path_translate(one, 20, 10);
	nemotale_path_attach_one(group, one);
	nemotale_path_load_text(one, "A", 1);
    ctx->one1_txt = one;

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "A-text2");
    nemotale_path_attach_style(one, NULL);
	nemotale_path_set_font_family(NTPATH_STYLE(one),
            "LiberationMono");
	nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
	nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 1, 0, 0);
    nemotale_path_translate(one, 70, 10);
	nemotale_path_attach_one(group, one);
	nemotale_path_load_text(one, "City Taxi Bla Bla",
            strlen("City Taxi Bla Bla"));
    ctx->one1_txt2 = one;

    // 2
    one = nemotale_path_create_rect(50, 50);
    nemotale_path_set_id(one, "B-rect");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1, 1, 0, 0);
    nemotale_path_translate(one, 10, 70);
    nemotale_path_attach_one(group, one);
    ctx->one2 = one;

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "B-text");
    nemotale_path_attach_style(one, NULL);
	nemotale_path_set_font_family(NTPATH_STYLE(one),
            "LiberationMono");
	nemotale_path_set_font_size(NTPATH_STYLE(one), 50);
	nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 0, 0, 0);
    nemotale_path_translate(one, 20, 70);
	nemotale_path_attach_one(group, one);
	nemotale_path_load_text(one, "B", 1);
    ctx->one2_txt = one;

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "B-text2");
    nemotale_path_attach_style(one, NULL);
	nemotale_path_set_font_family(NTPATH_STYLE(one),
            "LiberationMono");
	nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
	nemotale_path_set_fill_color(NTPATH_STYLE(one), 1, 1, 0, 0);
    nemotale_path_translate(one, 70, 70);
	nemotale_path_attach_one(group, one);
	nemotale_path_load_text(one, "Hotel Shuttle",
            strlen("Hotel Shuttle"));
    ctx->one2_txt2 = one;

    // 3
    one = nemotale_path_create_rect(50, 50);
    nemotale_path_set_id(one, "C-rect");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 0, 1, 0);
    nemotale_path_translate(one, 10, 130);
    nemotale_path_attach_one(group, one);
    ctx->one3 = one;

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "C-text");
    nemotale_path_attach_style(one, NULL);
	nemotale_path_set_font_family(NTPATH_STYLE(one),
            "LiberationMono");
	nemotale_path_set_font_size(NTPATH_STYLE(one), 50);
	nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 0, 0, 0);
    nemotale_path_translate(one, 20, 130);
	nemotale_path_attach_one(group, one);
	nemotale_path_load_text(one, "C", 1);
    ctx->one3_txt = one;

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "C-text2");
    nemotale_path_attach_style(one, NULL);
	nemotale_path_set_font_family(NTPATH_STYLE(one),
            "LiberationMono");
	nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
	nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 0, 1, 0);
    nemotale_path_translate(one, 70, 130);
	nemotale_path_attach_one(group, one);
	nemotale_path_load_text(one, "Parking Lot",
            strlen("Parking Lot"));
    ctx->one3_txt2 = one;
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
