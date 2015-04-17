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

#define IDLE_ALPHA 0.3
#define IDLE_COLOR 0.251, 0.490, 0.524 // dark blue

#define ALPHA 0.8
#define USER_COLOR 0.993, 0.673, 0.115  // amber
#define SYSTEM_COLOR 0.12, 0.165, 0.577 // INDIGO
#define NICE_COLOR  0.144, 0.539, 0.160 //  Green

/******************/
/**** Cpu Stat ****/
/******************/
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

#define CPUVIEW_TIMEOUT 3000
#define TRANS_DURATION 1000

typedef struct _Context Context;
typedef struct _CpuView CpuView;

struct _CpuView {
    int r, w, inoffset;
    Context *ctx;
    CpuStat *prev, *cur;

    bool state_changed;
    int state;

    int cnt;
    PieView **pieviews;

    char *str_user, *str_nice, *str_system, *str_idle;
    struct pathone *group;
    struct pathone *txt_name;
    struct pathone *txt_user;
    struct pathone *txt_nice;
    struct pathone *txt_system;

    struct taletransition *trans, *text_trans;
    struct nemotimer *timer;
};

/******************/
/**** Cpu Stat ****/
/******************/
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

/****************************/
/******** Mem Stat *********/
/****************************/
typedef struct _MemStat {
    int phy_total;
    int phy_used;
    int phy_free;
    int swap_total;
    int swap_used;
    int swap_free;
} MemStat;

static const char *PHY_MEM = "Phy. Mem.";
static const char *SWAP_MEM = "Swap. Mem.";

static void
_memstat_free(MemStat *stat)
{
    free(stat);
}

static MemStat *
_memstat_get()
{
    int line_len;
    char **lines = _file_load("/proc/meminfo", &line_len);
    int i = 0;

    MemStat *stat = calloc(sizeof(MemStat), 1);
    for (i = 0 ; i < line_len ; i++) {
        if (!strncmp(lines[i], "MemTotal:", 9)) {
            char *token;
            token = strtok(lines[i], " ");
            token = strtok(NULL, " ");
            stat->phy_total = atoi(token);
        }
        if (!strncmp(lines[i], "MemFree:", 8)) {
            char *token;
            token = strtok(lines[i], " ");
            token = strtok(NULL, " ");
            stat->phy_free = atoi(token);
        }
        if (!strncmp(lines[i], "SwapTotal:", 10)) {
            char *token;
            token = strtok(lines[i], " ");
            token = strtok(NULL, " ");
            stat->swap_total = atoi(token);
        }
        if (!strncmp(lines[i], "SwapFree:", 9)) {
            char *token;
            token = strtok(lines[i], " ");
            token = strtok(NULL, " ");
            stat->swap_free = atoi(token);
        }
        free(lines[i]);
    }
    free(lines);

    stat->phy_used = stat->phy_total - stat->phy_free;
    stat->swap_used = stat->swap_total - stat->swap_free;

    return stat;
}

struct _Context {
    _Win *win;

    struct talenode *node;
    struct pathone *group;
    struct pathone *bg;

    int w, h;
    CpuView *cv;
};

/*******************/
/**** Cpu View ****/
/*******************/

static void
_cpuview_destroy(CpuView *cv)
{

    if (cv->prev) _cpustat_free(cv->prev);
    if (cv->cur) _cpustat_free(cv->cur);
    if (cv->trans) nemotale_transition_destroy(cv->trans);
    if (cv->text_trans) nemotale_transition_destroy(cv->trans);
    if (cv->timer) nemotimer_destroy(cv->timer);

    if (cv->cur) {
        int i = 0;
        for (i = 0 ; i < cv->cur->cnt ; i++) {
            _pieview_destroy(cv->pieviews[i]);
        }
    }
    free(cv->pieviews);

    free(cv->str_user);
    free(cv->str_nice);
    free(cv->str_system);
    free(cv->str_idle);
    nemotale_path_destroy_one(cv->txt_name);
    nemotale_path_destroy_one(cv->txt_user);
    nemotale_path_destroy_one(cv->txt_system);
    nemotale_path_destroy_one(cv->txt_nice);
    nemotale_path_destroy_one(cv->group);
}

static void
_cpuview_update_end(struct taletransition *trans, struct nemoobject *obj)
{
    CpuView *cv = nemoobject_igetp(obj, 0);
    cv->trans = NULL;
}

static void
_cpuview_text_update_end(struct taletransition *trans, struct nemoobject *obj)
{
    CpuView *cv = nemoobject_igetp(obj, 0);
    cv->text_trans = NULL;
}

