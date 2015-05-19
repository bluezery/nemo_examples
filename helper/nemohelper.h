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

#include "util.h"

/*******************************/
/***** WINDOW     **************/
/*******************************/
typedef enum
{
    WIN_TYPE_PIXMAN,
    WIN_TYPE_EGL
} NemoWinType;

typedef struct _NemoWin {
    NemoWinType type;

    int w, h;

    struct nemotool *tool;

    struct nemotale *tale;

    // Pixman
    struct nemocanvas *canvas;

    // Egl
    struct eglcontext *egl;
    struct eglcanvas *egl_canvas;

    struct talenode *node;
    struct pathone *group;
    struct pathone *bg;

    bool dirty;
    void *userdata;
} NemoWin;

static inline void
nemowin_destroy(NemoWin *win)
{
    nemotale_path_destroy_one(win->bg);
    nemotale_path_destroy_one(win->group);
    nemotale_node_destroy(win->node);

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
    NemoWin *win = nemotale_get_userdata(tale);

    // Cpuview Update timer
	if (secs == 0 && nsecs == 0) {
		nemocanvas_feedback(canvas);
		nemocanvas_commit(canvas);
	} else if (nemotale_has_transition(tale)) {

		nemocanvas_buffer(canvas);
		nemotale_detach_pixman(tale);
		nemotale_attach_pixman(tale,
				nemocanvas_get_data(canvas),
				nemocanvas_get_width(canvas),
				nemocanvas_get_height(canvas),
				nemocanvas_get_stride(canvas));

		nemotale_update_transition(tale, secs * 1000 + nsecs / 1000000);

        if (win->dirty) {
            nemotale_path_update_one(win->group);
            nemotale_node_render_path(win->node, win->group);
            win->dirty = false;
        }
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
        if (win->dirty) {
            nemotale_path_update_one(win->group);
            nemotale_node_render_path(win->node, win->group);
            win->dirty = false;
        }
		nemotale_composite(tale, NULL);

		nemocanvas_damage(canvas, 0, 0, 0, 0);
		nemocanvas_commit(canvas);
	}
}

static inline NemoWin *
nemowin_create(struct nemotool *tool, NemoWinType type, int w, int h, nemotale_dispatch_event_t _event_cb)
{
    NemoWin *win;
    win = calloc(sizeof(NemoWin), 1);
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
    nemotale_set_userdata(tale, win);

    struct talenode *node;
    node = nemotale_node_create_pixman(w, h);
    nemotale_node_set_id(node, 1);
    nemotale_attach_node(tale, node);
    nemotale_node_opaque(node, 0, 0, w, h);
    win->node = node;

    struct pathone *group;
    struct pathone *one;
    group = nemotale_path_create_group();
    win->group = group;

    // Background
    one = nemotale_path_create_rect(w, h);
    nemotale_path_set_id(one, "bg");
    nemotale_path_attach_style(one, NULL);
    //nemotale_path_set_anchor(one, -0.5f, -0.5f);
    nemotale_path_set_operator(one, CAIRO_OPERATOR_SOURCE);
    nemotale_path_set_fill_color(NTPATH_STYLE(one),
            1.0f, 1.0f, 1.0f, 0.0f);
    nemotale_path_attach_one(group, one);
    win->bg = one;

    return win;
}

static inline void
nemowin_set_bg_color(NemoWin *win, double r, double g, double b, double a)
{
    nemotale_path_set_fill_color(NTPATH_STYLE(win->bg), r, g, b, a);
}

static inline struct nemocanvas *
nemowin_get_canvas(NemoWin *win)
{
    if (win->type == WIN_TYPE_EGL) {
        return NTEGL_CANVAS(win->egl_canvas);
    } else {
        return win->canvas;
    }
}

static inline struct nemotale *
nemowin_get_tale(NemoWin *win)
{
    return win->tale;
}

static inline struct nemotool *
nemowin_get_tool(NemoWin *win)
{
    return win->tool;
}

static inline struct talenode *
nemowin_get_node(NemoWin *win)
{
    return win->node;
}

static inline struct pathone *
nemowin_get_group(NemoWin *win)
{
    return win->group;
}

static inline void
nemowin_set_userdata(NemoWin *win, void *userdata)
{
    win->userdata = userdata;
}

static inline void *
nemowin_get_userdata(NemoWin *win)
{
    return win->userdata;
}

static inline void
nemowin_dirty(NemoWin *win)
{
    struct nemocanvas *canvas = nemowin_get_canvas(win);

    win->dirty = true;
    nemocanvas_dispatch_frame(canvas);
}

static inline void
nemowin_show(NemoWin *win)
{
    nemotale_path_update_one(win->group);
    nemotale_node_render_path(win->node, win->group);
    nemotale_composite(win->tale, NULL);
}

static inline void
nemowin_set_surface_type(NemoWin *win, int type)
{
    struct nemocanvas *canvas = nemowin_get_canvas(win);
    nemocanvas_set_layer(canvas, type);
}

/*******************************/
/***** TRANSITION **************/
/*******************************/
static inline void
_nemotale_transition_damage(struct taletransition *trans, NemoWin *win)
{
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_node_handle_path_damage_event, "pp", win->node, win->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_damage_event, "pp", win->node, win->group);
}

static inline void
_nemotale_transition_transform(struct taletransition *trans, struct pathone *one)
{
    nemotale_path_transform_enable(one);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event, "p", one);
}

static inline void
_nemotale_transition_dispatch(struct taletransition *trans, NemoWin *win)
{
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_update_event, "p", win->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_render_event, "pp", win->node, win->group);
    nemotale_dispatch_transition(win->tale, trans);
    nemocanvas_dispatch_frame(win->canvas);
}

#endif
