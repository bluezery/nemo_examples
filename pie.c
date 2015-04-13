#include <stdlib.h>

#include <nemotool.h>
#include <nemocanvas.h>
#include <nemotale.h>
#include <talenode.h>
#include <talepixman.h>

#include <math.h>
#include <pathstyle.h>
#include "talehelper.h"

#include "util.h"
#include "nemohelper.h"

struct Pie {
    int power;
    struct pathone *one;
    double start, end;
    double r, g, b;
};

struct PieView {
    int r, ir;
    bool dirty;
    void *data;

    List *pies;
    List *pies_del;
    struct Pie *selected;

    struct pathone *group;
    struct pathone *loading_one;
    struct taletransition *loading_trans;

    struct taletransition *update_trans;
};

struct Context {
    struct nemocanvas *canvas;
    struct talenode *node;

    struct pathone *group;
    struct pathone *one_bg;

    int w, h;

    struct PieView *pieview;
};

static void
_pieview_set_data(struct PieView *pieview, void *data)
{
    pieview->data = data;
}

static void *
_pieview_get_data(struct PieView *pieview)
{
    return pieview->data;
}

static void _pieview_loading(struct PieView *view);
static void
_pieview_reloading(struct taletransition *trans, void *ctx, void *data)
{
    _pieview_loading(data);
}

static void
_pieview_loading(struct PieView *pieview)
{
    struct Context *ctx = _pieview_get_data(pieview);
    struct nemocanvas *canvas = ctx->canvas;
    struct taletransition *trans;

    trans = _transit_create(canvas, 0, 1500, NEMOEASE_CUBIC_OUT_TYPE);
    _transit_transform_path(trans, pieview->loading_one);
    _transit_damage_path(trans, ctx->node, ctx->group);
    _transit_add_event_end(trans, _pieview_reloading, ctx, pieview);
    _transit_go(trans, canvas);

    nemotale_path_transform_enable(pieview->loading_one);
    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(pieview->loading_one));

    nemotale_transition_attach_dattr(trans,
            NTPATH_CIRCLE_ATINNERRADIUS(pieview->loading_one), 0.5f, 10);
    nemotale_transition_attach_dattr(trans,
            NTPATH_CIRCLE_ATINNERRADIUS(pieview->loading_one), 1.0f, pieview->ir);
    nemotale_transition_attach_dattr(trans,
            NTPATH_CIRCLE_ATR(pieview->loading_one), 0.5f, 100);
    nemotale_transition_attach_dattr(trans,
            NTPATH_CIRCLE_ATR(pieview->loading_one), 1.0f, pieview->r);
    nemotale_transition_attach_dattr(trans,
            NTPATH_TRANSFORM_ATX(pieview->loading_one), 0.5f, 60);
    nemotale_transition_attach_dattr(trans,
            NTPATH_TRANSFORM_ATX(pieview->loading_one), 1.0f, 10);
    nemotale_transition_attach_dattr(trans,
            NTPATH_TRANSFORM_ATY(pieview->loading_one), 0.5f, 60);
    nemotale_transition_attach_dattr(trans,
            NTPATH_TRANSFORM_ATY(pieview->loading_one), 1.0f, 10);
    nemotale_transition_attach_dattrs(trans,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(pieview->loading_one)),
            4, 0.5f, 0.0f, 1.0f, 0.0f, 0.1f);
    nemotale_transition_attach_dattrs(trans,
            NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(pieview->loading_one)),
            4, 1.0f, 0.0f, 1.0f, 0.0f, 0.5f);
    pieview->loading_trans = trans;
}

static struct PieView *
_pieview_create()
{
    struct PieView *pieview;
    pieview = calloc(sizeof(struct PieView), 1);
    return pieview;
}

