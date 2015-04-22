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

#define UPDATE_TIMEOUT 1000
#define TRANS_DURATION 1000

/******************/
/**** Cpu Stat ****/
/******************/
typedef struct _Cpu Cpu;
struct _Cpu
{
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
};

typedef struct _CpuStat CpuStat;
struct _CpuStat
{
    int cnt;
    Cpu *cpus;
};

static void
_cpustat_free(CpuStat *stat)
{
    free(stat->cpus);
    free(stat);
}

static void
_cpustat_load(CpuStat *stat)
{
    int line_len;
    char **lines = _file_load("/proc/stat", &line_len);

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
}

static CpuStat *
_cpustat_create()
{
    CpuStat *stat = calloc(sizeof(CpuStat), 1);
    _cpustat_load(stat);

    int i = 0;
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
    CpuStat *new = calloc(sizeof(CpuStat), 1);
    _cpustat_load(new);

    int i = 0;
    for (i = 0 ; i < stat->cnt ; i++) {
        int total;
        total = new->cpus[i].total - stat->cpus[i].total;
        if (total != 0) {
            stat->cpus[i].user_share =
              (int)((((double)new->cpus[i].user - stat->cpus[i].user) * 100)/total);
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
    free(new);
}

/*******************/
/**** Cpu View ****/
/*******************/
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

    struct taletransition *pie_trans, *text_trans;
    struct nemolistener pie_trans_listener, text_trans_listener;
};

static void
_cpuview_destroy(CpuView *view)
{

    if (view->pie_trans) nemotale_transition_destroy(view->pie_trans);
    if (view->text_trans) nemotale_transition_destroy(view->text_trans);

    int i = 0;
    for (i = 0 ; i < view->stat->cnt ; i++) {
        _pieview_destroy(view->pieviews[i]);
    }
    free(view->pieviews);
    _cpustat_free(view->stat);

    nemotale_path_destroy_one(view->txt_name);
    nemotale_path_destroy_one(view->txt_user);
    nemotale_path_destroy_one(view->txt_system);
    nemotale_path_destroy_one(view->txt_nice);
    nemotale_path_destroy_one(view->group);
}

static void
_cpuview_text_trans_destroyed(struct nemolistener *listener, void *data)
{
    CpuView *view = container_of(listener, CpuView, text_trans_listener);
    view->text_trans = NULL;
}

static void
_cpuview_dispatch_text_trans(CpuView *view, CpuStat *stat)
{
    _NemoWin *win = view->win;
    // First CPU shows Total CPU Usage
    Cpu cpu = stat->cpus[0];

    if (view->text_trans) {
        nemotale_transition_destroy(view->text_trans);
    }

    struct taletransition *trans;
    trans = nemotale_transition_create(0, TRANS_DURATION);
    nemotale_transition_attach_timing(trans, 1.0f, NEMOEASE_CUBIC_OUT_TYPE);
    _nemotale_transition_damage(trans, win);

    char percent[256];
    if (cpu.user_share != _pieview_get_power(view->pieviews[0], 0)) {
        snprintf(percent, 255, "user %2d%%", cpu.user_share);
        nemotale_path_transform_enable(view->txt_user);
        nemotale_path_load_text(view->txt_user, percent, strlen(percent));
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(view->txt_user)),
                4, 0.5f, USER_COLOR,  0.0f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(view->txt_user)),
                4, 1.0f, USER_COLOR, 0.8f);
        nemotale_transition_attach_signal(trans,
                NTPATH_DESTROY_SIGNAL(view->txt_user));
    }

    if (cpu.nice_share != _pieview_get_power(view->pieviews[0], 1)) {
        snprintf(percent, 255, "nice %2d%%", cpu.nice_share);
        nemotale_path_transform_enable(view->txt_nice);
        nemotale_path_load_text(view->txt_nice, percent, strlen(percent));
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(view->txt_nice)),
                4, 0.5f, NICE_COLOR, 0.0f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(view->txt_nice)),
                4, 1.0f, NICE_COLOR, 0.8f);
        nemotale_transition_attach_signal(trans,
                NTPATH_DESTROY_SIGNAL(view->txt_nice));
    }

    if (cpu.system_share != _pieview_get_power(view->pieviews[0], 2)) {
        snprintf(percent, 255, "system %2d%%", cpu.system_share);
        nemotale_path_transform_enable(view->txt_system);
        nemotale_path_load_text(view->txt_system, percent, strlen(percent));
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(view->txt_system)),
                4, 0.5f, SYSTEM_COLOR, 0.0f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(view->txt_system)),
                4, 1.0f, SYSTEM_COLOR, 0.8f);
        nemotale_transition_attach_signal(trans,
                NTPATH_DESTROY_SIGNAL(view->txt_system));
    }
    nemolist_init(&(view->text_trans_listener.link));
    view->text_trans_listener.notify = _cpuview_text_trans_destroyed;
    nemosignal_add(NTTRANS_DESTROY_SIGNAL(trans), &view->text_trans_listener);
    _nemotale_transition_dispatch(trans, win);
    view->text_trans = trans;
}

