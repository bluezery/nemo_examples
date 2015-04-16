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
#include "pieview.h"


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

    PieView **pv_cpus;

    char *str_name, *str_user, *str_nice, *str_system, *str_idle;
    struct pathone *txt_name;
    struct pathone *txt_user;
    struct pathone *txt_nice;
    struct pathone *txt_system;

    struct taletransition *trans;
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
_cpuview_update_end(struct taletransition *trans, struct nemoobject *obj)
{
    CpuView *cpuview = nemoobject_igetp(obj, 0);
    cpuview->trans = NULL;
}

static void
_cpuview_update(CpuView *cpuview)
{
    if (cpuview->trans) return;

    CpuStat *cur = cpuview->cur;
    CpuStat *prev = cpuview->prev;

    Context *ctx = cpuview->ctx;
    struct taletransition *trans;
    trans = _win_trans_create(ctx->win, 0, 500, NEMOEASE_CUBIC_OUT_TYPE);
    _win_trans_damage(ctx->win, trans, ctx->node, ctx->group);

    int i = 0;
    for (i = 0 ; i < cur->cnt ; i++) {
        int user_diff, nice_diff, system_diff, idle_diff, tot_diff;
        user_diff = cur->cpus[i].user - prev->cpus[i].user;
        nice_diff = cur->cpus[i].nice - prev->cpus[i].nice;
        system_diff = cur->cpus[i].system - prev->cpus[i].system;
        idle_diff = cur->cpus[i].idle - prev->cpus[i].idle;
        tot_diff = user_diff + nice_diff + system_diff + idle_diff;
        if (tot_diff == 0) continue;

        // Text Update (Main cpu usage)
        if (i == 0) {
            _win_trans_transform(ctx->win, trans, cpuview->txt_user);
            _win_trans_transform(ctx->win, trans, cpuview->txt_system);
            _win_trans_transform(ctx->win, trans, cpuview->txt_nice);

            if (strcmp(cpuview->cur->cpus[cpuview->state].name, cpuview->str_name)) {
                nemotale_path_load_text(cpuview->txt_name,
                        cpuview->cur->cpus[cpuview->state].name,
                        strlen(cpuview->cur->cpus[cpuview->state].name));
                free(cpuview->str_name);
                cpuview->str_name = strdup(cpuview->cur->cpus[cpuview->state].name);

                nemotale_transition_attach_dattrs(trans,
                        NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cpuview->txt_name)),
                        4, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f);
                nemotale_transition_attach_dattrs(trans,
                        NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cpuview->txt_name)),
                        4, 1.0f, 0.0f, 1.0f, 1.0f, 0.8f);
            }

            char percent[256];
            snprintf(percent, 255, "user %2d%%", (int)(((double)user_diff/tot_diff) * 100));
            if (strcmp(percent, cpuview->str_user)) {
                nemotale_path_load_text(cpuview->txt_user, percent, strlen(percent));
                free(cpuview->str_user);
                cpuview->str_user = strdup(percent);
                nemotale_transition_attach_dattrs(trans,
                        NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cpuview->txt_user)),
                        4, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f);
                nemotale_transition_attach_dattrs(trans,
                        NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cpuview->txt_user)),
                        4, 1.0f, 1.0f, 0.0f, 0.0f, 0.8f);
            }

            snprintf(percent, 255, "nice %2d%%", (int)(((double)nice_diff/tot_diff) * 100));
            if (strcmp(percent, cpuview->str_nice)) {
                nemotale_path_load_text(cpuview->txt_nice, percent, strlen(percent));
                free(cpuview->str_nice);
                cpuview->str_nice = strdup(percent);
                nemotale_transition_attach_dattrs(trans,
                        NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cpuview->txt_nice)),
                        4, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f);
                nemotale_transition_attach_dattrs(trans,
                        NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cpuview->txt_nice)),
                        4, 1.0f, 0.0f, 1.0f, 0.0f, 0.8f);
            }

            snprintf(percent, 255, "system %2d%%", (int)(((double)system_diff/tot_diff) * 100));
            if (strcmp(percent, cpuview->str_system)) {
                nemotale_path_load_text(cpuview->txt_system, percent, strlen(percent));
                free(cpuview->str_system);
                cpuview->str_system = strdup(percent);
                nemotale_transition_attach_dattrs(trans,
                        NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cpuview->txt_system)),
                        4, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f);
                nemotale_transition_attach_dattrs(trans,
                        NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cpuview->txt_system)),
                        4, 1.0f, 0.0f, 0.0f, 1.0f, 0.8f);
            }
        }

        if (cpuview->state == 0) {
            _pieview_change_power(cpuview->pv_cpus[i], 0, user_diff);
            _pieview_change_power(cpuview->pv_cpus[i], 1, nice_diff);
            _pieview_change_power(cpuview->pv_cpus[i], 2, system_diff);
            _pieview_change_power(cpuview->pv_cpus[i], 3, idle_diff);
            _pieview_update(cpuview->pv_cpus[i], ctx->win, trans);
        }
    }

    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_END,
            _cpuview_update_end, "p", cpuview);
    _win_trans_render(ctx->win, trans, ctx->node, ctx->group);
    _win_trans_do(ctx->win, trans);
    cpuview->trans = trans;
}

