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

#define MAX_NUM_CPU 16
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
    int total;

    int user_share;
    int nice_share;
    int system_share;
    int idle_share;
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
    _NemoWin *win;
    int r, w, inoffset;

    bool state_changed;
    int state;

    CpuStat *stat;
    PieView **pieviews;

    struct pathone *group;
    struct pathone *txt_name;
    struct pathone *txt_user;
    struct pathone *txt_nice;
    struct pathone *txt_system;

    struct taletransition *pie_trans, *text_trans, *move_trans;
    struct nemolistener pie_trans_listener, text_trans_listener, move_trans_listener;
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
_cpustat_load()
{
    int line_len;
    char **lines = _file_load("/proc/stat", &line_len);

    CpuStat *stat = calloc(sizeof(CpuStat), 1);

    int i = 0;
    int cnt = 0;
    Cpu *cpus = NULL;
    for (i = 0 ; i < line_len ; i++) {
        if (!strncmp(lines[i], "cpu", 3)) {
            char *token;

            cnt++;
            int idx = cnt - 1;
            cpus = realloc(cpus, sizeof(Cpu) * cnt);
            token = strtok(lines[i], " ");
            cpus[idx].name = strdup(token);
            token = strtok(NULL, " ");
            cpus[idx].user = atoi(token);
            token = strtok(NULL, " ");
            cpus[idx].nice = atoi(token);
            token = strtok(NULL, " ");
            cpus[idx].system = atoi(token);
            token = strtok(NULL, " ");
            cpus[idx].idle = atoi(token);

            cpus[idx].total = cpus[idx].user + cpus[idx].nice +
                cpus[idx].system + cpus[idx].idle;
        }
        free(lines[i]);
    }
    free(lines);

    stat->cpus = cpus;
    stat->cnt = cnt;
    return stat;
}

static CpuStat *
_cpustat_create()
{
    int i = 0;
    CpuStat *stat = _cpustat_load();
    Cpu *cpus = stat->cpus;

    for (i = 0 ; i < stat->cnt ; i++) {
        if (cpus[i].total == 0) continue;
        cpus[i].user_share = (int)((double)cpus[i].user/cpus[i].total * 100);
        cpus[i].nice_share = (int)((double)cpus[i].nice/cpus[i].total * 100);
        cpus[i].system_share = (int)((double)cpus[i].system/cpus[i].total * 100);
        cpus[i].idle_share = (int)((double)cpus[i].idle/cpus[i].total * 100);
    }
    return stat;
}

static void
_cpustat_update(CpuStat *stat)
{
    CpuStat *new = _cpustat_load();

    int i = 0;
    for (i = 0 ; i < stat->cnt ; i++) {
        int total;
        total = new->cpus[i].total - stat->cpus[i].total;
        if (total != 0) {
            stat->cpus[i].user_share =
              (int)((new->cpus[i].user - stat->cpus[i].user)/(double)total * 100);
            stat->cpus[i].nice_share =
              (int)((new->cpus[i].nice - stat->cpus[i].nice)/(double)total * 100);
            stat->cpus[i].system_share =
              (int)((new->cpus[i].system - stat->cpus[i].system)/(double)total * 100);
            stat->cpus[i].idle_share =
              (int)((new->cpus[i].idle - stat->cpus[i].idle)/(double)total * 100);
        }
        stat->cpus[i].user = new->cpus[i].user;
        stat->cpus[i].nice = new->cpus[i].nice;
        stat->cpus[i].system = new->cpus[i].system;
        stat->cpus[i].idle = new->cpus[i].idle;
        stat->cpus[i].total = new->cpus[i].total;
    }
    _cpustat_free(new);
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
    _NemoWin *win;

    int w, h;
    CpuView *cv;

    int prev_x, prev_y;
};

/*******************/
/**** Cpu View ****/
/*******************/

    static void
_cpuview_destroy(CpuView *cv)
{

    if (cv->pie_trans) nemotale_transition_destroy(cv->pie_trans);
    if (cv->text_trans) nemotale_transition_destroy(cv->text_trans);
    if (cv->move_trans) nemotale_transition_destroy(cv->move_trans);
    if (cv->timer) nemotimer_destroy(cv->timer);

    int i = 0;
    for (i = 0 ; i < cv->stat->cnt ; i++) {
        _pieview_destroy(cv->pieviews[i]);
    }
    free(cv->pieviews);
    _cpustat_free(cv->stat);

    nemotale_path_destroy_one(cv->txt_name);
    nemotale_path_destroy_one(cv->txt_user);
    nemotale_path_destroy_one(cv->txt_system);
    nemotale_path_destroy_one(cv->txt_nice);
    nemotale_path_destroy_one(cv->group);
}

