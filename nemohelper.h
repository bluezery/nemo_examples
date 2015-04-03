#ifndef	__NEMO_HELPER_H__
#define	__NEMO_HELPER_H__

#include <nemotool.h>
#include <nemocanvas.h>
#include <nemotale.h>
#include <talenode.h>
#include <taletransition.h>
#include <talemisc.h>
#include "talehelper.h"

static inline struct taletransition *
_transit_create(struct nemocanvas *canvas, int delay, int duration, uint32_t type)
{
    struct nemotale *tale =nemocanvas_get_userdata(canvas);
    struct taletransition *trans;
    trans = nemotale_transition_create(delay, duration);
    nemotale_transition_attach_timing(trans, 1.0, type);
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
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_damage_event, node, one);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_clip_and_render_event, node, one);
}

static inline void
_transit_transform_path(struct taletransition *trans, struct pathone *one)
{
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event, NULL, one);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_update_event, NULL, one);
}

static inline void
_transit_go(struct taletransition *trans, struct nemocanvas *canvas)
{
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    struct nemotale *tale =nemocanvas_get_userdata(canvas);

    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_composite_event, tale, NULL);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_canvas_flush_event, canvas, tale);

    nemotale_dispatch_transition_timer_event(tool, trans);
}

#endif