static void
_pieview_destroy(struct PieView *pieview)
{
    List *l, *ll;
    struct Pie *pie;
    LIST_FOR_EACH_SAFE(pieview->pies, l, ll, pie) {
        pieview->pies = list_data_remove(pieview->pies, pie);
        nemotale_path_destroy_one(pie->one);
        free(pie);
    }
    LIST_FOR_EACH_SAFE(pieview->pies_del, l, ll, pie) {
        pieview->pies_del = list_data_remove(pieview->pies_del, pie);
        nemotale_path_destroy_one(pie->one);
        free(pie);
    }
    if (pieview->group) nemotale_path_destroy_one(pieview->group);
    if (pieview->loading_one) nemotale_path_destroy_one(pieview->loading_one);
    if (pieview->loading_trans)
        nemotale_transition_destroy(pieview->loading_trans);
    if (pieview->update_trans)
        nemotale_transition_destroy(pieview->update_trans);
    free(pieview);
}

static int
_pieview_count(struct PieView *pieview)
{
    return list_count(pieview->pies);
}

static int
_pieview_get_power(struct PieView *pieview)
{
    int pow = 0;
    List *l;
    struct Pie *pie;
    LIST_FOR_EACH(pieview->pies, l, pie) {
        pow += pie->power;
    }
    return pow;
}

static struct Pie *
_pieview_add_ap(struct PieView *pieview, int power)
{
    struct Pie *pie;
    struct pathone *one;

    double endangle = M_PI * 2;
    int cnt = _pieview_count(pieview);
    if (cnt > 0) {
        struct Pie *last = pieview->pies->data;
        endangle = NTPATH_CIRCLE(last->one)->startangle;
    }

    pie = calloc(sizeof(struct Pie), 1);
    pie->power = power;

    pie->r = (double)rand()/RAND_MAX;
    pie->g = (double)rand()/RAND_MAX;
    pie->b = (double)rand()/RAND_MAX;
    one = nemotale_path_create_circle(pieview->r);
    nemotale_path_set_data(one, pie);
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_donut_circle(one, pieview->ir);
    nemotale_path_set_fill_color(NTPATH_STYLE(one),
            pie->r, pie->g, pie->b, 0.0);
    nemotale_path_translate(one, 10, 10);
    nemotale_path_attach_one(pieview->group, one);
    nemotale_path_set_pie_circle(one, endangle, endangle);
    pie->one = one;

    pieview->pies = list_data_insert(pieview->pies, pie);
    pieview->dirty = true;

    return pie;
}

static void
_pieview_del_ap(struct PieView *pieview, struct Pie *pie)
{
    pieview->pies = list_data_remove(pieview->pies, pie);
    pieview->pies_del = list_data_insert(pieview->pies_del, pie);
    nemotale_path_set_data(pie->one, NULL);
    pieview->dirty = true;
}

static void
_pieview_select(struct PieView *pieview, struct Pie *pie)
{
    struct Context *ctx = _pieview_get_data(pieview);
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotale *tale = nemocanvas_get_userdata(canvas);
    struct pathone *one = pie->one;

#if 0
    if (pieview->update_trans) {
        ERR("destroy");
        nemotale_transition_destroy(pieview->update_trans);
        pieview->update_trans = NULL;
    }
#endif

    nemotale_handle_canvas_update_event(NULL, canvas, tale);

    nemotale_path_detach_one(pieview->group, one);
    nemotale_path_attach_one(pieview->group, one);
    nemotale_path_set_stroke_color(NTPATH_STYLE(one),
            1, 1, 1, 1.0);
    nemotale_path_set_stroke_width(NTPATH_STYLE(one), 5.0f);

    nemotale_node_clear_path(ctx->node);
    nemotale_node_damage_all(ctx->node);

    nemotale_path_update_one(pieview->group);
    nemotale_node_render_path(ctx->node, ctx->group);

    nemotale_composite(tale, NULL);
    nemotale_handle_canvas_flush_event(NULL, canvas, NULL);

    pieview->selected = pie;
    pieview->dirty = true;
}