CpuView *_cv;

static void
_text_trans_destroyed(struct nemolistener *listener, void *data)
{
    ERR("destroy: %p", _cv->text_trans);
    _cv->text_trans = NULL;
}

static void
_cpuview_dispatch_text_anim(CpuView *cv)
{
    _NemoWin *win = cv->win;
    // Text transition (0th cpu, Total Usage)
    Cpu cpu = cv->stat->cpus[0];
    bool force_update = false;

    if (cv->text_trans) {
        nemotale_transition_destroy(cv->text_trans);
        force_update = true;
    }

    struct taletransition *trans;
    trans = nemotale_transition_create(0, TRANS_DURATION);
    nemotale_transition_attach_timing(trans, 1.0f, NEMOEASE_CUBIC_OUT_TYPE);
    _nemotale_transition_damage(trans, win);

    char percent[256];
    if (force_update || (cpu.user_share != _pieview_get_power(cv->pieviews[0], 0))) {
        snprintf(percent, 255, "user %2d%%", cpu.user_share);
        nemotale_path_load_text(cv->txt_user, percent,
                strlen(percent));
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cv->txt_user)),
                4, 0.5f, 1.0f, 1.0f, 0.0f, 0.0f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cv->txt_user)),
                4, 1.0f, 1.0f, 1.0f, 0.0f, 0.8f);
    }

    if (force_update || (cpu.nice_share != _pieview_get_power(cv->pieviews[0], 1))) {
        snprintf(percent, 255, "nice %2d%%", cpu.nice_share);
        nemotale_path_load_text(cv->txt_nice, percent,
                strlen(percent));
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cv->txt_nice)),
                4, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cv->txt_nice)),
                4, 1.0f, 0.0f, 1.0f, 0.0f, 0.8f);
    }

    if (force_update || (cpu.system_share != _pieview_get_power(cv->pieviews[0], 2))) {
        snprintf(percent, 255, "system %2d%%", cpu.system_share);
        nemotale_path_load_text(cv->txt_system, percent,
                strlen(percent));
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cv->txt_system)),
                4, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(cv->txt_system)),
                4, 1.0f, 0.0f, 0.0f, 1.0f, 0.8f);
    }
    nemolist_init(&(cv->text_trans_listener.link));
    cv->text_trans_listener.notify = _text_trans_destroyed;
    nemosignal_add(NTTRANS_DESTROY_SIGNAL(trans), &cv->text_trans_listener);
    _nemotale_transition_dispatch(trans, win);
    cv->text_trans = trans;
    ERR("create: %p", cv->text_trans);
}

static void
_pie_trans_destroyed(struct nemolistener *listener, void *data)
{
    struct taletransition *trans = data;
    _cv->pie_trans = NULL;
}

static void
_cpuview_dispatch_pie_anim(CpuView *cv)
{
    if (cv->pie_trans) {
        nemotale_transition_destroy(cv->pie_trans);
    }
    int i = 0;
    _NemoWin *win = cv->win;
    CpuStat *stat  = cv->stat;

    // Pie Transition
    struct taletransition *trans;
    trans = nemotale_transition_create(0, TRANS_DURATION);
    nemotale_transition_attach_timing(trans, 1.0f, NEMOEASE_CUBIC_OUT_TYPE);
    _nemotale_transition_damage(trans, win);
    for (i = 0 ; i < stat->cnt ; i++) {
        double offset = (double)1.0/stat->cnt;

        PieView *pv = cv->pieviews[i];
        _pieview_change_power(pv, 0, stat->cpus[i].user_share);
        _pieview_change_power(pv, 1, stat->cpus[i].nice_share);
        _pieview_change_power(pv, 2, stat->cpus[i].system_share);
        _pieview_change_power(pv, 3, stat->cpus[i].idle_share);

        double delay = 0, to = 1;
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
                    radius = cv->r + (cv->w + cv->inoffset) * (stat->cnt - 1);
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
                delay = offset * (stat->cnt - i - 1);
                to = delay + offset;
            }
        }

        _pieview_update(pv, win, trans, delay, to);
    }

    nemolist_init(&(cv->pie_trans_listener.link));
    cv->pie_trans_listener.notify = _pie_trans_destroyed;
    nemosignal_add(NTTRANS_DESTROY_SIGNAL(trans), &cv->pie_trans_listener);

    _nemotale_transition_dispatch(trans, win);
    cv->pie_trans = trans;
}

    static void