static void
_cpuview_pie_trans_destroyed(struct nemolistener *listener, void *data)
{
    CpuView *view = container_of(listener, CpuView, pie_trans_listener);
    view->pie_trans = NULL;
}

static void
_cpuview_dispatch_pie_trans(CpuView *view, CpuStat *stat)
{
    _NemoWin *win = view->win;

    if (!view->pieviews) {
        view->pieviews = calloc(sizeof(PieView *), stat->cnt - 1);
    }

    // Pie Transition
    struct taletransition *trans;
    trans = nemotale_transition_create(0, TRANS_DURATION);
    nemotale_transition_attach_timing(trans, 1.0f, NEMOEASE_CUBIC_OUT_TYPE);
    _nemotale_transition_damage(trans, win);

    if (view->pie_trans) {
        nemotale_transition_destroy(view->pie_trans);
    }

    int i = 0;
    for (i = 0 ; i < stat->cnt ; i++) {
        double offset = (double)1.0/stat->cnt;
        int radius = 0, inradius = 0;
        if (i == 0) {
            radius = view->r + (view->w + view->inoffset) * (stat->cnt - 1);
            inradius = view->r - view->w;
        } else {
            radius = view->r + ((view->w + view->inoffset) * (i -1));
            inradius = radius - view->w;
        }

        PieView *pv = view->pieviews[i];
        if (!pv) {
            pv = _pieview_create(view->group, radius, inradius);
            _pieview_add_ap(pv);
            _pieview_add_ap(pv);
            _pieview_add_ap(pv);
            _pieview_add_ap(pv);
            view->pieviews[i] = pv;
        }

        _pieview_change_power(pv, 0, stat->cpus[i].user_share);
        _pieview_change_power(pv, 1, stat->cpus[i].nice_share);
        _pieview_change_power(pv, 2, stat->cpus[i].system_share);
        _pieview_change_power(pv, 3, stat->cpus[i].idle_share);

        double delay = 0, to = 1;

        if (view->state_changed) {
            if (view->state == 1) {
                radius = view->r + ((view->w + view->inoffset) * i);
                inradius = radius - view->w;
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
                    radius = view->r + (view->w + view->inoffset) * (stat->cnt - 1);
                    inradius = view->r - view->w;
                    _pieview_set_color(pv, 0, USER_COLOR, ALPHA);
                    _pieview_set_color(pv, 1, NICE_COLOR, ALPHA);
                    _pieview_set_color(pv, 2, SYSTEM_COLOR, ALPHA);
                    _pieview_set_color(pv, 3, IDLE_COLOR, IDLE_ALPHA);
                } else {
                    radius = view->r + ((view->w + view->inoffset) * (i -1));
                    inradius = radius - view->w;
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
    view->state_changed = false;

    view->pie_trans_listener.notify = _cpuview_pie_trans_destroyed;
    nemosignal_add(NTTRANS_DESTROY_SIGNAL(trans), &view->pie_trans_listener);

    _nemotale_transition_dispatch(trans, win);
    view->pie_trans = trans;
}

static void
_cpuview_change(CpuView *view)
{
    view->state++;
    if (view->state >= 2)
        view->state = 0;
    view->state_changed = true;

    if (view->pie_trans) {
        nemotale_transition_destroy(view->pie_trans);
        view->pie_trans = NULL;
    }
}

static CpuView *
_cpuview_create(_NemoWin *win, struct pathone *parent, int r, int w, int offset, int inoffset)
{
    CpuView *view;
    view = calloc(sizeof(CpuView), 1);
    view->win = win;

    view->state = 0;
    view->r = r;
    view->w = w;
    view->inoffset = inoffset;

    CpuStat *stat = _cpustat_create();
    view->stat = stat;

    struct pathone *one;
    struct pathone *group;
    group = nemotale_path_create_group();
    nemotale_path_attach_one(parent, group);
    view->group = group;

    // Text
    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "txt_name");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
    nemotale_path_set_font_weight(NTPATH_STYLE(one), CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), IDLE_COLOR, 1.0);
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    nemotale_path_load_text(one, "CPU Total", strlen("Cpu Total"));
    nemotale_path_translate(one, 0, -50);
    view->txt_name = one;

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "txt_user");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
    nemotale_path_set_font_weight(NTPATH_STYLE(one), CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), USER_COLOR, 1.0);
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    nemotale_path_translate(one, 0, -10);
    view->txt_user = one;

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "txt_system");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
    nemotale_path_set_font_weight(NTPATH_STYLE(one), CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), SYSTEM_COLOR, 1.0);
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    nemotale_path_translate(one, 0, 15);
    view->txt_system = one;

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "txt_nice");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
    nemotale_path_set_font_weight(NTPATH_STYLE(one),
            CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), NICE_COLOR, 1.0);
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    nemotale_path_translate(one, 0, 40);
    view->txt_nice = one;

    Cpu cpu = stat->cpus[0];

    char percent[256];
    snprintf(percent, 255, "user %2d%%", stat->cpus[0].user_share);
    nemotale_path_load_text(view->txt_user, percent, strlen(percent));

    snprintf(percent, 255, "system %2d%%", stat->cpus[0].system_share);
    nemotale_path_load_text(view->txt_system, percent, strlen(percent));

    snprintf(percent, 255, "nice %2d%%", stat->cpus[0].nice_share);
    nemotale_path_load_text(view->txt_nice, percent, strlen(percent));

    // CpuView: Pie
    struct taletransition *trans;
    trans = nemotale_transition_create(0, TRANS_DURATION);
    nemotale_transition_attach_timing(trans, 1.0f, NEMOEASE_CUBIC_OUT_TYPE);
    _nemotale_transition_damage(trans, win);

    view->pieviews = calloc(sizeof(PieView *), stat->cnt - 1);
    int i = 0;
    for (i = 0 ; i < stat->cnt ; i++) {
        // Resize effect for total cpu
        int radius = 0, inradius = 0;
        if (i == 0) {
            radius = r + (w + inoffset) * (stat->cnt - 1);
            inradius = r - w;
        } else {
            radius = view->r + ((view->w + view->inoffset) * (i -1));
            inradius = radius - view->w;
        }

        double alpha = 0.0, idle_alpha = 0.0;
        if (i == 0) {
            alpha = 1.0;
            idle_alpha = IDLE_ALPHA;
        }

        PieView *pv = _pieview_create(group, radius, inradius);
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

        _pieview_update(pv, win, trans, 0, 1.0f);
        view->pieviews[i] = pv;
    }

    nemolist_init(&(view->pie_trans_listener.link));
    view->pie_trans_listener.notify = _cpuview_pie_trans_destroyed;
    nemosignal_add(NTTRANS_DESTROY_SIGNAL(trans), &view->pie_trans_listener);

    _nemotale_transition_dispatch(trans, win);
    view->pie_trans = trans;

    return view;
}