static void
_pieview_unselect(struct PieView *pieview)
{
    struct Pie *pie = pieview->selected;
    if (!pie) return;

    struct Context *ctx = _pieview_get_data(pieview);
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotale *tale = nemocanvas_get_userdata(canvas);
    struct pathone *one = pie->one;

    nemotale_handle_canvas_update_event(NULL, canvas, tale);

    nemotale_path_set_stroke_color(NTPATH_STYLE(one),
            pie->r, pie->g, pie->b, 0.0);
    nemotale_path_set_stroke_width(NTPATH_STYLE(one), 0.0f);

    nemotale_node_clear_path(ctx->node);
    nemotale_node_damage_all(ctx->node);

    nemotale_path_update_one(pieview->group);
    nemotale_node_render_path(ctx->node, ctx->group);

    nemotale_composite(tale, NULL);
    nemotale_handle_canvas_flush_event(NULL, canvas, NULL);

    pieview->selected = NULL;
    pieview->dirty = true;
}

static void
_pieview_update_end(struct taletransition *trans, void *ctx, void *data)
{
    struct PieView *pieview = data;
    List *l, *ll;
    struct Pie *pie;
    LIST_FOR_EACH_SAFE(pieview->pies_del, l, ll, pie) {
        pieview->pies_del = list_data_remove(pieview->pies_del, pie);
        nemotale_path_destroy_one(pie->one);
        free(pie);
    }
    pieview->update_trans = NULL;
}

static void
_pieview_update(struct PieView *pieview)
{
    if (!pieview->dirty) return;

    struct Context *ctx = _pieview_get_data(pieview);
    struct nemocanvas *canvas = ctx->canvas;
    int pow;

    List *l;
    struct Pie *pie;


    pow = _pieview_get_power(pieview);

    struct taletransition *trans;
    trans = _transit_create(canvas, 0, 1000, NEMOEASE_CUBIC_OUT_TYPE);

    double start = 0;
    LIST_FOR_EACH_REVERSE(pieview->pies, l, pie) {
        double end = ((double)pie->power/pow) * M_PI * 2 + start;

        nemotale_transition_attach_dattr(trans,
                NTPATH_CIRCLE_ATSTARTANGLE(pie->one), 1.0f, start);
        nemotale_transition_attach_dattr(trans,
                NTPATH_CIRCLE_ATENDANGLE(pie->one), 1.0f, end);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(pie->one)),
                4, 1.0f, pie->r, pie->g, pie->b, 0.8f);
        _transit_transform_path(trans, pie->one);

        start = end;
    }

    // Remove animation
    LIST_FOR_EACH_REVERSE(pieview->pies_del, l, pie) {
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(pie->one)),
                4, 1.0f, pie->r, pie->g, pie->b, 0.0f);
        _transit_add_event_end(trans, _pieview_update_end, ctx, pieview);
    }

    _transit_damage_path(trans, ctx->node, ctx->group);
    _transit_go(trans, canvas);
    pieview->update_trans = trans;
}

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

    nemotale_path_set_parent_transform(ctx->group, &matrix);
    nemotale_path_update_one(ctx->group);
    nemotale_node_render_path(node, ctx->group);

    nemotale_composite(tale, NULL);
    nemotale_handle_canvas_flush_event(NULL, canvas, NULL);
}

