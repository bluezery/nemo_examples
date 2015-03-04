#ifndef	__NEMOUX_TALE_HELPER_H__
#define	__NEMOUX_TALE_HELPER_H__

#include <nemotool.h>
#include <nemocanvas.h>
#include <nemotimer.h>

#include <nemotale.h>
#include <talenode.h>
#include <talepixman.h>
#include <talegl.h>
#include <taleevent.h>
#include <talegrab.h>
#include <taletransition.h>
#include <talegesture.h>
#include <talemisc.h>
#include <pathshape.h>
#include <pathstyle.h>

extern void nemotale_handle_canvas_update_event(struct taletransition *trans, void *context, void *data);
extern void nemotale_handle_canvas_flush_event(struct taletransition *trans, void *context, void *data);

extern void nemotale_dispatch_transition_timer_event(struct nemotool *tool, struct taletransition *trans);

extern void nemotale_attach_canvas(struct nemotale *tale, struct nemocanvas *canvas, nemotale_dispatch_event_t dispatch);

#endif