/****************************/
/******** Mem Stat *********/
/****************************/
typedef struct _MemStat MemStat;
struct _MemStat {
    int phy_total;
    int phy_used;
    int phy_free;
    int swap_total;
    int swap_used;
    int swap_free;
};

static void
_memstat_free(MemStat *stat)
{
    free(stat);
}

static void
_memstat_load(MemStat *stat)
{
    int line_len;
    char **lines = _file_load("/proc/meminfo", &line_len);
    int i = 0;

    for (i = 0 ; i < line_len ; i++) {
        if (!strncmp(lines[i], "MemTotal:", 9)) {
            char *token;
            token = strtok(lines[i], " ");
            token = strtok(NULL, " ");
            stat->phy_total = atoi(token)/1024;
        }
        if (!strncmp(lines[i], "MemFree:", 8)) {
            char *token;
            token = strtok(lines[i], " ");
            token = strtok(NULL, " ");
            stat->phy_free = atoi(token)/1024;
        }
        if (!strncmp(lines[i], "SwapTotal:", 10)) {
            char *token;
            token = strtok(lines[i], " ");
            token = strtok(NULL, " ");
            stat->swap_total = atoi(token)/1024;
        }
        if (!strncmp(lines[i], "SwapFree:", 9)) {
            char *token;
            token = strtok(lines[i], " ");
            token = strtok(NULL, " ");
            stat->swap_free = atoi(token)/1024;
        }
        free(lines[i]);
    }
    free(lines);

    stat->phy_used = stat->phy_total - stat->phy_free;
    stat->swap_used = stat->swap_total - stat->swap_free;
}