static void
_cpuview_change(CpuView *cpuview)
{
    cpuview->state++;
    if (cpuview->state >= cpuview->cnt)
        cpuview->state = 0;

    Context *ctx = cpuview->ctx;
    _pieview_resize(cpuview->pv_cpus[0], (ctx->w-20)/2, (ctx->w-20)/2 - 20);
    _cpuview_update(cpuview);
}

static CpuView *
_cpuview_create(Context *ctx, int r, int w, int offset)
{
    CpuView *cpuview;

    cpuview = calloc(sizeof(CpuView), 1);
    cpuview->ctx = ctx;


    CpuStat *cur = _cpustat_get();
    cpuview->cur = cur;

    cpuview->cnt = cur->cnt;
    cpuview->state = 0;

    // Main Cpu view
    cpuview->pv_cpus = calloc(sizeof(PieView *), (cur->cnt - 1));

    struct taletransition *trans;
    trans = _win_trans_create(ctx->win, 0, 1000, NEMOEASE_CUBIC_OUT_TYPE);
    _win_trans_damage(ctx->win, trans, ctx->node, ctx->group);

    int i = 0;
    for (i = 0 ; i < cur->cnt ; i++) {
        PieView *temp = NULL;
        int tot;
        tot = cur->cpus[i].user + cur->cpus[i].nice + cur->cpus[i].system + cur->cpus[i].idle;
        if (tot == 0) continue;
        temp = _pieview_create(ctx->group,
                r + ((w + offset) * i), r + ((w + offset)* i) - w);
        _pieview_set_data(temp, ctx);
        _pieview_add_ap(temp, cur->cpus[i].user);
        _pieview_set_color(temp, 0, 1, 0, 0, 0.8);
        _pieview_add_ap(temp, cur->cpus[i].nice);
        _pieview_set_color(temp, 1, 0, 1, 0, 0.8);
        _pieview_add_ap(temp, cur->cpus[i].system);
        _pieview_set_color(temp, 2, 0, 0, 1, 0.8);
        _pieview_add_ap(temp, cur->cpus[i].idle);
        _pieview_set_color(temp, 3, 1, 1, 1, 0.2);
        _pieview_update(temp, ctx->win, trans);
        _pieview_move(temp, -(w + offset) * i + w, -(w + offset) * i + w);
        cpuview->pv_cpus[i] = temp;
    }

    _win_trans_render(ctx->win, trans, ctx->node, ctx->group);
    _win_trans_do(ctx->win, trans);

    return cpuview;
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
                    (1 << NEMO_SURFACE_PICK_TYPE_ROTATE));
        }
    } else {
        if (type & NEMOTALE_UP_EVENT) {
        }
        if (nemotale_is_single_click(tale, event, type)) {
            if (ntaps == 1) {
                struct pathone *one = tap->item;
                if (one && NTPATH_ID(one)) {
                    if (!strcmp(NTPATH_ID(one), "bg") ||
                        !strcmp(NTPATH_ID(one), "txt_name"))
                        _cpuview_change(cpuview);
                }
            } else if (ntaps == 3) {
                if (nemotale_is_single_click(tale, event, type)) {
                    nemotool_exit(tool);
                }
            }
        }
    }
}

