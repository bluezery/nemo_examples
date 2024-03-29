#ifndef __NEMOTOOL_CANVAS_H__
#define	__NEMOTOOL_CANVAS_H__

#include <pixman.h>
#include <cairo.h>

#include <nemotool.h>
#include <nemolist.h>
#include <nemolistener.h>

struct nemocanvas;

typedef int (*nemocanvas_dispatch_event_t)(struct nemocanvas *canvas, uint32_t type, struct nemoevent *event);
typedef void (*nemocanvas_dispatch_resize_t)(struct nemocanvas *canvas, int32_t width, int32_t height);
typedef void (*nemocanvas_dispatch_frame_t)(struct nemocanvas *canvas, uint64_t secs, uint32_t nsecs);

struct nemobuffer {
	struct wl_buffer *buffer;
	char *shm_data;
	int shm_size;
	int busy;

	int width, height;
};

struct nemocanvas {
	struct nemotool *tool;

	struct wl_surface *surface;
	struct wl_subsurface *subsurface;
	struct nemo_surface *nemo_surface;

	struct presentation_feedback *feedback;

	struct nemocanvas *parent;

	int width, height;
	pixman_region32_t damage;

	struct nemotask repaint_task;
	struct nemobuffer buffers[2];
	struct nemobuffer *extras;
	struct nemobuffer *buffer;
	int nextras;

	nemocanvas_dispatch_event_t dispatch_event;
	nemocanvas_dispatch_resize_t dispatch_resize;
	nemocanvas_dispatch_frame_t dispatch_frame;

	struct nemosignal destroy_signal;

	void *userdata;
};

#define	NEMOCANVAS_DAMAGE(canvas)			(&canvas->damage)

extern struct nemocanvas *nemocanvas_create(struct nemotool *tool);
extern void nemocanvas_destroy(struct nemocanvas *canvas);

extern int nemocanvas_buffer(struct nemocanvas *canvas);
extern void nemocanvas_damage(struct nemocanvas *canvas, int32_t x, int32_t y, int32_t width, int32_t height);
extern void nemocanvas_damage_region(struct nemocanvas *canvas, pixman_region32_t *region);
extern void nemocanvas_feedback(struct nemocanvas *canvas);
extern void nemocanvas_commit(struct nemocanvas *canvas);

extern cairo_surface_t *nemocanvas_get_cairo_surface(struct nemocanvas *canvas);
extern pixman_image_t *nemocanvas_get_pixman_image(struct nemocanvas *canvas);

extern void nemocanvas_move(struct nemocanvas *canvas, uint32_t serial);
extern void nemocanvas_pick(struct nemocanvas *canvas, uint32_t serial0, uint32_t serial1, uint32_t type);
extern void nemocanvas_miss(struct nemocanvas *canvas);
extern void nemocanvas_follow(struct nemocanvas *canvas, int32_t x, int32_t y, int32_t degree, uint32_t delay, uint32_t duration);
extern void nemocanvas_execute(struct nemocanvas *canvas, const char *name, const char *cmds);
extern void nemocanvas_set_size(struct nemocanvas *canvas, int32_t width, int32_t height);
extern void nemocanvas_set_input(struct nemocanvas *canvas, int32_t x, int32_t y, int32_t width, int32_t height);
extern void nemocanvas_set_pivot(struct nemocanvas *canvas, int px, int py);
extern void nemocanvas_set_anchor(struct nemocanvas *canvas, float ax, float ay);
extern void nemocanvas_set_layer(struct nemocanvas *canvas, uint32_t type);
extern void nemocanvas_set_parent(struct nemocanvas *canvas, struct nemocanvas *parent);
extern void nemocanvas_set_fullscreen(struct nemocanvas *canvas);
extern void nemocanvas_unset_fullscreen(struct nemocanvas *canvas);

extern void nemocanvas_set_nemosurface(struct nemocanvas *canvas, uint32_t type);

extern int nemocanvas_set_subsurface(struct nemocanvas *canvas, struct nemocanvas *parent);
extern void nemocanvas_set_subsurface_position(struct nemocanvas *canvas, int32_t x, int32_t y);
extern void nemocanvas_set_subsurface_sync(struct nemocanvas *canvas);
extern void nemocanvas_set_subsurface_desync(struct nemocanvas *canvas);

extern void nemocanvas_set_dispatch_event(struct nemocanvas *canvas, nemocanvas_dispatch_event_t dispatch);
extern void nemocanvas_set_dispatch_resize(struct nemocanvas *canvas, nemocanvas_dispatch_resize_t dispatch);
extern void nemocanvas_set_dispatch_frame(struct nemocanvas *canvas, nemocanvas_dispatch_frame_t dispatch);

extern void nemocanvas_dispatch_frame(struct nemocanvas *canvas);

extern void nemocanvas_attach_queue(struct nemocanvas *canvas, struct nemoqueue *queue);
extern void nemocanvas_detach_queue(struct nemocanvas *canvas);

static inline struct nemotool *nemocanvas_get_tool(struct nemocanvas *canvas)
{
	return canvas->tool;
}

static inline struct nemocanvas *nemocanvas_get_parent(struct nemocanvas *canvas)
{
	return canvas->parent;
}

static inline struct wl_surface *nemocanvas_get_surface(struct nemocanvas *canvas)
{
	return canvas->surface;
}

static inline char *nemocanvas_get_data(struct nemocanvas *canvas)
{
	return canvas->buffer == NULL ? NULL : canvas->buffer->shm_data;
}

static inline int nemocanvas_get_width(struct nemocanvas *canvas)
{
	return canvas->width;
}

static inline int nemocanvas_get_height(struct nemocanvas *canvas)
{
	return canvas->height;
}

static inline int nemocanvas_get_stride(struct nemocanvas *canvas)
{
	return canvas->width * 4;
}

static inline void nemocanvas_set_userdata(struct nemocanvas *canvas, void *data)
{
	canvas->userdata = data;
}

static inline void *nemocanvas_get_userdata(struct nemocanvas *canvas)
{
	return canvas->userdata;
}

#endif
