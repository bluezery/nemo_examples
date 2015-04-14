#include <stdlib.h>

#include <nemotool.h>
#include <nemocanvas.h>
#include <nemotale.h>
#include <talenode.h>
#include <talepixman.h>
#include <nemotimer.h>

#include <math.h>
#include <pathstyle.h>
#include "talehelper.h"

#include "util.h"
#include "nemohelper.h"

struct Pie {
    int power;
    struct pathone *one;
    double start, end;
    double r, g, b, a;
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

typedef struct _Cpu {
    char *name;
    int user;
    int nice;
    int system;
    int idle;
} Cpu;

typedef struct _CpuStat {
    int cnt;
    Cpu *cpus;
} CpuStat;

#define CPUVIEW_TIMEOUT 1000

typedef struct _Context Context;
typedef struct _CpuView CpuView;

struct _CpuView {
    Context *ctx;
    CpuStat *prev, *cur;

    int state;
    int cnt;

    struct PieView *pieview;
    struct pathone *txt;

};

struct _Context {
    _Win *win;

    struct talenode *node;
    struct pathone *group;
    struct pathone *bg;

    int w, h;
    CpuView *cpuview;
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
    Context *ctx = _pieview_get_data(pieview);
    struct taletransition *trans;

    trans = _win_trans_create(ctx->win, 0, 1200, NEMOEASE_CUBIC_OUT_TYPE);

    _win_trans_damage(ctx->win, trans, ctx->node, ctx->group);
    _win_trans_transform(ctx->win, trans, pieview->loading_one);
    _win_trans_render(ctx->win, trans, ctx->node, ctx->group);

    _win_trans_add_event_end(ctx->win, trans, _pieview_reloading, ctx, pieview);

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

    _win_trans_do(ctx->win, trans);
}

static struct PieView *
_pieview_create(struct pathone *group, int r, int ir)
{
    struct PieView *pieview;
    pieview = calloc(sizeof(struct PieView), 1);
    pieview->r = r;
    pieview->ir = ir;
    pieview->group = nemotale_path_create_group();
    nemotale_path_attach_one(group, pieview->group);
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
_pieview_get_ap(struct PieView *pieview, int idx)
{
    return list_idx_get_data(pieview->pies, idx);
}

static void
_pieview_change_power(struct PieView *pieview, int idx, int pow)
{
    struct Pie *pie = list_idx_get_data(pieview->pies, idx);
    if (!pie) return;
    pie->power = pow;
}

static void
_pieview_set_color(struct PieView *pieview, int idx, double r, double g, double b, double a)
{
    struct Pie *pie = list_idx_get_data(pieview->pies, idx);
    if (!pie) return;
    pie->r = r;
    pie->g = g;
    pie->b = b;
    pie->a = a;
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
    Context *ctx = _pieview_get_data(pieview);
    struct nemocanvas *canvas = _win_get_canvas(ctx->win);
    struct nemotale *tale = _win_get_tale(ctx->win);
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
            1, 1, 1, 0.5);
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

    Context *ctx = _pieview_get_data(pieview);
    struct nemocanvas *canvas = _win_get_canvas(ctx->win);
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

    Context *ctx = _pieview_get_data(pieview);
    int pow;

    List *l;
    struct Pie *pie;


    pow = _pieview_get_power(pieview);

    struct taletransition *trans;
    trans = _win_trans_create(ctx->win, 0, 1000, NEMOEASE_CUBIC_OUT_TYPE);

    _win_trans_damage(ctx->win, trans, ctx->node, ctx->group);

    double start = 0;
    LIST_FOR_EACH_REVERSE(pieview->pies, l, pie) {
        double end = ((double)pie->power/pow) * M_PI * 2 + start;

        nemotale_transition_attach_dattr(trans,
                NTPATH_CIRCLE_ATSTARTANGLE(pie->one), 1.0f, start);
        nemotale_transition_attach_dattr(trans,
                NTPATH_CIRCLE_ATENDANGLE(pie->one), 1.0f, end);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(pie->one)),
                4, 1.0f, pie->r, pie->g, pie->b, pie->a);
        _win_trans_transform(ctx->win, trans, pie->one);

        start = end;
    }
    _win_trans_render(ctx->win, trans, ctx->node, ctx->group);

    // Remove animation
    LIST_FOR_EACH_REVERSE(pieview->pies_del, l, pie) {
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(pie->one)),
                4, 1.0f, pie->r, pie->g, pie->b, 0.0f);
        _transit_add_event_end(trans, _pieview_update_end, ctx, pieview);
    }
    _win_trans_do(ctx->win, trans);

    pieview->update_trans = trans;
}