static void
_cpuview_update(CpuView *cv)
{
    if (cv->trans) return;

    CpuStat *cur = cv->cur;
    CpuStat *prev = cv->prev;
    if (!cur || !prev) return;

    Context *ctx = cv->ctx;
    struct taletransition *trans;
    trans = nemotale_transition_create(0, TRANS_DURATION);
    nemotale_transition_attach_timing(trans, 1.0f, NEMOEASE_CUBIC_OUT_TYPE);
    _nemotale_transition_damage(trans, ctx->node, ctx->group);

    int i = 0;
    for (i = 0 ; i < cur->cnt ; i++) {
        double offset = (double)1.0/cur->cnt;

        int user_diff, nice_diff, system_diff, idle_diff, tot_diff;
        user_diff = cur->cpus[i].user - prev->cpus[i].user;
        nice_diff = cur->cpus[i].nice - prev->cpus[i].nice;
        system_diff = cur->cpus[i].system - prev->cpus[i].system;
        idle_diff = cur->cpus[i].idle - prev->cpus[i].idle;
        tot_diff = user_diff + nice_diff + system_diff + idle_diff;

        // Text Update (Total cpu usage)
        if (i == 0) {
            if (tot_diff == 0 || cv->text_trans) continue;

            struct taletransition *text_trans;
            text_trans = nemotale_transition_create(0, TRANS_DURATION);
            nemotale_transition_attach_timing(text_trans, 1.0f, NEMOEASE_CUBIC_OUT_TYPE);
            _nemotale_transition_damage(text_trans, ctx->node, ctx->group);

            char percent[256];
            snprintf(percent, 255, "user %2d%%",
                    (int)(((double)user_diff/tot_diff) * 100));
            if (strcmp(percent, cv->str_user)) {
                nemotale_path_load_text(cv->txt_user, percent,
                        strlen(percent));
                free(cv->str_user);
                cv->str_user = strdup(percent);
                nemotale_transition_attach_dattrs(text_trans,
                        NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cv->txt_user)),
                        4, 0.5f, 1.0f, 1.0f, 0.0f, 0.0f);
                nemotale_transition_attach_dattrs(text_trans,
                        NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cv->txt_user)),
                        4, 1.0f, 1.0f, 1.0f, 0.0f, 0.8f);
            }

            snprintf(percent, 255, "nice %2d%%",
                    (int)(((double)nice_diff/tot_diff) * 100));
            if (strcmp(percent, cv->str_nice)) {
                nemotale_path_load_text(cv->txt_nice, percent,
                        strlen(percent));
                free(cv->str_nice);
                cv->str_nice = strdup(percent);
                nemotale_transition_attach_dattrs(text_trans,
                        NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cv->txt_nice)),
                        4, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f);
                nemotale_transition_attach_dattrs(text_trans,
                        NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cv->txt_nice)),
                        4, 1.0f, 0.0f, 1.0f, 0.0f, 0.8f);
            }

            snprintf(percent, 255, "system %2d%%",
                    (int)(((double)system_diff/tot_diff) * 100));
            if (strcmp(percent, cv->str_system)) {
                nemotale_path_load_text(cv->txt_system, percent,
                        strlen(percent));
                free(cv->str_system);
                cv->str_system = strdup(percent);
                nemotale_transition_attach_dattrs(text_trans,
                        NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cv->txt_system)),
                        4, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f);
                nemotale_transition_attach_dattrs(text_trans,
                        NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cv->txt_system)),
                        4, 1.0f, 0.0f, 0.0f, 1.0f, 0.8f);
            }
            nemotale_transition_attach_event(text_trans,
                    NEMOTALE_TRANSITION_EVENT_END,
                    _cpuview_text_update_end, "p", cv);
            _nemotale_transition_render(text_trans, ctx->node, ctx->group);
            _win_trans_do(ctx->win, text_trans);
        }

        PieView *pv = cv->pieviews[i];
        _pieview_change_power(pv, 0, user_diff);
        _pieview_change_power(pv, 1, nice_diff);
        _pieview_change_power(pv, 2, system_diff);
        _pieview_change_power(pv, 3, idle_diff);

        double delay, to;
        int radius, inradius;

        if (cv->state_changed) {
            if (cv->state == 1) {
                radius = cv->r + ((cv->w + cv->inoffset) * i);
                inradius = radius - cv->w;
                if (i != 0) {
                    _pieview_set_color(pv, 0, USER_COLOR, ALPHA);
                    _pieview_set_color(pv, 1, NICE_COLOR, ALPHA);
                    _pieview_set_color(pv, 2, SYSTEM_COLOR, ALPHA);
                    _pieview_set_color(pv, 3, IDLE_COLOR, IDLE_ALPHA);
                }
                _pieview_resize(pv, radius, inradius);
                delay = offset * i;
                to = delay + offset;
            } else {
                if (i == 0) {
                    radius = cv->r + (cv->w + cv->inoffset) * (cur->cnt - 1);
                    inradius = cv->r - cv->w;
                } else {
                    radius = cv->r + ((cv->w + cv->inoffset) * (i -1));
                    inradius = radius - cv->w;
                    _pieview_set_color(pv, 0, USER_COLOR, 0);
                    _pieview_set_color(pv, 1, NICE_COLOR, 0);
                    _pieview_set_color(pv, 2, SYSTEM_COLOR, 0);
                    _pieview_set_color(pv, 3, IDLE_COLOR, 0);
                }
                _pieview_resize(pv, radius, inradius);
                delay = offset * (cur->cnt - i - 1);
                to = delay + offset;
            }
        }

        _pieview_update(pv, ctx->win, trans, delay, to);
    }

    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_END,
            _cpuview_update_end, "p", cv);
    _nemotale_transition_render(trans, ctx->node, ctx->group);
    _win_trans_do(ctx->win, trans);

    cv->trans = trans;
}

