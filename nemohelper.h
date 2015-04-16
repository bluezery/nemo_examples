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

static void nemoclip_dispatch_canvas_frame(struct nemocanvas *canvas, uint64_t secs, uint32_t nsecs)
{
	struct nemotale *tale = nemocanvas_get_userdata(canvas);

	if (secs == 0 && nsecs == 0) {
		nemocanvas_feedback(canvas);
		nemocanvas_commit(canvas);
	} else if (nemotale_has_transition(tale) != 0) {
		nemocanvas_buffer(canvas);
		nemotale_detach_pixman(tale);
		nemotale_attach_pixman(tale,
				nemocanvas_get_data(canvas),
				nemocanvas_get_width(canvas),
				nemocanvas_get_height(canvas),
				nemocanvas_get_stride(canvas));

		nemotale_update_transition(tale, secs * 1000 + nsecs / 1000000);

		nemotale_composite(tale, NULL);

		nemocanvas_damage(canvas, 0, 0, 0, 0);
		nemocanvas_feedback(canvas);
		nemocanvas_commit(canvas);
	} else {
		nemocanvas_buffer(canvas);
		nemotale_detach_pixman(tale);
		nemotale_attach_pixman(tale,
				nemocanvas_get_data(canvas),
				nemocanvas_get_width(canvas),
				nemocanvas_get_height(canvas),
				nemocanvas_get_stride(canvas));

		nemotale_damage_all(tale);
		nemotale_composite(tale, NULL);

		nemocanvas_damage(canvas, 0, 0, 0, 0);
		nemocanvas_commit(canvas);
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
	    nemocanvas_set_dispatch_frame(canvas, nemoclip_dispatch_canvas_frame);
	    //nemocanvas_set_anchor(canvas, -0.5f, -0.5f);

        nemocanvas_buffer(canvas);

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
_win_set_userdata(_Win *win, void *userdata)
{
    win->userdata = userdata;
}

static inline void *
_win_get_userdata(_Win *win)
{
    return win->userdata;
}

/*******************************/
/***** TRANSITION **************/
/*******************************/
static inline struct taletransition *
_win_trans_create(_Win *win, int delay, int duration)
{
    struct taletransition *trans;
    trans = nemotale_transition_create(delay, duration);

    return trans;
}

static inline void
_win_trans_damage(_Win *win, struct taletransition *trans, struct talenode *node, struct pathone *one)
{
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_node_handle_path_damage_event, "pp", node, one);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_damage_event, "pp", node, one);
}


static inline void
_win_trans_transform(_Win *win, struct taletransition *trans, struct pathone *one)
{
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event, "p", one);
    nemotale_path_transform_enable(one);
    nemotale_transition_attach_signal(trans,
            NTPATH_DESTROY_SIGNAL(one));
}

static inline void
_win_trans_render(_Win *win, struct taletransition *trans, struct talenode *node, struct pathone *one)
{
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_update_event, "p", one);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_render_event, "pp", node, one);
}

static inline void
_win_trans_do(_Win *win, struct taletransition *trans)
{
    struct nemotale *tale = _win_get_tale(win);
    struct nemocanvas *canvas = _win_get_canvas(win);

    nemotale_dispatch_transition(tale, trans);
    nemocanvas_dispatch_frame(canvas);
}

static inline void
_win_trans_add_event_end(_Win *win, struct taletransition *trans, nemotale_transition_dispatch_t callback, void *ctx, void *data)
{
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_END,
            callback, ctx, data);
}

#endif