static MemStat *
_memstat_create()
{
    MemStat *stat = calloc(sizeof(MemStat), 1);
    _memstat_load(stat);

    return stat;
}

static void
_memstat_update(MemStat *stat)
{
    _memstat_load(stat);
}

/**********************/
/***** Mem View ******/
/**********************/
typedef struct _MemView MemView;
struct _MemView {
    _NemoWin *win;
    int r, w, inoffset;

    MemStat *stat;
    PieView **pieviews;

    struct pathone *group;
    struct pathone *txt_name;
    struct pathone *txt_phy_used;
    struct pathone *txt_swap_used;

    struct taletransition *pie_trans, *text_trans;
    struct nemolistener pie_trans_listener, text_trans_listener;
};

static void
_memview_destroy(MemView *view)
{

    if (view->pie_trans) nemotale_transition_destroy(view->pie_trans);
    if (view->text_trans) nemotale_transition_destroy(view->text_trans);

    _pieview_destroy(view->pieviews[0]);
    _pieview_destroy(view->pieviews[1]);
    free(view->pieviews);
    _memstat_free(view->stat);

    nemotale_path_destroy_one(view->txt_phy_used);
    nemotale_path_destroy_one(view->txt_swap_used);
}

static void
_memview_text_trans_destroyed(struct nemolistener *listener, void *data)
{
    MemView *view = container_of(listener, MemView, text_trans_listener);
    view->text_trans = NULL;
}

static void
_memview_dispatch_text_trans(MemView *view)
{
    _NemoWin *win = view->win;

    if (view->text_trans) {
        nemotale_transition_destroy(view->text_trans);
    }

    struct taletransition *trans;
    trans = nemotale_transition_create(0, TRANS_DURATION);
    nemotale_transition_attach_timing(trans, 1.0f, NEMOEASE_CUBIC_OUT_TYPE);
    _nemotale_transition_damage(trans, win);

    char percent[256];
    if (view->stat->phy_used != _pieview_get_power(view->pieviews[0], 0)) {
        snprintf(percent, 255, "Phy.: %dMB", view->stat->phy_used);
        nemotale_path_load_text(view->txt_phy_used, percent,
                strlen(percent));
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(view->txt_phy_used)),
                4, 0.5f, USER_COLOR, 0.0f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(view->txt_phy_used)),
                4, 1.0f, USER_COLOR, 0.8f);
        nemotale_transition_attach_signal(trans,
                NTPATH_DESTROY_SIGNAL(view->txt_phy_used));
    }

    if (view->stat->swap_used != _pieview_get_power(view->pieviews[1], 0)) {
        snprintf(percent, 255, "Swap.: %dMB", view->stat->swap_used);
        nemotale_path_load_text(view->txt_swap_used, percent,
                strlen(percent));
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(view->txt_swap_used)),
                4, 0.5f, SYSTEM_COLOR, 0.0f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(view->txt_swap_used)),
                4, 1.0f, SYSTEM_COLOR, 0.8f);
        nemotale_transition_attach_signal(trans,
                NTPATH_DESTROY_SIGNAL(view->txt_swap_used));
    }

    nemolist_init(&(view->text_trans_listener.link));
    view->text_trans_listener.notify = _memview_text_trans_destroyed;
    nemosignal_add(NTTRANS_DESTROY_SIGNAL(trans), &view->text_trans_listener);
    _nemotale_transition_dispatch(trans, win);
    view->text_trans = trans;
}

