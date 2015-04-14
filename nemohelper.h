#ifndef	__NEMO_HELPER_H__
#define	__NEMO_HELPER_H__

#include <nemotool.h>
#include <nemocanvas.h>
#include <nemoegl.h>
#include <nemotale.h>
#include <talenode.h>
#include <taletransition.h>
#include <talemisc.h>
#include "talehelper.h"

/*******************************/
/***** TRANSITION **************/
/*******************************/
static inline struct taletransition *
_transit_create(struct nemocanvas *canvas, int delay, int duration, uint32_t type)
{
    struct nemotale *tale =nemocanvas_get_userdata(canvas);
    struct taletransition *trans;
    trans = nemotale_transition_create(delay, duration);
    nemotale_transition_attach_timing(trans, 1.0f, type);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_handle_canvas_update_event, canvas, tale);

    return trans;
}

static inline void
_transit_add_event_start(struct taletransition *trans, nemotale_transition_dispatch_t callback, void *ctx, void *data)
{
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_START,
            callback, ctx, data);
}

static inline void
_transit_add_event_end(struct taletransition *trans, nemotale_transition_dispatch_t callback, void *ctx, void *data)
{
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_END,
            callback, ctx, data);
}

static inline void
_transit_damage_path(struct taletransition *trans, struct talenode *node, struct pathone *one)
{
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_node_handle_path_damage_event, node, one);
}

static inline void
_transit_transform_path(struct taletransition *trans, struct talenode *node, struct pathone *one)
{
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event, NULL, one);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_update_event, NULL, one);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_damage_event, node, one);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_clip_and_render_event, node, one);
}

static inline void
_transit_go(struct taletransition *trans, struct nemocanvas *canvas)
{
    struct nemotale *tale =nemocanvas_get_userdata(canvas);

    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_composite_event, tale, NULL);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_canvas_flush_event, canvas, tale);

    nemotale_dispatch_canvas_transition(canvas, trans);
}


/*******************************/
/***** WINDOW     **************/
/*******************************/
typedef enum
{
    WIN_TYPE_PIXMAN,
    WIN_TYPE_EGL
} _WinType;

typedef struct _Win {
    _WinType type;
    int w, h;
    struct nemotale *tale;
    struct nemotool *tool;
    void *userdata;

    // Pixman
    struct nemocanvas *canvas;

    // Egl
    struct eglcontext *egl;
    struct eglcanvas *egl_canvas;

} _Win;

static inline void
_win_destroy(_Win *win)
{
    nemotale_destroy(win->tale);
    if (win->type == WIN_TYPE_EGL) {
        nemotool_destroy_egl_canvas(win->egl_canvas);
        nemotool_destroy_egl(win->egl);
    } else {
        nemocanvas_destroy(win->canvas);
    }
}

static inline _Win *
_win_create(struct nemotool *tool, _WinType type, int w, int h, nemotale_dispatch_event_t _event_cb)
{
    _Win *win;
    win = calloc(sizeof(_Win), 1);
    win->tool = tool;
    win->type = type;
    win->w = w;
    win->h = h;

    struct nemotale *tale;
    if (type == WIN_TYPE_EGL)
    {
        struct eglcontext *egl;
        struct eglcanvas *egl_canvas;

        egl = nemotool_create_egl(tool);
        egl_canvas = nemotool_create_egl_canvas(egl, w, h);
        nemocanvas_set_nemosurface(NTEGL_CANVAS(egl_canvas), NEMO_SHELL_SURFACE_TYPE_NORMAL);

        tale = nemotale_create_egl(
                NTEGL_DISPLAY(egl),
                NTEGL_CONTEXT(egl),
                NTEGL_CONFIG(egl));
        nemotale_attach_egl(tale, (EGLNativeWindowType)NTEGL_WINDOW(egl_canvas));
        nemotale_resize_egl(tale, w, h);
        nemotale_attach_canvas(tale, NTEGL_CANVAS(egl_canvas), _event_cb);
        win->egl = egl;
        win->egl_canvas = egl_canvas;
    } else {
        struct nemocanvas *canvas;

        canvas = nemocanvas_create(tool);
        nemocanvas_set_size(canvas, w, h);
        nemocanvas_set_nemosurface(canvas, NEMO_SHELL_SURFACE_TYPE_NORMAL);

        nemocanvas_flip(canvas);
        nemocanvas_clear(canvas);

        tale = nemotale_create_pixman();
        nemotale_attach_pixman(tale,
                nemocanvas_get_data(canvas),
                nemocanvas_get_width(canvas),
                nemocanvas_get_height(canvas),
                nemocanvas_get_stride(canvas));
        nemotale_attach_canvas(tale, canvas, _event_cb);
        win->canvas = canvas;
    }
    win->tale = tale;
    return win;
}

static inline struct nemocanvas *
_win_get_canvas(_Win *win)
{
    if (win->type == WIN_TYPE_EGL) {
        return NTEGL_CANVAS(win->egl_canvas);
    } else {
        return win->canvas;
    }
}

static inline struct nemotale *
_win_get_tale(_Win *win)
{
    return win->tale;
}

static inline struct nemotool *
_win_get_tool(_Win *win)
{
    return win->tool;
}

static inline void
_win_render_prepare(_Win *win)
{
    if (win->type == WIN_TYPE_PIXMAN) {
        nemotale_handle_canvas_update_event(NULL, win->canvas, win->tale);
    }
}

static inline void
_win_render(_Win *win)
{
    nemotale_composite(win->tale, NULL);
    if (win->type == WIN_TYPE_PIXMAN) {
        nemotale_handle_canvas_flush_event(NULL, win->canvas, NULL);
    }
}

static inline void
_win_set_userdata(_Win *win, void *userdata)
{
    win->userdata = userdata;
}

static inline void *
_win_get_userdata(_Win *win)
{
    return win->userdata;
}

static inline struct taletransition *
_win_trans_create(_Win *win, int delay, int duration, uint32_t type)
{
    struct nemocanvas *canvas = _win_get_canvas(win);
    struct nemotale *tale = _win_get_tale(win);
    struct taletransition *trans;
    trans = nemotale_transition_create(delay, duration);
    nemotale_transition_attach_timing(trans, 1.0f, type);
    if (win->type == WIN_TYPE_PIXMAN) {
        nemotale_transition_attach_event(trans,
                NEMOTALE_TRANSITION_EVENT_PREUPDATE,
                nemotale_handle_canvas_update_event, canvas, tale);
    }

    return trans;
}

static inline void
_win_trans_damage(_Win *win, struct taletransition *trans, struct talenode *node, struct pathone *one)
{
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_node_handle_path_damage_event, node, one);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_damage_event, node, one);
}


static inline void
_win_trans_transform(_Win *win, struct taletransition *trans, struct pathone *one)
{
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event, NULL, one);
    nemotale_path_transform_enable(one);
    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(one));
}

static inline void
_win_trans_render(_Win *win, struct taletransition *trans, struct talenode *node, struct pathone *one)
{
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_update_event, NULL, one);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_clip_and_render_event, node, one);
}

static inline void
_win_trans_do(_Win *win, struct taletransition *trans)
{
    struct nemotale *tale = _win_get_tale(win);
    struct nemocanvas *canvas = _win_get_canvas(win);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_composite_event, tale, NULL);
    nemotale_dispatch_canvas_transition(canvas, trans);
}

static inline void
_win_trans_add_event_end(_Win *win, struct taletransition *trans, nemotale_transition_dispatch_t callback, void *ctx, void *data)
{
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_END,
            callback, ctx, data);
}

#endif
