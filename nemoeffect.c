#include <nemotool.h>
#include <nemocanvas.h>
#include <nemotale.h>
#include <talenode.h>
#include <talepixman.h>

#include <math.h>
#include <pathpaint.h>
#include "talehelper.h"
//#include <pathshape.h>
//#include <pathstyle.h>
//
//#include <talemisc.h>
//#include <taleevent.h>
//#include <talegesture.h>
//
//
#include "util.h"

struct Context {
    struct nemocanvas *canvas;
    struct talenode *node;

    struct pathone *group;
    struct pathone *one_bg;
    double one_stroke_w;
    struct pathone *one_circle, *one_rect, *one_path, *one_text;
    struct pathone *one_clip;
    int w, h;

    int fade_state;
    int zoom_state;
    int move_state;
    int wipe_state;
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
    struct Context *ctx = _ctx;
    if (ctx->fade_state == 1) ctx->fade_state = 2;
    else if (ctx->fade_state == -1) ctx->fade_state = -2;
    else ERR("something wrong!!");
}

static void
_fade_begin(struct Context *ctx)
{
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    struct nemotale *tale =nemocanvas_get_userdata(canvas);
    struct taletransition *trans;

    if ((ctx->fade_state == 1) || (ctx->fade_state == -1)) {
        ERR("fading is already stared");
        return;
    }
    trans = nemotale_transition_create(0, 1500);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_handle_canvas_update_event, canvas, tale);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_node_handle_path_damage_event,
            ctx->node, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_damage_event,
            ctx->node, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event,
            NULL, ctx->one_circle);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event,
            NULL, ctx->one_path);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event,
            NULL, ctx->one_rect);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event,
            NULL, ctx->one_text);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_update_event,
            NULL, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_render_event,
            ctx->node, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_composite_event, tale, NULL);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_canvas_flush_event, canvas, tale);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_END,
            _fade_end, ctx, NULL);

    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(ctx->one_circle));
    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(ctx->one_path));
    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(ctx->one_rect));
    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(ctx->one_text));

    if (ctx->fade_state > 0) { // fade out start
        nemotale_transition_attach_timing(trans, 1.0,
                NEMOEASE_CUBIC_OUT_TYPE);
        /* FIXME: opacity cannot remember RGB state
        nemotale_transition_attach_dattr(trans,
                NTSTYLE_FILL_ATOPACITY(NTPATH_STYLE(ctx->one_circle)),
                0.4f, 0.0f);*/
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one_circle)),
                4, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one_path)),
                4, 0.2f, 0.0f, 1.0f, 0.0f, 0.5f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one_path)),
                4, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one_rect)),
                4, 0.4f, 0.0f, 1.0f, 0.0f, 0.5f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one_rect)),
                4, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one_text)),
                4, 0.6f, 1.0f, 1.0f, 0.0f, 0.5f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one_text)),
                4, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        ctx->fade_state = -1;
    } else {                    // fade in start
        nemotale_transition_attach_timing(trans, 1.0,
                NEMOEASE_CUBIC_IN_TYPE);
        /* FIXME: opacity cannot remember RGB state
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATOPACITY(NTPATH_STYLE(ctx->one_circle)),
                0.4f, 1.0f);*/
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one_circle)),
                4, 0.4f, 1.0f, 0.0f, 0.0f, 0.5f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one_path)),
                4, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one_path)),
                4, 0.6f, 0.0f, 1.0f, 0.0f, 0.5f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one_rect)),
                4, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one_rect)),
                4, 0.8f, 0.0f, 0.0f, 1.0f, 0.5f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one_text)),
                4, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f);
        nemotale_transition_attach_dattrs(trans,
                NTSTYLE_FILL_ATCOLOR(NTPATH_STYLE(ctx->one_text)),
                4, 1.0f, 1.0f, 1.0f, 0.0f, 0.5f);
        ctx->fade_state = 1;
    }

    nemotale_dispatch_transition_timer_event(tool, trans);
}

static void
_zoom_end(struct taletransition *trans, void *_ctx, void *data)
{
    struct Context *ctx = _ctx;
    if (ctx->zoom_state == 1) ctx->zoom_state = 2;
    else if (ctx->zoom_state == -1) ctx->zoom_state = -2;
    else ERR("something wrong!!");
}

