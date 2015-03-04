#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <talehelper.h>
#include <mischelper.h>

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

static void nemotale_handle_transition_timer_event(struct nemotimer *timer, void *data)
{
	struct taletransition *trans = (struct taletransition *)data;

	if (nemotale_transition_update(trans, time_current_msecs()) == 0)
		nemotimer_set_timeout(timer, 1000 / 60);
}

static void nemotale_handle_transition_destroy(struct nemolistener *listener, void *data)
{
	struct talesensor *sensor = (struct talesensor *)container_of(listener, struct talesensor, listener);

	nemotimer_destroy((struct nemotimer *)sensor->data);

	nemolist_remove(&sensor->listener.link);

	free(sensor);
}

void nemotale_dispatch_transition_timer_event(struct nemotool *tool, struct taletransition *trans)
{
	struct talesensor *sensor;
	struct nemotimer *timer;

	timer = nemotimer_create(tool);
	nemotimer_set_callback(timer, nemotale_handle_transition_timer_event);
	nemotimer_set_userdata(timer, trans);
	nemotimer_set_timeout(timer, 1000 / 60);

	sensor = (struct talesensor *)malloc(sizeof(struct talesensor));
	sensor->data = timer;
	sensor->listener.notify = nemotale_handle_transition_destroy;
	nemosignal_add(NTTRANS_DESTROY_SIGNAL(trans), &sensor->listener);
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
}

void nemotale_attach_canvas(struct nemotale *tale, struct nemocanvas *canvas, nemotale_dispatch_event_t dispatch)
{
	nemocanvas_set_dispatch_event(canvas, nemotale_dispatch_canvas_event);
	nemocanvas_set_userdata(canvas, tale);

	nemotale_set_dispatch_event(tale, dispatch);
}
