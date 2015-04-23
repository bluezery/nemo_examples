#include <nemotool.h>
#include <nemocanvas.h>
#include <nemoegl.h>
#include <nemotale.h>
#include <talenode.h>
#include <talepixman.h>

#include <math.h>
#include <pathpaint.h>
#include "talehelper.h"

#include "util.h"
#include "nemohelper.h"


typedef enum
{
    TABLE_STATE_SCROLL = 0,
    TABLE_STATE_SELECTED
} TableState;

typedef struct _TableView TableView;
typedef struct _TableItem TableItem;

struct _TableItem {
    struct pathone *one;
    TableView *tv;
    double r, g, b;
};

struct _TableView {
    struct talenode *node;
    struct pathone *bg;

    struct talenode *item_node;
    struct pathone *item_group;
    List *items;

    TableState state;
    int gap;
};

struct Context {
    _Win *win;
    struct talenode *node;
    struct pathone *bg;

    struct pathone *title_bg;
    int w, h;

    int prev_event_y;
    int prev_event_x;

    TableView *tv;
};

static double
_table_item_get_width(TableItem *ti)
{
    return NTPATH_WIDTH(ti->one) * NTPATH_TRANSFORM_SX(ti->one);
}

static double
_table_item_get_height(TableItem *ti)
{
    return NTPATH_HEIGHT(ti->one) * NTPATH_TRANSFORM_SY(ti->one);
}


static double
_table_item_get_x(TableItem *ti)
{
    return (NTPATH_X(ti->one) + NTPATH_TRANSFORM_X(ti->one)) * NTPATH_TRANSFORM_SX(ti->one);
}

static double
_table_item_get_y(TableItem *ti)
{
    return (NTPATH_Y(ti->one) + NTPATH_TRANSFORM_Y(ti->one)) * NTPATH_TRANSFORM_SY(ti->one);
}

static struct nemopath *
_title_bg_create(double w, double h, double r)
{
    struct nemopath *path;

    path = nemopath_create();
    nemopath_move_to(path, r, 0);
    nemopath_line_to(path, w - r, 0);
    nemopath_curve_to(path,
            w - r, 0,
            w,  0,
            w, r);
    nemopath_line_to(path, w, h);
    nemopath_line_to(path, -w + r, h);
    nemopath_line_to(path, 0, r);
    nemopath_curve_to(path,
            0, r,
            0, 0,
            r, 0);
    nemopath_close_path(path);

    return path;
}

static struct nemopath *
_body_bg_create(double w, double h, double r)
{
    struct nemopath *path;

    ERR("%lf %lf", w, h);
    path = nemopath_create();
    nemopath_line_to(path, w - r, 0);
    nemopath_curve_to(path,
            w - r, 0,
            w, 0,
            w, r);
    nemopath_line_to(path, w, h - r);
    nemopath_curve_to(path,
            w, h - r,
            w, h,
            w - r, h);
    nemopath_line_to(path, r, h);
    nemopath_curve_to(path,
            r, h,
            0, h,
            0, h - r);
    nemopath_line_to(path, 0, 0);
    nemopath_close_path(path);

    return path;
}

static TableView *
_table_create(struct nemotale *tale, int x, int y, int w, int h)
{
    TableView *tv = calloc(sizeof(TableView), 1);
    tv->gap = 10;

    // Table Background
    struct talenode *node;
    node = nemotale_node_create_pixman(w, h);
    nemotale_node_set_id(node, 2);
    nemotale_attach_node(tale, node);
    nemotale_node_opaque(node, 0, 0, w, h);
    nemotale_node_translate(node,x, y);
    tv->node = node;

    struct nemopath *path;
    struct pathone *one;
    one = nemotale_path_create_path(NULL);
    path = _body_bg_create(w, h, 10);
    nemotale_path_use_path(one, path);
    nemotale_path_set_id(one, "tableview");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1, 1, 1, 0.3);
    nemotale_path_set_operator(one, CAIRO_OPERATOR_SOURCE);
    nemotale_path_update_one(one);
    nemotale_node_render_path(node, one);
    tv->bg = one;

    // Table Item
    node = nemotale_node_create_pixman(w - tv->gap * 2, h - tv->gap * 2);
    nemotale_node_set_id(node, 3);
    nemotale_attach_node(tale, node);
    nemotale_node_translate(node, x + tv->gap, y + tv->gap);
    tv->item_node = node;

    struct pathone *group;
    group = nemotale_path_create_group();
    tv->item_group = group;
    tv->items = NULL;

    return tv;
}

static double
_table_get_width(TableView *tv)
{
    return NTPATH_WIDTH(tv->bg) * NTPATH_TRANSFORM_SX(tv->bg);
}