_cpuview_change(CpuView *cv)
{
    cv->state++;
    if (cv->state >= 2)
        cv->state = 0;
    cv->state_changed = true;

    if (cv->pie_trans) {
        nemotale_transition_destroy(cv->pie_trans);
        cv->pie_trans = NULL;
    }
    nemotimer_set_timeout(cv->timer, 16);
}

static void
_move_trans_destroyed(struct nemolistener *listener, void *data)
{
    _cv->move_trans = NULL;
}

static void
_cpuview_move_anim(CpuView *cv, int x, int y)
{
    _NemoWin *win = cv->win;
    if (cv->move_trans) {
        nemotale_transition_destroy(cv->move_trans);
    }

    struct taletransition *trans;
    trans = nemotale_transition_create(0, TRANS_DURATION);
    nemotale_transition_attach_timing(trans, 1.0f, NEMOEASE_CUBIC_OUT_TYPE);
    _nemotale_transition_damage(trans, win);
    _nemotale_transition_transform(trans, cv->group);
    nemotale_transition_attach_dattr(trans, NTPATH_TRANSFORM_ATX(cv->group),
            1.0f, (double)x);
    nemotale_transition_attach_dattr(trans, NTPATH_TRANSFORM_ATY(cv->group),
            1.0f, (double)y);
    nemolist_init(&(cv->move_trans_listener.link));
    cv->move_trans_listener.notify = _move_trans_destroyed;
    nemosignal_add(NTTRANS_DESTROY_SIGNAL(trans), &cv->move_trans_listener);
    _nemotale_transition_dispatch(trans, win);

    cv->move_trans = trans;
}

    static void
_cpuview_move(CpuView *cv, int x, int y)
{
    _NemoWin *win = cv->win;
    if (cv->move_trans) {
        nemotale_transition_destroy(cv->move_trans);
    }

    nemotale_path_translate(cv->group, x, y);
    _nemowin_dirty(win);
}

    static void
_cpuview_timeout(struct nemotimer *timer, void *data)
{
    CpuView *cv = data;

    _cpustat_update(cv->stat);
    _cpuview_dispatch_text_anim(cv);
    _cpuview_dispatch_pie_anim(cv);

    nemotimer_set_timeout(timer, CPUVIEW_TIMEOUT);
}

    static CpuView *
