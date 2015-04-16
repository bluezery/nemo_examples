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

typedef struct _MemStat {
    int phy_total;
    int phy_used;
    int phy_free;
    int swap_total;
    int swap_used;
    int swap_free;
} MemStat;

#define CPUVIEW_TIMEOUT 1000

static const char *PHY_MEM = "Phy. Mem.";
static const char *SWAP_MEM = "Swap. Mem.";

typedef struct _Context Context;
typedef struct _MemView MemView;

struct _MemView {
    Context *ctx;
    int state;
    PieView *pieview;

    char *str_name;
    int mtotal, mfree, mused;

    struct pathone *txt_name;
    struct pathone *txt_total;
    struct pathone *txt_free;
    struct pathone *txt_used;


    struct taletransition *trans;
};

struct _Context {
    _Win *win;

    struct talenode *node;
    struct pathone *group;
    struct pathone *bg;

    int w, h;
    MemView *memview;
};

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

static void
_memview_update_end(struct taletransition *trans, void *ctx, void *data)
{
    MemView *memview = data;
    memview->trans = NULL;
}

static void
_memview_update(MemView *memview)
{
    if (memview->trans) {
        nemotale_transition_revoke(memview->trans);
        nemotale_path_set_fill_color(NTPATH_STYLE(memview->txt_name),
                0, 1, 1, 0.8);
        nemotale_path_set_fill_color(NTPATH_STYLE(memview->txt_total),
                0, 0, 1, 0.8);
        nemotale_path_set_fill_color(NTPATH_STYLE(memview->txt_used),
                1, 0, 0, 0.8);
        nemotale_path_set_fill_color(NTPATH_STYLE(memview->txt_free),
                0, 1, 0, 0.8);
    }

    PieView *pieview = memview->pieview;
    Context *ctx = memview->ctx;
    MemStat *stat = _memstat_get();

    struct taletransition *trans;
    trans = _win_trans_create(ctx->win, 0, 500, NEMOEASE_CUBIC_OUT_TYPE);
    _win_trans_damage(ctx->win, trans, ctx->node, ctx->group);

    int mtotal, mused, mfree;
    if (memview->state == 0) {   // Physical view
        _pieview_change_power(pieview, 0, stat->phy_used);
        _pieview_change_power(pieview, 1, stat->phy_free);
        mtotal = stat->phy_total/1024;
        mused = stat->phy_used/1024;
        mfree = stat->phy_free/1024;

        if (strcmp(memview->str_name, PHY_MEM)) {
            free(memview->str_name);
            memview->str_name = strdup(PHY_MEM);
            nemotale_path_load_text(memview->txt_name, PHY_MEM, strlen(PHY_MEM));
            nemotale_transition_attach_dattrs(trans,
                    NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(memview->txt_name)),
                    4, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f);
            nemotale_transition_attach_dattrs(trans,
                    NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(memview->txt_name)),
                    4, 1.0f, 0.0f, 1.0f, 1.0f, 0.8f);
        }
    } else {    // Swap view
        _pieview_change_power(pieview, 0, stat->swap_used);
        _pieview_change_power(pieview, 1, stat->swap_free);
        mtotal = stat->swap_total/1024;
        mused = stat->swap_used/1024;
        mfree = stat->swap_free/1024;

        if (strcmp(memview->str_name, SWAP_MEM)) {
            free(memview->str_name);
            memview->str_name = strdup(SWAP_MEM);
            nemotale_path_load_text(memview->txt_name, SWAP_MEM, strlen(SWAP_MEM));
            nemotale_transition_attach_dattrs(trans,
                    NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(memview->txt_name)),
                    4, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f);
            nemotale_transition_attach_dattrs(trans,
                    NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(memview->txt_name)),
                    4, 1.0f, 0.0f, 1.0f, 1.0f, 0.8f);
        }
    }
    _memstat_free(stat);

    if (memview->mtotal != mtotal) {
        memview->mtotal = mtotal;
        char percent[256];
        snprintf(percent, 255, "Total %dMB", mtotal);
        nemotale_path_load_text(memview->txt_total, percent, strlen(percent));
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(memview->txt_total)),
                4, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(memview->txt_total)),
                4, 1.0f, 0.0f, 0.0f, 1.0f, 0.8f);
    }
    if (memview->mused != mused) {
        memview->mused = mused;
        char percent[256];
        snprintf(percent, 255, "Used %dMB", mused);
        nemotale_path_load_text(memview->txt_used, percent, strlen(percent));
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(memview->txt_used)),
                4, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(memview->txt_used)),
                4, 1.0f, 1.0f, 0.0f, 0.0f, 0.8f);
    }
    if (memview->mfree != mfree) {
        memview->mfree = mfree;
        char percent[256];
        snprintf(percent, 255, "Free %dMB", mfree);
        nemotale_path_load_text(memview->txt_free, percent, strlen(percent));
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(memview->txt_free)),
                4, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(memview->txt_free)),
                4, 1.0f, 0.0f, 1.0f, 0.0f, 0.8f);
    }

    _win_trans_transform(ctx->win, trans, memview->txt_total);
    _win_trans_transform(ctx->win, trans, memview->txt_used);
    _win_trans_transform(ctx->win, trans, memview->txt_free);

    _pieview_update(ctx->win, trans, pieview);

    _transit_add_event_end(trans, _memview_update_end, ctx, memview);
    _win_trans_render(ctx->win, trans, ctx->node, ctx->group);
    _win_trans_do(ctx->win, trans);
    memview->trans = trans;
}