static double
_table_get_height(TableView *tv)
{
    return NTPATH_HEIGHT(tv->bg) * NTPATH_TRANSFORM_SY(tv->bg);
}

static double
_table_get_content_height(TableView *tv)
{
    List *l;
    TableItem *ti;
    double maxh = 0;
    LIST_FOR_EACH(tv->items, l, ti) {
        double y;
        y = _table_item_get_y(ti) + _table_item_get_height(ti);
        if (maxh < y) maxh = y;
    }
    return maxh + tv->gap;
}

static void
_table_item_clicked(TableView *tv, struct Context *ctx, struct pathone *one)
{
    if (tv->state != TABLE_STATE_SCROLL) return;
    tv->state = TABLE_STATE_SELECTED;

    TableItem *ti = nemotale_path_get_data(one);
    struct nemocanvas *canvas = _win_get_canvas(ctx->win);

    struct taletransition *trans;
    trans = _transit_create(canvas, 0, 1000, NEMOEASE_CUBIC_OUT_TYPE);

    nemotale_path_detach_one(tv->item_group, one);
    nemotale_path_attach_one(tv->item_group, one);

    nemotale_path_transform_enable(one);
    _transit_transform_path(trans, one);
    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(one));
    nemotale_transition_attach_dattr(trans,
            NTPATH_TRANSFORM_ATSY(one), 1.0f, 2);

    double y = NTPATH_Y(one) + NTPATH_TRANSFORM_Y(one);

    List *l;
    LIST_FOR_EACH(tv->items, l, ti) {
        if (one == ti->one) continue;

        struct pathone *temp;
        temp = ti->one;
        nemotale_path_transform_enable(temp);
        _transit_transform_path(trans, temp);
        nemotale_transition_attach_signal(trans,
                NTPATH_DESTROY_SIGNAL(temp));
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(temp), 1.0f, y);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(temp)), 4.0f, 1.0f,
                ti->r, ti->g, ti->b, 0.0f);
    }

    nemotale_path_transform_enable(tv->bg);
    _transit_transform_path(trans, tv->bg);
    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(tv->bg));
    nemotale_transition_attach_dattr(trans,
            NTPATH_TRANSFORM_ATY(tv->bg), 1.0f, y);
    nemotale_transition_attach_dattr(trans,
            NTPATH_TRANSFORM_ATSY(tv->bg), 1.0f, 0.5);

    _transit_damage_path(trans, tv->item_node, tv->item_group);
    _transit_go(trans, canvas);
}

static void
_table_scroll(TableView *tv, struct Context *ctx, int pos_x, int pos_y)
{
    if (tv->state != TABLE_STATE_SCROLL) return;

    double th = _table_get_height(tv);
    double ch = _table_get_content_height(tv);

    if (ch < th) return;
    if (NTPATH_TRANSFORM_Y(tv->item_group) + pos_y >= 0) return;
    if ((ch > th) &&
        ((ch - th) <= -(NTPATH_TRANSFORM_Y(tv->item_group) + pos_y))) return;

    _win_render_prepare(ctx->win);

    nemotale_node_clear_path(tv->item_node);
    nemotale_node_damage_all(tv->item_node);

    nemotale_path_translate(tv->item_group,
            NTPATH_TRANSFORM_X(tv->item_group) + pos_x,
            NTPATH_TRANSFORM_Y(tv->item_group) + pos_y);

    nemotale_path_update_one(tv->item_group);
    nemotale_node_render_path(tv->item_node, tv->item_group);

    _win_render(ctx->win);
}

static TableItem *
_table_append(TableView *tv, const char *text)
{
    TableItem *ti = calloc(sizeof(TableItem), 1);
    double w = _table_get_width(tv) - tv->gap * 3;
    double h = 150;

    ti->r = (double)rand()/RAND_MAX;
    ti->g = (double)rand()/RAND_MAX;
    ti->b = (double)rand()/RAND_MAX;

    struct pathone *one;
    one = nemotale_path_create_rect(w, h);
    nemotale_path_set_rounded_rect(one, 1, 10);
    nemotale_path_set_id(one, "tableitem");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one),
            ti->r, ti->g, ti->b, 1.0);
    nemotale_path_set_data(one, ti);
    nemotale_path_attach_one(tv->item_group, one);
    nemotale_path_update_one(one);
    ti->one = one;
    ti->tv = tv;

    tv->items = list_data_insert(tv->items, ti);

    return ti;
}