static void
_memview_pie_trans_destroyed(struct nemolistener *listener, void *data)
{
    MemView *view = container_of(listener, MemView, pie_trans_listener);
    view->pie_trans = NULL;
}

static void
_memview_dispatch_pie_trans(MemView *view)
{
    if (view->pie_trans) {
        nemotale_transition_destroy(view->pie_trans);
    }
    int i = 0;
    _NemoWin *win = view->win;
    MemStat *stat  = view->stat;

    // Pie Transition
    struct taletransition *trans;
    trans = nemotale_transition_create(0, TRANS_DURATION);
    nemotale_transition_attach_timing(trans, 1.0f, NEMOEASE_CUBIC_OUT_TYPE);
    _nemotale_transition_damage(trans, win);
    int cnt = 2;
    for (i = 0 ; i < cnt ; i++) {
        double offset = (double)1.0/cnt;

        int pow_used, pow_free;
        if (i == 0) {
            pow_used = stat->phy_used;
            pow_free = stat->phy_free;
        } else {
            pow_used = stat->swap_used;
            pow_free = stat->swap_free;
        }
        PieView *pv = view->pieviews[i];
        _pieview_change_power(pv, 0, pow_used);
        _pieview_change_power(pv, 1, pow_free);

        double delay = 0, to = 1;
        int radius, inradius;

        if (1) {
            radius = view->r + ((view->w + view->inoffset) * i);
            inradius = radius - view->w;
            if (i != 0) {
                _pieview_set_color(pv, 0, USER_COLOR, ALPHA);
                _pieview_set_color(pv, 1, IDLE_COLOR, IDLE_ALPHA);
            }
            _pieview_resize(pv, radius, inradius);
            delay = offset * i;
            to = delay + offset;
        } else {
            if (i == 0) {
                radius = view->r + (view->w + view->inoffset) * (cnt - 1);
                inradius = view->r - view->w;
            } else {
                radius = view->r + ((view->w + view->inoffset) * (i -1));
                inradius = radius - view->w;
                _pieview_set_color(pv, 0, USER_COLOR, 0);
                _pieview_set_color(pv, 1, NICE_COLOR, 0);
                _pieview_set_color(pv, 2, SYSTEM_COLOR, 0);
                _pieview_set_color(pv, 3, IDLE_COLOR, 0);
            }
            _pieview_resize(pv, radius, inradius);
            delay = offset * (cnt - i - 1);
            to = delay + offset;
        }

        _pieview_update(pv, win, trans, delay, to);
    }

    view->pie_trans_listener.notify = _memview_pie_trans_destroyed;
    nemosignal_add(NTTRANS_DESTROY_SIGNAL(trans), &view->pie_trans_listener);

    _nemotale_transition_dispatch(trans, win);
    view->pie_trans = trans;
}

static void
_memview_move(MemView *view, int x, int y)
{
    nemotale_path_translate(view->group, (double)x, (double)y);
}

