#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

//#include <wayland-presentation-timing-client-protocol.h>

#include "talehelper.h"
#include <nemomisc.h>

struct taleframe {
	struct presentation *presentation;
	struct presentation_feedback *feedback;

	struct taletransition *transition;
	struct nemocanvas *canvas;
};

void nemotale_handle_canvas_update_event(struct taletransition *trans, void *context, void *data)
{
	struct nemocanvas *canvas = (struct nemocanvas *)context;
	struct nemotale *tale = (struct nemotale *)data;

	nemocanvas_flip(canvas);
	nemocanvas_clear(canvas);
	nemotale_detach_pixman(tale);
	nemotale_attach_pixman(tale,
			nemocanvas_get_data(canvas),
			nemocanvas_get_width(canvas),
			nemocanvas_get_height(canvas),
			nemocanvas_get_stride(canvas));
}

void nemotale_handle_canvas_flush_event(struct taletransition *trans, void *context, void *data)
{
	struct nemocanvas *canvas = (struct nemocanvas *)context;

	nemocanvas_damage(canvas, 0, 0, 0, 0);
	nemocanvas_commit(canvas);
}

static void nemotale_handle_canvas_transition_destroy(struct nemolistener *listener, void *data)
{
	struct talesensor *sensor = (struct talesensor *)container_of(listener, struct talesensor, listener);
	struct taleframe *frame = (struct taleframe *)sensor->data;

	if (frame->feedback != NULL) {
		presentation_feedback_destroy(frame->feedback);
	}
	free(frame);

	nemolist_remove(&sensor->listener.link);
	free(sensor);
}

static void presentation_feedback_sync_output(void *data, struct presentation_feedback *feedback, struct wl_output *output)
{
}

static const struct presentation_feedback_listener presentation_feedback_listener;

static void presentation_feedback_presented(void *data, struct presentation_feedback *feedback,
		uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec, uint32_t refresh,
		uint32_t seq_hi, uint32_t seq_lo, uint32_t flags)
{
	struct taleframe *frame = (struct taleframe *)data;

	if (frame->feedback != NULL) {
		presentation_feedback_destroy(frame->feedback);

		frame->feedback = NULL;
	}

	if (nemotale_transition_update(frame->transition, time_current_msecs()) == 0) {
		frame->feedback = presentation_feedback(frame->presentation, nemocanvas_get_surface(frame->canvas));
		presentation_feedback_add_listener(frame->feedback, &presentation_feedback_listener, frame);

		nemocanvas_damage(frame->canvas, 0, 0, 0, 0);
		nemocanvas_commit(frame->canvas);
	}
}

static void presentation_feedback_discarded(void *data, struct presentation_feedback *feedback)
{
	struct taleframe *frame = (struct taleframe *)data;

	if (frame->feedback != NULL) {
		presentation_feedback_destroy(frame->feedback);

		frame->feedback = NULL;
	}
}

static const struct presentation_feedback_listener presentation_feedback_listener = {
	presentation_feedback_sync_output,
	presentation_feedback_presented,
	presentation_feedback_discarded
};

void nemotale_dispatch_canvas_transition(struct nemocanvas *canvas, struct taletransition *trans)
{
	if (nemotale_transition_update(trans, time_current_msecs()) == 0) {
		struct nemotool *tool = nemocanvas_get_tool(canvas);
		struct presentation *presentation = nemotool_get_presentation(tool);
		struct talesensor *sensor;
		struct taleframe *frame;

		frame = (struct taleframe *)malloc(sizeof(struct taleframe));
		frame->canvas = canvas;
		frame->presentation = presentation;
		frame->transition = trans;
		frame->feedback = presentation_feedback(presentation, nemocanvas_get_surface(canvas));
		presentation_feedback_add_listener(frame->feedback, &presentation_feedback_listener, frame);

		nemocanvas_damage(canvas, 0, 0, 0, 0);
		nemocanvas_commit(canvas);

		sensor = (struct talesensor *)malloc(sizeof(struct talesensor));
		sensor->data = frame;
		sensor->listener.notify = nemotale_handle_canvas_transition_destroy;
		nemosignal_add(NTTRANS_DESTROY_SIGNAL(trans), &sensor->listener);
	}
}

static int nemotale_dispatch_canvas_event(struct nemocanvas *canvas, uint32_t type, struct nemoevent *event)
{
	struct nemotale *tale = (struct nemotale *)nemocanvas_get_userdata(canvas);

	if (type & NEMOTOOL_POINTER_ENTER_EVENT) {
		nemotale_push_pointer_enter_event(tale, event->serial, event->device, event->x, event->y);
	} else if (type & NEMOTOOL_POINTER_LEAVE_EVENT) {
		nemotale_push_pointer_leave_event(tale, event->serial, event->device);
	} else if (type & NEMOTOOL_POINTER_MOTION_EVENT) {
		nemotale_push_pointer_motion_event(tale, event->serial, event->device, event->time, event->x, event->y);
	} else if (type & NEMOTOOL_POINTER_BUTTON_EVENT) {
		if (event->state == WL_POINTER_BUTTON_STATE_PRESSED)
			nemotale_push_pointer_down_event(tale, event->serial, event->device, event->time, event->value);
		else
			nemotale_push_pointer_up_event(tale, event->serial, event->device, event->time, event->value);
	} else if (type & NEMOTOOL_POINTER_AXIS_EVENT) {
	} else if (type & NEMOTOOL_KEYBOARD_ENTER_EVENT) {
		nemotale_push_keyboard_enter_event(tale, event->serial, event->device);
	} else if (type & NEMOTOOL_KEYBOARD_LEAVE_EVENT) {
		nemotale_push_keyboard_leave_event(tale, event->serial, event->device);
	} else if (type & NEMOTOOL_KEYBOARD_KEY_EVENT) {
		if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED)
			nemotale_push_keyboard_down_event(tale, event->serial, event->device, event->time, event->value);
		else
			nemotale_push_keyboard_up_event(tale, event->serial, event->device, event->time, event->value);
	} else if (type & NEMOTOOL_KEYBOARD_MODIFIERS_EVENT) {
	} else if (type & NEMOTOOL_TOUCH_DOWN_EVENT) {
		nemotale_push_touch_down_event(tale, event->serial, event->device, event->time, event->x, event->y);
	} else if (type & NEMOTOOL_TOUCH_UP_EVENT) {
		nemotale_push_touch_up_event(tale, event->serial, event->device, event->time, event->x, event->y);
	} else if (type & NEMOTOOL_TOUCH_MOTION_EVENT) {
		nemotale_push_touch_motion_event(tale, event->serial, event->device, event->time, event->x, event->y);
	}
    return 0;
}

void nemotale_attach_canvas(struct nemotale *tale, struct nemocanvas *canvas, nemotale_dispatch_event_t dispatch)
{
	nemocanvas_set_dispatch_event(canvas, nemotale_dispatch_canvas_event);
	nemocanvas_set_userdata(canvas, tale);

	nemotale_set_dispatch_event(tale, dispatch);
}