#if 0
static void
_timeout(struct nemotimer *timer, void *data)
{
    Context *ctx = data;
    struct nemocanvas *canvas = _win_get_canvas(ctx->win);
    struct nemotale *tale = nemocanvas_get_userdata(canvas);
    struct PieView *pieview = ctx->pieview;

    nemotale_handle_canvas_update_event(NULL, canvas, tale);
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

    nemotimer_set_timeout(timer, 2000);
}
#endif

static void
_cpustat_free(CpuStat *stat)
{
    free(stat->cpus);
    free(stat);
}

static CpuStat *
_cpustat_dup(CpuStat *stat)
{
    CpuStat *new;
    new = calloc(sizeof(CpuStat), 1);
    new->cnt = stat->cnt;

    int i = 0;
    new->cpus = calloc(sizeof(Cpu), stat->cnt);
    for ( i = 0 ; i < stat->cnt ; i++) {
        new->cpus[i] = stat->cpus[i];
        if (stat->cpus[i].name)
            new->cpus[i].name = strdup(stat->cpus[i].name);
    }
    return new;
}

static CpuStat *
_cpustat_get()
{
    int line_len;
    char **lines = _file_load("/proc/stat", &line_len);
    int i = 0;

    CpuStat *stat = calloc(sizeof(CpuStat), 1);
    for (i = 0 ; i < line_len ; i++) {
        if (!strncmp(lines[i], "cpu", 3)) {
            stat->cnt++;
            stat->cpus = realloc(stat->cpus, sizeof(Cpu) * stat->cnt);

            char *token;
            token = strtok(lines[i], " ");
            stat->cpus[stat->cnt - 1].name = strdup(token);
            token = strtok(NULL, " ");
            stat->cpus[stat->cnt - 1].user = atoi(token);
            token = strtok(NULL, " ");
            stat->cpus[stat->cnt - 1].nice = atoi(token);
            token = strtok(NULL, " ");
            stat->cpus[stat->cnt - 1].system = atoi(token);
            token = strtok(NULL, " ");
            stat->cpus[stat->cnt - 1].idle = atoi(token);
        }
        free(lines[i]);
    }
    free(lines);
    return stat;
}

static void
_cpuview_change(CpuView *cpuview)
{
    cpuview->state++;
    if (cpuview->state > cpuview->cnt)
        cpuview->state = 0;
    if (cpuview->state == cpuview->cnt)
        nemotale_path_load_text(cpuview->txt, "Total", 5);
    else
        nemotale_path_load_text(cpuview->txt,
                cpuview->cur->cpus[cpuview->state].name,
                strlen(cpuview->cur->cpus[cpuview->state].name));
}

static void
_cpuview_update(CpuView *cpuview)
{
    struct PieView *pieview = cpuview->pieview;
    CpuStat *cur = cpuview->cur;
    CpuStat *prev = cpuview->prev;

    int i = 0;
    int user_diff_t = 0, nice_diff_t = 0, system_diff_t = 0;
    int idle_diff_t = 0, tot_diff_t = 0;
    for (i = 0 ; i < prev->cnt ; i++) {
        int user_diff, nice_diff, system_diff, idle_diff, tot_diff;
        user_diff = cur->cpus[i].user - prev->cpus[i].user;
        nice_diff = cur->cpus[i].nice - prev->cpus[i].nice;
        system_diff = cur->cpus[i].system - prev->cpus[i].system;
        idle_diff = cur->cpus[i].idle - prev->cpus[i].idle;
        tot_diff = user_diff + nice_diff + system_diff + idle_diff;
        if (tot_diff == 0) continue;
        user_diff_t += user_diff;
        nice_diff_t += nice_diff;
        system_diff_t += system_diff;
        idle_diff_t += idle_diff;
        tot_diff_t += tot_diff;
        if (cpuview->state == i) {
            ERR("%d", i);
            _pieview_change_power(pieview, 0, user_diff);
            _pieview_change_power(pieview, 1, nice_diff);
            _pieview_change_power(pieview, 2, system_diff);
            _pieview_change_power(pieview, 3, idle_diff);
        }
    }
    if (cpuview->state >= i) {
        //ERR("");
        _pieview_change_power(pieview, 0, user_diff_t);
        _pieview_change_power(pieview, 1, nice_diff_t);
        _pieview_change_power(pieview, 2, system_diff_t);
        _pieview_change_power(pieview, 3, idle_diff_t);
    }
    _pieview_update(pieview);
}