static MemView *
_memview_create(_NemoWin *win, struct pathone *parent, int r, int w, int offset, int inoffset)
{
    MemView *view;
    view = calloc(sizeof(MemView), 1);
    view->win = win;

    view->r = r;
    view->w = w;
    view->inoffset = inoffset;

    struct pathone *one;
    struct pathone *group;
    group = nemotale_path_create_group();
    nemotale_path_attach_one(parent, group);
    view->group = group;

    MemStat *stat = _memstat_create();
    view->stat = stat;

    char percent[256];
    // MemView: Text
    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "txt_name");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
    nemotale_path_set_font_weight(NTPATH_STYLE(one),
            CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), IDLE_COLOR, 1.0);
    nemotale_path_load_text(one, "Phy. Mem.", strlen("Phy. Mem."));
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    nemotale_path_translate(one, 0, - 50);
    view->txt_name = one;

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "txt_phy_used");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
    nemotale_path_set_font_weight(NTPATH_STYLE(one),
            CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), USER_COLOR, 1.0);

    snprintf(percent, 255, "Phy.: %dMB", stat->phy_used);
    nemotale_path_load_text(one, percent, strlen(percent));

    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    nemotale_path_translate(one, 0, - 10);
    view->txt_phy_used = one;

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "txt_swap_used");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
    nemotale_path_set_font_weight(NTPATH_STYLE(one),
            CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), SYSTEM_COLOR, 1.0);

    snprintf(percent, 255, "Swap.: %dMB", stat->swap_used);
    nemotale_path_load_text(one, percent, strlen(percent));

    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    nemotale_path_translate(one, 0, 40);
    view->txt_swap_used = one;

    // MemView: Pie
    struct taletransition *trans;
    trans = nemotale_transition_create(0, TRANS_DURATION);
    nemotale_transition_attach_timing(trans, 1.0f, NEMOEASE_CUBIC_OUT_TYPE);
    _nemotale_transition_damage(trans, win);

    int cnt = 2;
    view->pieviews = calloc(sizeof(PieView *), cnt);
    int i = 0;
    for (i = 0 ; i < cnt ; i++) {
        PieView *pv = NULL;
        int radius, inradius;
        radius = view->r + ((view->w + view->inoffset) * i);
        inradius = radius - view->w;

        pv = _pieview_create(group, radius, inradius);
        _pieview_add_ap(pv);
        _pieview_add_ap(pv);
        _pieview_add_ap(pv);
        _pieview_add_ap(pv);

        int pow_used, pow_free;
        if (i == 0) {
            pow_used = stat->phy_used;
            pow_free = stat->phy_free;
        } else {
            pow_used = stat->swap_used;
            pow_free = stat->swap_free;
        }
        _pieview_change_power(pv, 0, pow_used);
        _pieview_change_power(pv, 1, pow_free);
        _pieview_set_color(pv, 0, USER_COLOR, ALPHA);
        _pieview_set_color(pv, 1, IDLE_COLOR, IDLE_ALPHA);

        _pieview_resize(pv, radius, inradius);

        _pieview_update(pv, win, trans, 0, 1.0f);
        view->pieviews[i] = pv;
    }

    nemolist_init(&(view->pie_trans_listener.link));
    view->pie_trans_listener.notify = _memview_pie_trans_destroyed;
    nemosignal_add(NTTRANS_DESTROY_SIGNAL(trans), &view->pie_trans_listener);

    _nemotale_transition_dispatch(trans, win);
    view->pie_trans = trans;

    return view;
}

/**************************/
/********* Context ********/
/**************************/
typedef struct _Context Context;
struct _Context {
    _NemoWin *win;

    int w, h;
    CpuView *cpu_view;
    MemView *mem_view;

    int prev_x, prev_y;

    struct nemotimer *timer;

    struct pathone *move_group;
    struct taletransition *move_trans;
    struct nemolistener move_trans_listener;
};

static void
_update_timeout(struct nemotimer *timer, void *data)
{
    Context *ctx = data;
    _cpustat_update(ctx->cpu_view->stat);
    _cpuview_dispatch_text_trans(ctx->cpu_view, ctx->cpu_view->stat);
    _cpuview_dispatch_pie_trans(ctx->cpu_view, ctx->cpu_view->stat);

    _memstat_update(ctx->mem_view->stat);
    _memview_dispatch_text_trans(ctx->mem_view);
    _memview_dispatch_pie_trans(ctx->mem_view);

    nemotimer_set_timeout(timer, UPDATE_TIMEOUT);
}

static void
_move_trans_destroyed(struct nemolistener *listener, void *data)
{
    Context *ctx = container_of(listener, Context, move_trans_listener);
    ctx->move_trans = NULL;
}