static void
_table_arrange(TableView *tv, struct Context *ctx)
{
    TableItem *ti;
    List *l;
    int h = 0;
    _win_render_prepare(ctx->win);

    bool first = false;
    double gapw = tv->gap/2;
    LIST_FOR_EACH(tv->items, l, ti) {
        if (!first) {
            nemotale_path_translate(ti->one, gapw, tv->gap);
            h += _table_item_get_height(ti) + tv->gap * 2;
            first = true;
        } else {
            nemotale_path_translate(ti->one, gapw, h);
            h += _table_item_get_height(ti) + tv->gap;
        }
    }

    nemotale_path_update_one(tv->item_group);
    nemotale_node_damage_all(tv->item_node);
    nemotale_node_render_path(tv->item_node, tv->item_group);
    _win_render(ctx->win);
}

static void
_tale_event(struct nemotale *tale, struct talenode *node, uint32_t type, struct taleevent *event)
{
    struct Context *ctx = nemotale_get_userdata(tale);
    struct nemocanvas *canvas = _win_get_canvas(ctx->win);
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
                ctx->tv->item_group, event->x, event->y);
        if (ntaps == 1) {
            struct pathone *one = nemotale_path_pick_one(
                    ctx->title_bg, event->x, event->y);
            if (one && NTPATH_ID(one) &&
                    !strcmp(NTPATH_ID(one), "title")) {
                nemocanvas_move(canvas, taps[0]->serial);
            }
        } else if (ntaps == 2) {
            /*
            nemocanvas_pick(canvas,
                    taps[0]->serial,
                    taps[1]->serial,
                    (1 << NEMO_SURFACE_PICK_TYPE_SCALE));
                    */
        } else if (ntaps == 3) {
            if (nemotale_is_single_click(tale, event, type)) {
                nemotool_exit(tool);
            }
        }
    } else if (nemotale_is_single_click(tale, event, type)) {
        if (ntaps == 1) {
            struct pathone *one = tap->item;
            if (one) {
                _table_item_clicked(ctx->tv, ctx, one);
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
            //double pos_x = event->x - ctx->prev_event_x;
            double pos_y = event->y - ctx->prev_event_y;
            ctx->prev_event_x = event->x;
            ctx->prev_event_y = event->y;

            _table_scroll(ctx->tv, ctx, 0, pos_y);
        }
    }
}

int main()
{
    int w = 320, h = 640;
    struct Context *ctx = malloc(sizeof(struct Context));
    ctx->w = w;
    ctx->h = h;

    struct nemotool *tool;
    tool = nemotool_create();
    nemotool_connect_wayland(tool, NULL);

    _Win *win = _win_create(tool, WIN_TYPE_EGL, w, h, _tale_event);
    nemocanvas_set_anchor(_win_get_canvas(win), -0.5f, -0.5f);
    ctx->win = win;

    struct nemotale *tale;
    tale = _win_get_tale(win);
    nemotale_set_userdata(tale, ctx);

    struct talenode *node;
    struct pathone *group;
    struct pathone *one;

    // Main node
    node = nemotale_node_create_pixman(w, h);
    nemotale_node_set_id(node, 1);
    nemotale_attach_node(tale, node);
    nemotale_node_opaque(node, 0, 0, w, h);
    ctx->node = node;

    // Background
    one = nemotale_path_create_rect(w, h);
    nemotale_path_set_id(one, "bg");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1, 1, 1, 0.0);
    nemotale_path_set_operator(one, CAIRO_OPERATOR_SOURCE);
    nemotale_path_update_one(one);
    nemotale_node_render_path(node, one);
    ctx->bg = one;

    // Title
    struct nemopath *path;
    one = nemotale_path_create_path(NULL);
    path = _title_bg_create(200, 50, 10);
    nemotale_path_use_path(one, path);
    nemotale_path_set_id(one, "title");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1, 1, 1, 0.3);
    nemotale_path_update_one(one);
    nemotale_node_render_path(node, one);
    ctx->title_bg = one;

    // Body
    TableView *tv = _table_create(tale, 0, 50, w, h - 50);
    _table_append(tv, NULL);
    _table_append(tv, NULL);
    _table_append(tv, NULL);
    _table_append(tv, NULL);
    _table_append(tv, NULL);
    _table_arrange(tv, ctx);
    ctx->tv = tv;

    _win_render(ctx->win);

    nemotool_run(tool);

    nemotale_path_destroy_one(ctx->bg);
    nemotale_path_destroy_one(ctx->title_bg);
    nemotale_path_destroy_one(group);
    nemotale_node_destroy(ctx->node);

    _win_destroy(win);

    nemotool_disconnect_wayland(tool);
    nemotool_destroy(tool);

    return 0;
}