static void
_memview_change(MemView *memview)
{
    memview->state++;
    if (memview->state >= 2)
        memview->state = 0;
    _memview_update(memview);
}

static void
_memview_init(MemView *memview)
{
    PieView *pieview = memview->pieview;
    MemStat *stat = _memstat_get();
    memview->state = 0;
    _pieview_add_ap(pieview, stat->phy_used);
    _pieview_set_color(pieview, 0, 1, 0, 0, 0.8);
    _pieview_add_ap(pieview, stat->phy_free);
    _pieview_set_color(pieview, 1, 0, 1, 0, 0.8);
    memview->mtotal = stat->phy_total/1024;
    memview->mused = stat->phy_used/1024;
    memview->mfree = stat->phy_free/1024;
    _memstat_free(stat);

    char percent[256];
    snprintf(percent, 255, "Total %dMB", memview->mtotal);
    nemotale_path_load_text(memview->txt_total, percent, strlen(percent));
    snprintf(percent, 255, "Used %dMB", memview->mused);
    nemotale_path_load_text(memview->txt_used, percent, strlen(percent));
    snprintf(percent, 255, "Free %dMB", memview->mfree);
    nemotale_path_load_text(memview->txt_free, percent, strlen(percent));

    Context *ctx = memview->ctx;
    struct taletransition *trans;
    trans = _win_trans_create(ctx->win, 0, 1000, NEMOEASE_CUBIC_OUT_TYPE);
    _win_trans_damage(ctx->win, trans, ctx->node, ctx->group);

    _pieview_update(ctx->win, trans, pieview);

    _win_trans_render(ctx->win, trans, ctx->node, ctx->group);
    _win_trans_do(ctx->win, trans);
}

static void
_memview_timeout(struct nemotimer *timer, void *data)
{
    MemView *memview = data;

    _memview_update(memview);

    nemotimer_set_callback(timer, _memview_timeout);
    nemotimer_set_userdata(timer, data);
    nemotimer_set_timeout(timer, CPUVIEW_TIMEOUT);
}

static void
_tale_event(struct nemotale *tale, struct talenode *node, uint32_t type, struct taleevent *event)
{
    Context *ctx = nemotale_get_userdata(tale);
    struct nemocanvas *canvas = _win_get_canvas(ctx->win);
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    MemView *memview = ctx->memview;

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
                        _memview_change(memview);
                }
            } else if (ntaps == 3) {
                if (nemotale_is_single_click(tale, event, type)) {
                    nemotool_exit(tool);
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

    // MemView
    MemView *memview = calloc(sizeof(MemView), 1);
    ctx->memview = memview;

    memview->ctx = ctx;

    // MemView: PieView
    PieView *pieview;
    pieview = _pieview_create(group, (w-20)/2, 80);
    _pieview_set_data(pieview, ctx);
    memview->pieview = pieview;

    // MemView: Pieview Text
    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "txt_name");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 50);
    nemotale_path_set_font_weight(NTPATH_STYLE(one),
            CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 1, 1, 0.8);
    nemotale_path_load_text(one, PHY_MEM, strlen(PHY_MEM));
    nemotale_path_translate(one, w/2, h/2 - 50);
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    memview->txt_name = one;
    memview->str_name = strdup(PHY_MEM);

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "Total");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
    nemotale_path_set_font_weight(NTPATH_STYLE(one),
            CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 0, 1, 0.8);
    nemotale_path_load_text(one, "Total 0MB", strlen("Total 00%"));
    nemotale_path_translate(one, w/2, h/2);
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    memview->txt_total = one;

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "Used");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
    nemotale_path_set_font_weight(NTPATH_STYLE(one),
            CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1, 0, 0, 0.8);
    nemotale_path_load_text(one, "Used  0MB", strlen("Used 00%"));
    nemotale_path_translate(one, w/2, h/2 + 25);
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    memview->txt_used = one;

    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "Free");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_font_family(NTPATH_STYLE(one), "Sans");
    nemotale_path_set_font_size(NTPATH_STYLE(one), 25);
    nemotale_path_set_font_weight(NTPATH_STYLE(one),
            CAIRO_FONT_WEIGHT_BOLD);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 1, 0, 0.8);
    nemotale_path_load_text(one, "Free 0MB", strlen("Free 00%"));
    nemotale_path_translate(one, w/2, h/2 + 50);
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_attach_one(group, one);
    memview->txt_free = one;

    // MemView init
    _memview_init(memview);

    // Memview Update timer
    struct nemotimer *timer = nemotimer_create(tool);
    _memview_timeout(timer, memview);

    nemotool_run(tool);

    nemotimer_destroy(timer);
    _pieview_destroy(memview->pieview);

    nemotale_path_destroy_one(group);
    nemotale_node_destroy(node);

    _win_destroy(win);
    nemotool_disconnect_wayland(tool);
    nemotool_destroy(tool);
    free(ctx);

    return 0;
}