static void
_zoom_begin(struct Context *ctx)
{
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    struct nemotale *tale =nemocanvas_get_userdata(canvas);
    struct taletransition *trans;

    if ((ctx->zoom_state == 1) || (ctx->zoom_state == -1)) {
        ERR("zooming is already stared");
        return;
    }
    trans = nemotale_transition_create(0, 1000);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_handle_canvas_update_event, canvas, tale);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_node_handle_path_damage_event,
            ctx->node, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_damage_event,
            ctx->node, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event,
            NULL, ctx->one_circle);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event,
            NULL, ctx->one_path);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event,
            NULL, ctx->one_rect);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event,
            NULL, ctx->one_text);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_update_event,
            NULL, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_render_event,
            ctx->node, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_composite_event, tale, NULL);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_canvas_flush_event, canvas, tale);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_END,
            _zoom_end, ctx, NULL);

    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(ctx->one_circle));
    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(ctx->one_path));
    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(ctx->one_rect));
    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(ctx->one_text));

    if (ctx->zoom_state > 0) { // zoom out start
        ERR("zoom out")
        nemotale_transition_attach_timing(trans, 1.0,
                NEMOEASE_CUBIC_OUT_TYPE);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATSX(ctx->one_circle),
                1.0f, 1);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATSY(ctx->one_circle),
                1.0f, 1);

        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATSX(ctx->one_path),
                1.0f, 1);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATSY(ctx->one_path),
                1.0f, 1);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATX(ctx->one_path),
                1.0f, 110);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(ctx->one_path),
                1.0f, 10);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATSX(ctx->one_rect),
                1.0f, 1);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATSY(ctx->one_rect),
                1.0f, 1);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATX(ctx->one_rect),
                1.0f, 10);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(ctx->one_rect),
                1.0f, 110);

        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATSX(ctx->one_text),
                1.0f, 1);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATSY(ctx->one_text),
                1.0f, 1);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATX(ctx->one_text),
                1.0f, 110);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(ctx->one_text),
                1.0f, 110);

        ctx->zoom_state = -1;
    } else {                    // zoom in start
        ERR("zoom in")
        nemotale_transition_attach_timing(trans, 1.0,
                NEMOEASE_CUBIC_IN_TYPE);

        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATSX(ctx->one_circle),
                1.0f, 1.5);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATSY(ctx->one_circle),
                1.0f, 1.5);

        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATSX(ctx->one_path),
                1.0f, 1.5);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATSY(ctx->one_path),
                1.0f, 1.5);

        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATSX(ctx->one_rect),
                1.0f, 1.5);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATSY(ctx->one_rect),
                1.0f, 1.5);

        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATSX(ctx->one_text),
                1.0f, 1.5);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATSY(ctx->one_text),
                1.0f, 1.5);
        ctx->zoom_state = 1;
    }

    nemotale_dispatch_transition_timer_event(tool, trans);
}

static void
_move_end(struct taletransition *trans, void *_ctx, void *data)
{
    struct Context *ctx = _ctx;
    if (ctx->move_state == 1) ctx->move_state = 2;
    else if (ctx->move_state == -1) ctx->move_state = -2;
    else ERR("something wrong!!");
}

