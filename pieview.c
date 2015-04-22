#include "pieview.h"

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

struct _Pie {
    int power;
    struct pathone *one;
    double start, end;
    double r, g, b, a;
};

struct _PieView {
    int r, ir;
    bool resized;
    bool colored;
    bool dirty;
    void *data;

    List *pies;
    List *pies_del;
    Pie *selected;

    struct pathone *group;
    struct pathone *loading_one;
    struct taletransition *loading_trans;
};

void
_pieview_set_data(PieView *pieview, void *data)
{
    pieview->data = data;
}

#if 0
void *
_pieview_get_data(PieView *pieview)
{
    return pieview->data;
}

void
_pieview_reloading(struct taletransition *trans, void *ctx, void *data)
{
    _pieview_loading(data);
}

void
_pieview_loading(_Win *win, struct taletransition *trans, PieView *pieview)
{
    _nemotale_transition_transform(trans, pieview->loading_one);
    _win_trans_add_event_end(win, trans, _pieview_reloading, win, pieview);

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
#endif

PieView *
_pieview_create(struct pathone *group, int r, int ir)
{
    PieView *pieview;
    pieview = calloc(sizeof(PieView), 1);
    pieview->r = r;
    pieview->ir = ir;
    pieview->group = nemotale_path_create_group();
    nemotale_path_attach_one(group, pieview->group);
    pieview->resized = true;
    pieview->colored = true;
    pieview->dirty = true;
    return pieview;
}

void
_pieview_resize(PieView *pieview, int r, int ir)
{
    pieview->r = r;
    pieview->ir = ir;
    pieview->dirty = true;
    pieview->resized = true;
}

void
_pieview_destroy(PieView *pieview)
{
    List *l, *ll;
    Pie *pie;
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
    free(pieview);
}

int
_pieview_count(PieView *pieview)
{
    return list_count(pieview->pies);
}

int
_pieview_get_power(PieView *pieview, int idx)
{
    Pie *pie = list_idx_get_data(pieview->pies, idx);
    return pie->power;
}

Pie *
_pieview_get_ap(PieView *pieview, int idx)
{
    return list_idx_get_data(pieview->pies, idx);
}

struct pathone *
_pieview_get_one(PieView *pieview, int idx)
{
    Pie *pie = list_idx_get_data(pieview->pies, idx);
    return pie->one;
}

void
_pieview_move(PieView *pieview, int x, int y)
{
    List *l;
    Pie *pie;
    LIST_FOR_EACH(pieview->pies, l, pie) {
        nemotale_path_translate(pie->one, x, y);
    }
    pieview->dirty = true;
}

void
_pieview_change_power(PieView *pieview, int idx, int pow)
{
    Pie *pie = list_idx_get_data(pieview->pies, idx);
    if (!pie) return;
    pie->power = pow;
    pieview->dirty = true;
}

void
_pieview_set_color(PieView *pieview, int idx, double r, double g, double b, double a)
{
    Pie *pie = list_idx_get_data(pieview->pies, idx);
    if (!pie) return;

    pie->r = r;
    pie->g = g;
    pie->b = b;
    pie->a = a;
    pieview->colored = true;
    pieview->dirty = true;
}

Pie *
_pieview_add_ap(PieView *pieview)
{
    Pie *pie;
    struct pathone *one;

    double endangle = M_PI * 2;
    int cnt = _pieview_count(pieview);
    if (cnt > 0) {
        Pie *last = pieview->pies->data;
        endangle = NTPATH_CIRCLE(last->one)->startangle;
    }

    pie = calloc(sizeof(Pie), 1);

    pie->r = (double)rand()/RAND_MAX;
    pie->g = (double)rand()/RAND_MAX;
    pie->b = (double)rand()/RAND_MAX;
    one = nemotale_path_create_circle(pieview->r);
    nemotale_path_set_data(one, pie);
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_donut_circle(one, pieview->ir);
    nemotale_path_set_fill_color(NTPATH_STYLE(one),
            pie->r, pie->g, pie->b, 0.0);
    nemotale_path_attach_one(pieview->group, one);
    nemotale_path_set_pie_circle(one, endangle, endangle);
    nemotale_path_set_anchor(one, -0.5f, -0.5f);
    pie->one = one;

    pieview->pies = list_data_insert(pieview->pies, pie);
    pieview->dirty = true;

    return pie;
}

void
_pieview_del_ap(PieView *pieview, Pie *pie)
{
    pieview->pies = list_data_remove(pieview->pies, pie);
    pieview->pies_del = list_data_insert(pieview->pies_del, pie);
    nemotale_path_set_data(pie->one, NULL);
    pieview->dirty = true;
}

void
_pieview_update_end(struct taletransition *trans, struct nemoobject *obj)
{
	PieView *pieview = nemoobject_igetp(obj, 0);

    List *l, *ll;
    Pie *pie;
    LIST_FOR_EACH_SAFE(pieview->pies_del, l, ll, pie) {
        pieview->pies_del = list_data_remove(pieview->pies_del, pie);
        nemotale_path_destroy_one(pie->one);
        free(pie);
    }
}

void
_pieview_update(PieView *pieview, _NemoWin *win, struct taletransition *trans, double delay, double to)
{
    if (!pieview->dirty) return;

    int pow = 0;

    List *l;
    Pie *pie;

    LIST_FOR_EACH(pieview->pies, l, pie) {
        pow += pie->power;
    }
    if (pow <= 0) return;

    double start = 0;
    LIST_FOR_EACH_REVERSE(pieview->pies, l, pie) {
        double end = ((double)pie->power/pow) * M_PI * 2 + start;

        if (pieview->resized) {
            if (delay != 0) {
                nemotale_transition_attach_dattr(trans,
                        NTPATH_CIRCLE_ATR(pie->one), delay,
                        NTPATH_CIRCLE(pie->one)->outr);
                nemotale_transition_attach_dattr(trans,
                        NTPATH_CIRCLE_ATINNERRADIUS(pie->one), delay,
                        NTPATH_CIRCLE(pie->one)->inr);
            }
            nemotale_transition_attach_dattr(trans,
                    NTPATH_CIRCLE_ATR(pie->one), to, pieview->r);
            nemotale_transition_attach_dattr(trans,
                    NTPATH_CIRCLE_ATINNERRADIUS(pie->one), to, pieview->ir);
        }
        nemotale_transition_attach_dattr(trans,
                NTPATH_CIRCLE_ATSTARTANGLE(pie->one), to, start);
        nemotale_transition_attach_dattr(trans,
                NTPATH_CIRCLE_ATENDANGLE(pie->one), to, end);

       if (pieview->colored) {
            if (delay != 0) {
                struct pathpaint pp = NTPATH_STYLE(pie->one)->fill_paint;
                nemotale_transition_attach_dattrs(trans,
                        NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(pie->one)),
                        4, delay, pp.u.rgba[0], pp.u.rgba[1],
                        pp.u.rgba[2], pp.u.rgba[3]);
            }
           nemotale_transition_attach_dattrs(trans,
                   NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(pie->one)),
                   4, to, pie->r, pie->g, pie->b, pie->a);
       }
       nemotale_transition_attach_signal(trans,
               NTPATH_DESTROY_SIGNAL(pie->one));
       _nemotale_transition_transform(trans, pie->one);

        start = end;
    }
    if (pieview->resized) pieview->resized = false;
    if (pieview->colored) pieview->colored = false;

    // Remove animation
    LIST_FOR_EACH_REVERSE(pieview->pies_del, l, pie) {
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(pie->one)),
                4, to, pie->r, pie->g, pie->b, 0.0f);
        nemotale_transition_attach_signal(trans,
                NTPATH_DESTROY_SIGNAL(pie->one));
    }
    nemotale_transition_attach_event(trans, NEMOTALE_TRANSITION_EVENT_END,
            _pieview_update_end, "p", pieview);
}