static void
_tale_event(struct nemotale *tale, struct talenode *node, uint32_t type, struct taleevent *event)
{
    struct Context *ctx = nemotale_get_userdata(tale);
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    struct PieView *pieview = ctx->pieview;

    struct taletap *taps[16];
    int ntaps;
    struct taletap *tap;

    ntaps = nemotale_get_node_taps(tale, node, taps, type);
    tap = nemotale_get_tap(tale, event->device, type);

    if (type & NEMOTALE_DOWN_EVENT) {
        tap->item = nemotale_path_pick_one(
                pieview->group, event->x, event->y);
        if (ntaps == 1) {
            struct pathone *one = tap->item;
            if (!one || !NTPATH_ID(one) ||
                !strcmp(NTPATH_ID(one), "bg") ||
                !strcmp(NTPATH_ID(one), "loading")) {
                nemocanvas_move(canvas, taps[0]->serial);
            } else {
                struct Pie *pie = nemotale_path_get_data(one);
                _pieview_select(pieview, pie);
            }
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
    } else {
        _pieview_unselect(pieview);
        if (nemotale_is_single_click(tale, event, type)) {
            if (ntaps == 1) {
                struct pathone *one = tap->item;
                if (one) LOG("%s", NTPATH_ID(one));
            }
        }
    }
}

static void
_timeout(struct nemotimer *timer, void *data)
{
    struct Context *ctx = data;
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotale *tale = nemocanvas_get_userdata(canvas);
    struct PieView *pieview = ctx->pieview;

    if (pieview->loading_trans) {
        nemotale_transition_destroy(pieview->loading_trans);
        pieview->loading_trans = NULL;
    }
    if (pieview->loading_one) {
        nemotale_path_destroy_one(pieview->loading_one);
        pieview->loading_one = NULL;
    }

    if (rand() % 3) {
        int pow = ((double)rand()/RAND_MAX) * 100;
        _pieview_add_ap(ctx->pieview, pow);
    } else if (list_count(ctx->pieview->pies) > 0) {
        int idx = rand() % list_count(ctx->pieview->pies);
        struct Pie *pie = list_idx_get_data(ctx->pieview->pies, idx);
        if (pie) _pieview_del_ap(ctx->pieview, pie);
    }
    _pieview_update(ctx->pieview);

    nemotale_handle_canvas_update_event(NULL, canvas, tale);

    nemotale_node_damage_all(ctx->node);

    nemotale_path_update_one(pieview->group);
    nemotale_node_render_path(ctx->node, ctx->group);

    nemotale_composite(tale, NULL);
    nemotale_handle_canvas_flush_event(NULL, canvas, NULL);

    nemotimer_set_timeout(timer, 2000);
}


int main()
{
    int w = 320, h = 320;
    struct Context *ctx = calloc(sizeof(struct Context), 1);
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

    struct pathone *group, *pie_group;
    group = nemotale_path_create_group();
    ctx->group = group;

    struct pathone *one;
    one = nemotale_path_create_rect(w, h);
    nemotale_path_set_id(one, "bg");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_operator(one, CAIRO_OPERATOR_SOURCE);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1.0f, 1.0f, 1.0f, 0.0f);
    nemotale_path_attach_one(group, one);
    ctx->one_bg = one;

    ctx->pieview = _pieview_create();
    _pieview_set_data(ctx->pieview, ctx);

    pie_group = nemotale_path_create_group();
    nemotale_path_attach_one(group, pie_group);
    ctx->pieview->group = pie_group;

    ctx->pieview->r = (w - 20)/2;
    ctx->pieview->ir = 80;
    one = nemotale_path_create_circle(ctx->pieview->r);
    nemotale_path_set_id(one, "loading");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 1, 0, 0.5);
    nemotale_path_set_pivot_type(one, NEMOTALE_PATH_PIVOT_RATIO);
    nemotale_path_set_donut_circle(one, ctx->pieview->ir);
    nemotale_path_translate(one, 10, 10);
    nemotale_path_attach_one(pie_group, one);
    ctx->pieview->loading_one = one;
    _pieview_loading(ctx->pieview);

#if 1
    struct nemotimer *timer = nemotimer_create(tool);
    nemotimer_set_timeout(timer, 3000);
    nemotimer_set_callback(timer, _timeout);
    nemotimer_set_userdata(timer, ctx);
#endif

    nemotale_path_update_one(group);
    nemotale_node_render_path(node, group);
    nemotale_composite(tale, NULL);
    nemocanvas_damage(canvas, 0, 0, 0, 0);
    nemocanvas_commit(canvas);

    nemotool_run(tool);

    _pieview_destroy(ctx->pieview);
    free(ctx);

    nemotale_path_destroy_one(group);
    nemotale_node_destroy(node);

    nemotale_destroy(tale);
    nemocanvas_destroy(canvas);

    nemotool_disconnect_wayland(tool);
    nemotool_destroy(tool);

    return 0;
}