_cpuview_create(_NemoWin *win, int r, int w, int offset, int inoffset)
{
    CpuView *cv;
    cv = calloc(sizeof(CpuView), 1);
    cv->win = win;

    cv->state = 0;
    cv->r = r;
    cv->w = w;
    cv->inoffset = inoffset;

    struct pathone *one;
    struct pathone *parent, *group;
    parent = _nemowin_get_group(win);
    group = nemotale_path_create_group();
    nemotale_path_attach_one(parent, group);
    cv->group = group;

    // CpuView: Text
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

    CpuStat *stat = _cpustat_create();
    cv->stat = stat;

    // CpuView: Pie
    struct taletransition *trans;
    trans = nemotale_transition_create(0, TRANS_DURATION);
    nemotale_transition_attach_timing(trans, 1.0f, NEMOEASE_CUBIC_OUT_TYPE);
    _nemotale_transition_damage(trans, win);

    cv->pieviews = calloc(sizeof(PieView *), stat->cnt - 1);
    int i = 0;
    for (i = 0 ; i < stat->cnt ; i++) {
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

        PieView *pv = NULL;
        pv = _pieview_create(group, radius, inradius);
        _pieview_add_ap(pv);
        _pieview_add_ap(pv);
        _pieview_add_ap(pv);
        _pieview_add_ap(pv);
        _pieview_change_power(pv, 0, stat->cpus[i].user_share);
        _pieview_change_power(pv, 1, stat->cpus[i].nice_share);
        _pieview_change_power(pv, 2, stat->cpus[i].system_share);
        _pieview_change_power(pv, 3, stat->cpus[i].idle_share);
        _pieview_set_color(pv, 0, USER_COLOR, alpha);
        _pieview_set_color(pv, 1, NICE_COLOR, alpha);
        _pieview_set_color(pv, 2, SYSTEM_COLOR, alpha);
        _pieview_set_color(pv, 3, IDLE_COLOR, idle_alpha);

        // Resize effect for total cpu
        if (i == 0) {
            radius = r + (w + inoffset) * (stat->cnt - 1);
            inradius = r - w;
            _pieview_resize(pv, radius, inradius);
        }

        _pieview_update(pv, win, trans, 0, 1.0f);
        cv->pieviews[i] = pv;
    }

    nemolist_init(&(cv->pie_trans_listener.link));
    cv->pie_trans_listener.notify = _pie_trans_destroyed;
    nemosignal_add(NTTRANS_DESTROY_SIGNAL(trans), &cv->pie_trans_listener);

    _nemotale_transition_dispatch(trans, win);
    cv->pie_trans = trans;

    // Cpuview Update timer
    struct nemotool *tool = _nemowin_get_tool(win);
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
    _NemoWin *win = nemotale_get_userdata(tale);
    Context *ctx = _nemowin_get_userdata(win);
    struct nemocanvas *canvas = _nemowin_get_canvas(win);
    struct nemotool *tool = _nemowin_get_tool(win);
    CpuView *cv = ctx->cv;

    int ntaps;
    struct taletap *taps[16];
    struct taletap *tap;

    ntaps = nemotale_get_node_taps(tale, node, taps, type);
    tap = nemotale_get_tap(tale, event->device, type);

    if ((type & NEMOTALE_DOWN_EVENT) ||
            (type & NEMOTALE_UP_EVENT)) {
        ctx->prev_y = -9999;  // reset
        ctx->prev_x = -9999;  // reset
    }
    if (type & NEMOTALE_UP_EVENT) {
        struct pathone *one = tap->item;
        if (one) {
            _cpuview_move_anim(cv, ctx->w/2, ctx->h/2);
        }
    }

    if (type & NEMOTALE_DOWN_EVENT) {
        struct pathone *group = _nemowin_get_group(win);
        tap->item = nemotale_path_pick_one(group, event->x, event->y);
        if (ntaps == 1) {
            struct pathone *one = tap->item;
            if (one) {
                /*
            if (one && NTPATH_ID(one) &&
                !strcmp(NTPATH_ID(one), "txt_name")) {
                */
            } else
                nemocanvas_move(canvas, taps[0]->serial);
        } else if (ntaps == 2) {
            nemocanvas_pick(canvas,
                    taps[0]->serial,
                    taps[1]->serial,
                    (1 << NEMO_SURFACE_PICK_TYPE_ROTATE));
        }
    } else if (type & NEMOTALE_MOTION_EVENT) {
        struct pathone *one = tap->item;
        if (one) {
            /*
        if (one && NTPATH_ID(one) &&
            !strcmp(NTPATH_ID(one), "txt_name")) {
            */
            // scrolling
            if ((ctx->prev_y == -9999) ||
                    (ctx->prev_x == -9999)) {
                ctx->prev_x = event->x; // reset
                ctx->prev_y = event->y; // reset
                return;
            }
            double pos_x = event->x - ctx->prev_x +
                NTPATH_TRANSFORM_X(cv->group);
            double pos_y = event->y - ctx->prev_y +
                NTPATH_TRANSFORM_Y(cv->group);

            ctx->prev_x = event->x;
            ctx->prev_y = event->y;
            _cpuview_move(cv, pos_x, ctx->h/2);
        }
    } else if (nemotale_is_single_click(tale, event, type)) {
        if (ntaps == 1) {
            _cpuview_change(cv);
        } else if (ntaps == 3) {
            if (nemotale_is_single_click(tale, event, type)) {
                nemotool_exit(tool);
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

    _NemoWin *win = _nemowin_create(tool, WIN_TYPE_PIXMAN, w, h, _tale_event);
    _nemowin_set_surface_type(win, NEMO_SURFACE_LAYER_TYPE_OVERLAY);
    _nemowin_set_userdata(win, ctx);
    ctx->win = win;

    // CpuView
    int offset = 100;
    int inoffset = 2;
    int pw = 20;
    int pr = (w - offset * 2)/2;
    CpuView *cv = _cpuview_create(win, pr, pw, offset, inoffset);
    _cpuview_move(cv, w/2, h/2);
    ctx->cv = cv;
    _cv = cv;

    _nemowin_show(win);

    nemotool_run(tool);

    _cpuview_destroy(cv);

    _nemowin_destroy(win);
    nemotool_disconnect_wayland(tool);
    nemotool_destroy(tool);

    free(ctx);

    return 0;
}