static void
_move_begin(struct Context *ctx)
{
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    struct nemotale *tale =nemocanvas_get_userdata(canvas);
    struct taletransition *trans;

    if ((ctx->move_state == 1) || (ctx->move_state == -1)) {
        ERR("move is already stared");
        return;
    }
    trans = nemotale_transition_create(0, 1000);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_handle_canvas_update_event, canvas, tale);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_node_handle_path_damage_event,
            ctx->node, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_damage_event,
            ctx->node, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event,
            NULL, ctx->one_circle);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event,
            NULL, ctx->one_path);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event,
            NULL, ctx->one_rect);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event,
            NULL, ctx->one_text);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_update_event,
            NULL, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_render_event,
            ctx->node, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_composite_event, tale, NULL);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_canvas_flush_event, canvas, tale);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_END,
            _move_end, ctx, NULL);

    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(ctx->one_circle));
    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(ctx->one_path));
    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(ctx->one_rect));
    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(ctx->one_text));

    if (ctx->move_state > 0) { // move out start
        ERR("move out")
        nemotale_transition_attach_timing(trans, 1.0,
                NEMOEASE_CUBIC_OUT_TYPE);

        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATX(ctx->one_circle),
                0.4f, 10);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(ctx->one_path),
                0.2f, 110);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(ctx->one_path),
                0.6f, 10);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(ctx->one_rect),
                0.4f, 10);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(ctx->one_rect),
                0.8f, 110);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATX(ctx->one_text),
                0.6f, 10);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATX(ctx->one_text),
                1.0f, 110);

        ctx->move_state = -1;
    } else {                    // move in start
        ERR("move in");
        nemotale_transition_attach_timing(trans, 1.0,
                NEMOEASE_CUBIC_IN_TYPE);

        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATX(ctx->one_circle),
                0.4f, 110);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(ctx->one_path),
                0.2f, 10);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(ctx->one_path),
                0.6f, 110);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(ctx->one_rect),
                0.4f, 110);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(ctx->one_rect),
                0.8f, 10);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATX(ctx->one_text),
                0.6f, 110);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATX(ctx->one_text),
                1.0f, 10);

        ctx->move_state = 1;
    }

    nemotale_dispatch_transition_timer_event(tool, trans);
}

static void
_wipe_end(struct taletransition *trans, void *_ctx, void *data)
{
    struct Context *ctx = _ctx;
    if (ctx->wipe_state == 1) ctx->wipe_state = 2;
    else if (ctx->wipe_state == -1) ctx->wipe_state = -2;
    else ERR("something wrong!!");
}

static void
_wipe_begin(struct Context *ctx)
{
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    struct nemotale *tale =nemocanvas_get_userdata(canvas);
    struct taletransition *trans;

    if ((ctx->wipe_state == 1) || (ctx->wipe_state == -1)) {
        ERR("wipe is already stared");
        return;
    }
    trans = nemotale_transition_create(0, 2000);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_handle_canvas_update_event, canvas, tale);
#if 0
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_node_handle_path_damage_event,
            ctx->node, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_node_handle_path_damage_event,
            ctx->node, ctx->one_clip);
#endif

#if 1
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event,
            NULL, ctx->one_clip);
#endif
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_update_event,
            NULL, ctx->one_clip);
#if 0
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_damage_event,
            ctx->node, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_update_event,
            NULL, ctx->group);
#endif
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_damage_event,
            ctx->node, ctx->one_clip);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_clip_and_render_event,
            ctx->node, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_composite_event, tale, NULL);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_canvas_flush_event, canvas, tale);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_END,
            _wipe_end, ctx, NULL);

    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(ctx->one_clip));

    if (ctx->wipe_state > 0) { // wipe out start
        ERR("wipe out")
        nemotale_transition_attach_timing(trans, 1.0,
                NEMOEASE_CUBIC_OUT_TYPE);
        nemotale_transition_attach_timing(trans, 1.0,
                NEMOEASE_CUBIC_IN_TYPE);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATX(ctx->one_clip),
                0.3f, 10);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(ctx->one_clip),
                0.3f, 110);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATX(ctx->one_clip),
                0.5f, 110);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATX(ctx->one_clip),
                0.7f, 10);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(ctx->one_clip),
                0.7f, 10);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATX(ctx->one_clip),
                1.0f, 0);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(ctx->one_clip),
                1.0f, 0);

        ctx->wipe_state = -1;
    } else {                    // wipe in start
        ERR("wipe in");
        nemotale_transition_attach_timing(trans, 1.0,
                NEMOEASE_CUBIC_IN_TYPE);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATX(ctx->one_clip),
                0.3f, 10);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(ctx->one_clip),
                0.3f, 10);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATX(ctx->one_clip),
                0.5f, 110);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATX(ctx->one_clip),
                0.7f, 10);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(ctx->one_clip),
                0.7f, 110);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATX(ctx->one_clip),
                1.0f, 110);
        nemotale_transition_attach_dattr(trans,
                NTPATH_TRANSFORM_ATY(ctx->one_clip),
                1.0f, 110);

        ctx->wipe_state = 1;
    }

    nemotale_dispatch_transition_timer_event(tool, trans);
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
            if (one) ERR("%s", NTPATH_ID(one));
            if (one && !strcmp(NTPATH_ID(one), "circle")) {
                _fade_begin(ctx);
            } else if (one && !strcmp(NTPATH_ID(one), "rect")) {
                _zoom_begin(ctx);
            } else if (one && !strcmp(NTPATH_ID(one), "path")) {
                _move_begin(ctx);
            } else if (one && !strcmp(NTPATH_ID(one), "text")) {
                _wipe_begin(ctx);
            }
        }
        tap->item = NULL;
    }
}