static void
_cpuview_init(CpuView *cpuview)
{
    struct PieView *pieview = cpuview->pieview;
    CpuStat *cur = _cpustat_get();
    cpuview->cur = cur;

    int i = 0;
    int user_t = 0, nice_t = 0, system_t = 0;
    int idle_t = 0, tot_t = 0;
    for (i = 0 ; i < cur->cnt ; i++) {
        int tot;
        tot = cur->cpus[i].user + cur->cpus[i].nice + cur->cpus[i].system + cur->cpus[i].idle;
        if (tot == 0) continue;
        user_t += cur->cpus[i].user;
        nice_t += cur->cpus[i].nice;
        system_t += cur->cpus[i].system;
        idle_t += cur->cpus[i].idle;
        tot_t += tot;
    }
    cpuview->cnt = cur->cnt;
    cpuview->state = -1;

    _pieview_add_ap(pieview, user_t);
    _pieview_set_color(pieview, 0, 1, 0, 0, 0.8);
    _pieview_add_ap(pieview, nice_t);
    _pieview_set_color(pieview, 1, 0, 1, 0, 0.8);
    _pieview_add_ap(pieview, system_t);
    _pieview_set_color(pieview, 2, 0, 0, 1, 0.8);
    _pieview_add_ap(pieview, idle_t);
    _pieview_set_color(pieview, 3, 1, 1, 1, 0.2);
    _pieview_update(pieview);
}

static void
_cpuview_timeout(struct nemotimer *timer, void *data)
{
    CpuView *cpuview = data;

    if (cpuview->prev) _cpustat_free(cpuview->prev);
    cpuview->prev = _cpustat_dup(cpuview->cur);

    _cpustat_free(cpuview->cur);
    cpuview->cur = _cpustat_get();

    _cpuview_update(cpuview);

    nemotimer_set_callback(timer, _cpuview_timeout);
    nemotimer_set_userdata(timer, data);
    nemotimer_set_timeout(timer, CPUVIEW_TIMEOUT);
}

static void
_tale_event(struct nemotale *tale, struct talenode *node, uint32_t type, struct taleevent *event)
{
    Context *ctx = nemotale_get_userdata(tale);
    struct nemocanvas *canvas = _win_get_canvas(ctx->win);
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    CpuView *cpuview = ctx->cpuview;
    struct PieView *pieview = cpuview->pieview;

    struct taletap *taps[16];
    int ntaps;
    struct taletap *tap;

    ntaps = nemotale_get_node_taps(tale, node, taps, type);
    tap = nemotale_get_tap(tale, event->device, type);

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
    } else {
        if (nemotale_is_single_click(tale, event, type)) {
            if (ntaps == 1) {
                struct pathone *one = tap->item;
                if (one && NTPATH_ID(one)) {
                    if (!strcmp(NTPATH_ID(one), "bg") ||
                        !strcmp(NTPATH_ID(one), "text"))
                        _cpuview_change(cpuview);
                }
            }
        }
    }
}

int main()
{
    int w = 320, h = 320;
    Context *ctx = calloc(sizeof(Context), 1);
    ctx->w = w;
    ctx->h = h;

    struct nemotool *tool;
    tool = nemotool_create();
    nemotool_connect_wayland(tool, NULL);

    _Win *win = _win_create(tool, WIN_TYPE_PIXMAN, w, h, _tale_event);
    ctx->win = win;
    struct nemotale *tale = _win_get_tale(win);
    nemocanvas_set_anchor(_win_get_canvas(win), -0.5f, -0.5f);
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

    // Background
    one = nemotale_path_create_rect(w, h);
    nemotale_path_set_id(one, "bg");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_operator(one, CAIRO_OPERATOR_SOURCE);
    nemotale_path_set_fill_color(NTPATH_STYLE(one),
            0.0f, 0.0f, 0.0f, 0.0f);
    nemotale_path_attach_one(group, one);
    ctx->bg = one;

    // CpuView
    CpuView *cpuview = calloc(sizeof(CpuView), 1);
    ctx->cpuview = cpuview;

    cpuview->ctx = ctx;

    // CpuView: PieView
    struct PieView *pieview;
    pieview = _pieview_create(group, (w-20)/2, 80);
    _pieview_set_data(pieview, ctx);
    cpuview->pieview = pieview;

    // CpuView: Pieview Text
    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "text");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 50);
    nemotale_path_set_font_weight(NTPATH_STYLE(one),
            CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 1, 1, 1);
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_load_text(one, "Total", strlen("Total"));
    nemotale_path_translate(one, w/2, h/2);
    nemotale_path_attach_one(group, one);
    cpuview->txt = one;

    // CpuView init
    _cpuview_init(cpuview);

    // Cpuview Update timer
    struct nemotimer *timer = nemotimer_create(tool);
    _cpuview_timeout(timer, cpuview);

#if 0 // Pieview loading animation
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
#endif
    nemotool_run(tool);

    nemotimer_destroy(timer);
    _pieview_destroy(cpuview->pieview);

    nemotale_path_destroy_one(group);
    nemotale_node_destroy(node);

    _win_destroy(win);
    nemotool_disconnect_wayland(tool);
    nemotool_destroy(tool);
    free(ctx);

    return 0;
}