int main(){
    int w = 500, h = 500;
    Context *ctx = calloc(sizeof(Context), 1);
    ctx->w = w;
    ctx->h = h;

    struct nemotool *tool;
    tool = nemotool_create();
    nemotool_connect_wayland(tool, NULL);

    _Win *win = _win_create(tool, WIN_TYPE_PIXMAN, w, h, _tale_event);
    ctx->win = win;
    struct nemotale *tale = _win_get_tale(win);
    struct nemocanvas *canvas = _win_get_canvas(win);
    nemocanvas_set_layer(canvas, NEMO_SURFACE_LAYER_TYPE_OVERLAY);
    nemocanvas_set_anchor(canvas, -0.5f, -0.5f);
    nemotale_set_userdata(tale, ctx);

    struct talenode *node;
    node = nemotale_node_create_pixman(w, h);
    nemotale_node_set_id(node, 1);
    nemotale_attach_node(tale, node);
    nemotale_node_opaque(node, 0, 0, w, h);
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
            0.5f, 0.5f, 0.5f, 1.0f);
    nemotale_path_attach_one(group, one);

    // CpuView
    int offset = 1;
    int pw = 20;
    int pr = w/2 - pw;
    CpuView *cpuview = _cpuview_create(ctx, pr, pw, offset);
    ctx->cpuview = cpuview;

    // CpuView: Pieview Text
    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "txt_name");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 50);
    nemotale_path_set_font_weight(NTPATH_STYLE(one),
            CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 1, 1, 0.8);
    nemotale_path_load_text(one, "cpu  ", strlen("cpu  "));
    nemotale_path_translate(one, w/2, h/2 - 50);
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    cpuview->txt_name = one;
    cpuview->str_name = strdup("cpu");

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "txt_user");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
    nemotale_path_set_font_weight(NTPATH_STYLE(one),
            CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1, 0, 0, 0.8);
    nemotale_path_load_text(one, "user 00%", strlen("user 00%"));
    nemotale_path_translate(one, w/2, h/2);
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    cpuview->txt_user = one;
    cpuview->str_user = strdup("user 00%");

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "txt_system");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
    nemotale_path_set_font_weight(NTPATH_STYLE(one),
            CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 0, 1, 0.8);
    nemotale_path_load_text(one, "system 00%", strlen("system 00%"));
    nemotale_path_translate(one, w/2, h/2 + 25);
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    cpuview->txt_system = one;
    cpuview->str_system = strdup("system 00%");

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "txt_nice");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
    nemotale_path_set_font_weight(NTPATH_STYLE(one),
            CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 1, 0, 0.8);
    nemotale_path_load_text(one, "nice 00%", strlen("nice 00%"));
    nemotale_path_translate(one, w/2, h/2 + 50);
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    cpuview->txt_nice = one;
    cpuview->str_nice = strdup("nice 00%");

    // Cpuview Update timer
    struct nemotimer *timer = nemotimer_create(tool);
    _cpuview_timeout(timer, cpuview);

	nemotale_path_update_one(group);
	nemotale_node_render_path(node, group);
	nemotale_composite(tale, NULL);

    nemocanvas_set_dispatch_frame(canvas, nemoclip_dispatch_canvas_frame);

    nemotool_run(tool);

    nemotimer_destroy(timer);
    _pieview_destroy(cpuview->pv_cpus[0]);

    nemotale_path_destroy_one(group);
    nemotale_node_destroy(node);

    _win_destroy(win);
    nemotool_disconnect_wayland(tool);
    nemotool_destroy(tool);
    free(ctx);

    return 0;
}