static void
_tale_event(struct nemotale *tale, struct talenode *node, uint32_t type, struct taleevent *event)
{
    _NemoWin *win = nemotale_get_userdata(tale);
    Context *ctx = _nemowin_get_userdata(win);
    struct nemocanvas *canvas = _nemowin_get_canvas(win);
    struct nemotool *tool = _nemowin_get_tool(win);
    CpuView *view = ctx->cpu_view;

    int ntaps;
    struct taletap *taps[16];
    struct taletap *tap;

    ntaps = nemotale_get_node_taps(tale, node, taps, type);
    tap = nemotale_get_tap(tale, event->device, type);

    if (type & NEMOTALE_UP_EVENT) {
        struct pathone *one = tap->item;
        if (one) {
            if (ctx->move_trans) nemotale_transition_destroy(ctx->move_trans);

            struct taletransition *trans;
            trans = nemotale_transition_create(0, TRANS_DURATION);
            nemotale_transition_attach_timing(trans, 1.0f, NEMOEASE_CUBIC_OUT_TYPE);
            _nemotale_transition_damage(trans, ctx->win);
            _nemotale_transition_transform(trans, ctx->move_group);

            if ((NTPATH_TRANSFORM_X(ctx->move_group) < 0)) {
                nemotale_transition_attach_dattr(trans,
                        NTPATH_TRANSFORM_ATX(ctx->move_group),
                        1.0f, (double)ctx->w/2 - ctx->w);
            } else  {
                nemotale_transition_attach_dattr(trans,
                        NTPATH_TRANSFORM_ATX(ctx->move_group),
                        1.0f, (double)ctx->w/2);
            }
            nemotale_transition_attach_signal(trans,
                    NTPATH_DESTROY_SIGNAL(ctx->move_group));
            nemolist_init(&(ctx->move_trans_listener.link));
            ctx->move_trans_listener.notify = _move_trans_destroyed;
            nemosignal_add(NTTRANS_DESTROY_SIGNAL(trans), &ctx->move_trans_listener);
            _nemotale_transition_dispatch(trans, win);
            ctx->move_trans = trans;
        }
    }

    // Reset prev x, y
    if ((type & NEMOTALE_DOWN_EVENT) ||
            (type & NEMOTALE_UP_EVENT)) {
        ctx->prev_y = -9999;
        ctx->prev_x = -9999;
    }
    if (type & NEMOTALE_DOWN_EVENT) {
        struct pathone *group = _nemowin_get_group(win);
        tap->item = nemotale_path_pick_one(group, event->x, event->y);
        if (ntaps == 1) {
            struct pathone *one = tap->item;
            if (one && NTPATH_ID(one) &&
                !strcmp(NTPATH_ID(one), "bg"))
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
                NTPATH_TRANSFORM_X(ctx->move_group);
            double pos_y = event->y - ctx->prev_y +
                NTPATH_TRANSFORM_Y(ctx->move_group);

            ctx->prev_x = event->x;
            ctx->prev_y = event->y;

            if (ctx->move_trans) nemotale_transition_destroy(ctx->move_trans);

            nemotale_path_translate(ctx->move_group, pos_x, ctx->h/2);
            _nemowin_dirty(win);
        }
    } else if (nemotale_is_single_click(tale, event, type)) {
        if (ntaps == 1) {
            _cpuview_change(view);
            nemotimer_set_timeout(ctx->timer, 16);
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

    struct pathone *group;
    group = nemotale_path_create_group();
    nemotale_path_attach_one(_nemowin_get_group(win), group);
    ctx->move_group = group;

    // CpuView
    int offset = 100;               // offset
    int inoffset = 2;               // offset between pieviews
    int pw = 20;                    // pieviews width
    int pr = (w - offset * 2)/2;    // center pieview radius
    ctx->cpu_view = _cpuview_create(win, ctx->move_group, pr, pw, offset,
            inoffset);
    ctx->mem_view = _memview_create(win, ctx->move_group, pr, pw, offset,
            inoffset);
    _memview_move(ctx->mem_view, w, 0);

    nemotale_path_translate(ctx->move_group, w/2, h/2);

    _nemowin_show(win);

    // Update timer
    struct nemotimer *timer = nemotimer_create(tool);
    nemotimer_set_callback(timer, _update_timeout);
    nemotimer_set_userdata(timer, ctx);
    nemotimer_set_timeout(timer, UPDATE_TIMEOUT);
    ctx->timer = timer;

    nemotool_run(tool);

    _memview_destroy(ctx->mem_view);
    _cpuview_destroy(ctx->cpu_view);

    nemotimer_destroy(ctx->timer);
    nemotale_path_destroy_one(ctx->move_group);
    if (ctx->move_trans) nemotale_transition_destroy(ctx->move_trans);

    _nemowin_destroy(win);
    nemotool_disconnect_wayland(tool);
    nemotool_destroy(tool);

    free(ctx);

    return 0;
}