static void
_cpuview_change(CpuView *cv)
{
    cv->state++;
    if (cv->state >= 2)
        cv->state = 0;
    cv->state_changed = true;

    if (cv->trans) {
        nemotale_transition_revoke(cv->trans);
        cv->trans = NULL;
    }
    nemotimer_set_timeout(cv->timer, 16);
}

static void
_cpuview_move(CpuView *cv, int x, int y)
{
    int i = 0;
    CpuStat *cur = cv->cur;
    for (i = 0 ; i < cur->cnt ; i++) {
        _pieview_move(cv->pieviews[i], x, y);
    }
    nemotale_path_translate(cv->txt_name, x, y - 50);
    nemotale_path_translate(cv->txt_user, x, y - 10);
    nemotale_path_translate(cv->txt_system, x, y + 15);
    nemotale_path_translate(cv->txt_nice, x, y + 40);
}

static void
_cpuview_timeout(struct nemotimer *timer, void *data)
{
    CpuView *cv = data;

    if (cv->prev) _cpustat_free(cv->prev);
    cv->prev = _cpustat_dup(cv->cur);

    _cpustat_free(cv->cur);
    cv->cur = _cpustat_get();

    _cpuview_update(cv);

    nemotimer_set_timeout(timer, CPUVIEW_TIMEOUT);
}

static CpuView *
_cpuview_create(Context *ctx, int r, int w, int offset, int inoffset)
{
    CpuView *cv;

    cv = calloc(sizeof(CpuView), 1);
    cv->ctx = ctx;

    CpuStat *cur = _cpustat_get();
    cv->cur = cur;

    cv->cnt = cur->cnt;
    cv->state = 0;
    cv->r = r;
    cv->w = w;
    cv->inoffset = inoffset;

    struct pathone *one;
    struct pathone *group;
    group = nemotale_path_create_group();
    nemotale_path_attach_one(ctx->group, group);
    cv->group = group;

    // CpuView: Pieview Text
    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "txt_name");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
    nemotale_path_set_font_weight(NTPATH_STYLE(one),
            CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), IDLE_COLOR, 1.0);
    nemotale_path_load_text(one, "CPU Total", strlen("Cpu Total"));
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    nemotale_path_translate(one, 0, - 50);
    cv->txt_name = one;

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "txt_user");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
    nemotale_path_set_font_weight(NTPATH_STYLE(one),
            CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), USER_COLOR, 1.0);
    nemotale_path_load_text(one, "user  0%", strlen("user  0%"));
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    nemotale_path_translate(one, 0, - 10);
    cv->txt_user = one;
    cv->str_user = strdup("user  0%");

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "txt_system");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
    nemotale_path_set_font_weight(NTPATH_STYLE(one),
            CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), SYSTEM_COLOR, 1.0);
    nemotale_path_load_text(one, "system  0%", strlen("system  0%"));
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    nemotale_path_translate(one, 0, 15);
    cv->txt_system = one;
    cv->str_system = strdup("system  0%");

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "txt_nice");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
    nemotale_path_set_font_weight(NTPATH_STYLE(one),
            CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), NICE_COLOR, 1.0);
    nemotale_path_load_text(one, "nice  0%", strlen("nice  0%"));
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    nemotale_path_translate(one, 0, 40);
    cv->txt_nice = one;
    cv->str_nice = strdup("nice  0%");

    cv->pieviews = calloc(sizeof(PieView *), (cur->cnt - 1));

    struct taletransition *trans;
    trans = nemotale_transition_create(0, TRANS_DURATION);
    nemotale_transition_attach_timing(trans, 1.0f, NEMOEASE_CUBIC_OUT_TYPE);
    _nemotale_transition_damage(trans, ctx->node, ctx->group);

    int i = 0;
    for (i = 0 ; i < cur->cnt ; i++) {
        PieView *temp = NULL;
        int tot;
        tot = cur->cpus[i].user + cur->cpus[i].nice + cur->cpus[i].system + cur->cpus[i].idle;
        if (tot == 0) continue;

        double alpha, idle_alpha;
        int radius, inradius;
        if (i == 0) {
            radius = r;
            inradius = r - w;
            alpha = ALPHA;
            idle_alpha = IDLE_ALPHA;
        } else {
            radius = r + ((w + inoffset) * (i -1));
            inradius = radius - w;
            alpha = idle_alpha = 0;
        }

        temp = _pieview_create(group, radius, inradius);
        _pieview_set_data(temp, ctx);
        _pieview_add_ap(temp, cur->cpus[i].user);
        _pieview_add_ap(temp, cur->cpus[i].nice);
        _pieview_add_ap(temp, cur->cpus[i].system);
        _pieview_add_ap(temp, cur->cpus[i].idle);
        _pieview_set_color(temp, 0, USER_COLOR, alpha);
        _pieview_set_color(temp, 1, NICE_COLOR, alpha);
        _pieview_set_color(temp, 2, SYSTEM_COLOR, alpha);
        _pieview_set_color(temp, 3, IDLE_COLOR, idle_alpha);

        // Resize effect for total cpu
        if (i == 0) {
            radius = r + (w + inoffset) * (cur->cnt - 1);
            inradius = r - w;
            _pieview_resize(temp, radius, inradius);
        }

        _pieview_update(temp, ctx->win, trans, 0, 1.0f);
        cv->pieviews[i] = temp;
    }

    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_END,
            _cpuview_update_end, "p", cv);
    _nemotale_transition_render(trans, ctx->node, ctx->group);
    _win_trans_do(ctx->win, trans);
    cv->trans = trans;

    struct nemotool *tool = _win_get_tool(ctx->win);
    // Cpuview Update timer
    struct nemotimer *timer = nemotimer_create(tool);
    nemotimer_set_callback(timer, _cpuview_timeout);
    nemotimer_set_userdata(timer, cv);
    nemotimer_set_timeout(timer, CPUVIEW_TIMEOUT);
    cv->timer = timer;

    return cv;
}