int main()
{
    int w = 220, h = 220;
    struct Context *ctx = malloc(sizeof(struct Context));
    ctx->w = w;
    ctx->h = h;
    ctx->fade_state = 2; // fade in state
    ctx->zoom_state =-2; // zoom out stae
    ctx->wipe_state =-2;
    ctx->move_state = -2;

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
    nemotale_node_opaque(node, 0, 0, w, h);
    nemotale_attach_node(tale, node);
    ctx->node = node;

    struct pathone *one;
    struct pathone *group;
    group = nemotale_path_create_group();
    ctx->group = group;

    // Background
    one = nemotale_path_create_rect(w, h);
    nemotale_path_set_id(one, "bg");
    nemotale_path_set_operator(one, CAIRO_OPERATOR_SOURCE);
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_stroke_color(NTPATH_STYLE(one), 1, 1, 1, 1);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1, 1, 1, 0.5);
    nemotale_path_set_stroke_width(NTPATH_STYLE(one), 3);
    nemotale_path_attach_one(group, one);
    ctx->one_stroke_w = 3;
    ctx->one_bg = one;

    // Circle
    one = nemotale_path_create_circle(50);
    nemotale_path_set_id(one, "circle");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1, 0, 0, 0.5);
    nemotale_path_translate(one, 10, 10);
    nemotale_path_attach_one(group, one);
    ctx->one_circle = one;

    // Path Star
    one = nemotale_path_create_path
        ("M50 0 L15 100 L100 35 L0 35 L85 100 Z");
    nemotale_path_set_id(one, "path");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 1, 1, 0, 0.5);
    nemotale_path_translate(one, 110, 10);
    nemotale_path_set_pivot_type(one, NEMOTALE_PATH_PIVOT_RATIO);
    nemotale_path_set_pivot(one, 1, 0);
    nemotale_path_attach_one(group, one);
    ctx->one_path = one;

    // Rect
    one = nemotale_path_create_rect(100, 100);
    nemotale_path_set_id(one, "rect");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0, 0, 1, 0.5);
    nemotale_path_translate(one, 10, 110);
    nemotale_path_set_pivot_type(one, NEMOTALE_PATH_PIVOT_RATIO);
    nemotale_path_set_pivot(one, 0, 1);
    nemotale_path_attach_one(group, one);
    ctx->one_rect = one;

    // Text
    one = nemotale_path_create_text();
    nemotale_path_set_id(one, "text");
    nemotale_path_attach_style(one, NULL);
	nemotale_path_set_font_family(NTPATH_STYLE(one),
            "LiberationMono");
	nemotale_path_set_font_size(NTPATH_STYLE(one), 100);
	nemotale_path_set_fill_color(NTPATH_STYLE(one), 1, 1, 0, 0.5);
    nemotale_path_translate(one, 110, 110);
    nemotale_path_set_pivot_type(one, NEMOTALE_PATH_PIVOT_RATIO);
    nemotale_path_set_pivot(one, 1, 1);
	nemotale_path_attach_one(group, one);
	nemotale_path_load_text(one, "A", 1);
    ctx->one_text = one;

    // Clip
    one = nemotale_path_create_rect(w, h);
    nemotale_path_set_id(one, "clip");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_translate(one, 0, 1);
    nemotale_path_translate(one, 0, -1);
    nemotale_path_set_clip(ctx->group, one);
    ctx->one_clip = one;

    nemotale_path_update_one(ctx->one_clip);
    nemotale_path_update_one(group);
    nemotale_node_render_path(node, group);
    nemotale_composite(tale, NULL);

    nemocanvas_damage(canvas, 0, 0, 0, 0);
    nemocanvas_commit(canvas);

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