static void
_tale_event(struct nemotale *tale, struct talenode *node, uint32_t type, struct taleevent *event)
{
    Context *ctx = nemotale_get_userdata(tale);
    struct nemocanvas *canvas = _win_get_canvas(ctx->win);
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    CpuView *cv = ctx->cv;

    struct taletap *taps[16];
    int ntaps;
    struct taletap *tap;

    ntaps = nemotale_get_node_taps(tale, node, taps, type);
    tap = nemotale_get_tap(tale, event->device, type);

    if (type & NEMOTALE_DOWN_EVENT) {
        tap->item = nemotale_path_pick_one(ctx->group, event->x, event->y);
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
                _cpuview_change(cv);
            } else if (ntaps == 2) {
                ERR("");
            } else if (ntaps == 3) {
                if (nemotale_is_single_click(tale, event, type)) {
                    nemotool_exit(tool);
                }
            }
        }
    }
}

int main(){
    int w = 400, h = 400;
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
    nemotale_set_userdata(tale, ctx);

    struct talenode *node;
    node = nemotale_node_create_pixman(w, h);
    nemotale_node_set_id(node, 1);
    nemotale_attach_node(tale, node);
    nemotale_node_opaque(node, 0, 0, w, h);
    ctx->node = node;

    struct pathone *group;
    struct pathone *one;
    group = nemotale_path_create_group();
    ctx->group = group;

    // Background
    one = nemotale_path_create_rect(w, h);
    nemotale_path_set_id(one, "bg");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_operator(one, CAIRO_OPERATOR_SOURCE);
    nemotale_path_set_fill_color(NTPATH_STYLE(one),
            0.0f, 0.0f, 0.0f, 0.0f);
    nemotale_path_attach_one(group, one);

    // CpuView
    int offset = 100;
    int inoffset = 2;
    int pw = 20;
    int pr = (w - offset * 2)/2;
    CpuView *cv = _cpuview_create(ctx, pr, pw, offset, inoffset);
    _cpuview_move(cv, w/2, h/2);
    ctx->cv = cv;

	nemotale_path_update_one(group);
	nemotale_node_render_path(node, group);
	nemotale_composite(tale, NULL);

    nemocanvas_dispatch_frame(canvas);

    nemotool_run(tool);

    _cpuview_destroy(cv);
    nemotale_path_destroy_one(one);
    nemotale_path_destroy_one(group);
    nemotale_node_destroy(node);

    _win_destroy(win);
    nemotool_disconnect_wayland(tool);
    nemotool_destroy(tool);
    free(ctx);

    return 0;
}
