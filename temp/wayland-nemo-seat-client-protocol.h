#ifndef NEMO_SEAT_CLIENT_PROTOCOL_H
#define NEMO_SEAT_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct nemo_seat;
struct nemo_pointer;
struct nemo_keyboard;
struct nemo_touch;

extern const struct wl_interface nemo_seat_interface;
extern const struct wl_interface nemo_pointer_interface;
extern const struct wl_interface nemo_keyboard_interface;
extern const struct wl_interface nemo_touch_interface;

#ifndef NEMO_SEAT_CAPABILITY_ENUM
#define NEMO_SEAT_CAPABILITY_ENUM
enum nemo_seat_capability {
	NEMO_SEAT_CAPABILITY_POINTER = 1,
	NEMO_SEAT_CAPABILITY_KEYBOARD = 2,
	NEMO_SEAT_CAPABILITY_TOUCH = 4,
};
#endif /* NEMO_SEAT_CAPABILITY_ENUM */

struct nemo_seat_listener {
	/**
	 * capabilities - (none)
	 * @capabilities: (none)
	 */
	void (*capabilities)(void *data,
			     struct nemo_seat *nemo_seat,
			     uint32_t capabilities);
	/**
	 * name - (none)
	 * @name: (none)
	 */
	void (*name)(void *data,
		     struct nemo_seat *nemo_seat,
		     const char *name);
};

static inline int
nemo_seat_add_listener(struct nemo_seat *nemo_seat,
		       const struct nemo_seat_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) nemo_seat,
				     (void (**)(void)) listener, data);
}

#define NEMO_SEAT_GET_POINTER	0
#define NEMO_SEAT_GET_KEYBOARD	1
#define NEMO_SEAT_GET_TOUCH	2

static inline void
nemo_seat_set_user_data(struct nemo_seat *nemo_seat, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) nemo_seat, user_data);
}

static inline void *
nemo_seat_get_user_data(struct nemo_seat *nemo_seat)
{
	return wl_proxy_get_user_data((struct wl_proxy *) nemo_seat);
}

static inline void
nemo_seat_destroy(struct nemo_seat *nemo_seat)
{
	wl_proxy_destroy((struct wl_proxy *) nemo_seat);
}

static inline struct nemo_pointer *
nemo_seat_get_pointer(struct nemo_seat *nemo_seat)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) nemo_seat,
			 NEMO_SEAT_GET_POINTER, &nemo_pointer_interface, NULL);

	return (struct nemo_pointer *) id;
}

static inline struct nemo_keyboard *
nemo_seat_get_keyboard(struct nemo_seat *nemo_seat)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) nemo_seat,
			 NEMO_SEAT_GET_KEYBOARD, &nemo_keyboard_interface, NULL);

	return (struct nemo_keyboard *) id;
}

static inline struct nemo_touch *
nemo_seat_get_touch(struct nemo_seat *nemo_seat)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) nemo_seat,
			 NEMO_SEAT_GET_TOUCH, &nemo_touch_interface, NULL);

	return (struct nemo_touch *) id;
}

#ifndef NEMO_POINTER_ERROR_ENUM
#define NEMO_POINTER_ERROR_ENUM
enum nemo_pointer_error {
	NEMO_POINTER_ERROR_ROLE = 0,
};
#endif /* NEMO_POINTER_ERROR_ENUM */

#ifndef NEMO_POINTER_BUTTON_STATE_ENUM
#define NEMO_POINTER_BUTTON_STATE_ENUM
enum nemo_pointer_button_state {
	NEMO_POINTER_BUTTON_STATE_RELEASED = 0,
	NEMO_POINTER_BUTTON_STATE_PRESSED = 1,
};
#endif /* NEMO_POINTER_BUTTON_STATE_ENUM */

#ifndef NEMO_POINTER_AXIS_ENUM
#define NEMO_POINTER_AXIS_ENUM
enum nemo_pointer_axis {
	NEMO_POINTER_AXIS_VERTICAL_SCROLL = 0,
	NEMO_POINTER_AXIS_HORIZONTAL_SCROLL = 1,
};
#endif /* NEMO_POINTER_AXIS_ENUM */

struct nemo_pointer_listener {
	/**
	 * enter - (none)
	 * @serial: (none)
	 * @surface: (none)
	 * @id: (none)
	 * @surface_x: (none)
	 * @surface_y: (none)
	 */
	void (*enter)(void *data,
		      struct nemo_pointer *nemo_pointer,
		      uint32_t serial,
		      struct wl_surface *surface,
		      int32_t id,
		      wl_fixed_t surface_x,
		      wl_fixed_t surface_y);
	/**
	 * leave - (none)
	 * @serial: (none)
	 * @surface: (none)
	 * @id: (none)
	 */
	void (*leave)(void *data,
		      struct nemo_pointer *nemo_pointer,
		      uint32_t serial,
		      struct wl_surface *surface,
		      int32_t id);
	/**
	 * motion - (none)
	 * @time: (none)
	 * @surface: (none)
	 * @id: (none)
	 * @surface_x: (none)
	 * @surface_y: (none)
	 */
	void (*motion)(void *data,
		       struct nemo_pointer *nemo_pointer,
		       uint32_t time,
		       struct wl_surface *surface,
		       int32_t id,
		       wl_fixed_t surface_x,
		       wl_fixed_t surface_y);
	/**
	 * button - (none)
	 * @serial: (none)
	 * @time: (none)
	 * @surface: (none)
	 * @id: (none)
	 * @button: (none)
	 * @state: (none)
	 */
	void (*button)(void *data,
		       struct nemo_pointer *nemo_pointer,
		       uint32_t serial,
		       uint32_t time,
		       struct wl_surface *surface,
		       int32_t id,
		       uint32_t button,
		       uint32_t state);
	/**
	 * axis - (none)
	 * @time: (none)
	 * @surface: (none)
	 * @id: (none)
	 * @axis: (none)
	 * @value: (none)
	 */
	void (*axis)(void *data,
		     struct nemo_pointer *nemo_pointer,
		     uint32_t time,
		     struct wl_surface *surface,
		     int32_t id,
		     uint32_t axis,
		     wl_fixed_t value);
};

static inline int
nemo_pointer_add_listener(struct nemo_pointer *nemo_pointer,
			  const struct nemo_pointer_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) nemo_pointer,
				     (void (**)(void)) listener, data);
}

#define NEMO_POINTER_SET_CURSOR	0
#define NEMO_POINTER_RELEASE	1

static inline void
nemo_pointer_set_user_data(struct nemo_pointer *nemo_pointer, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) nemo_pointer, user_data);
}

static inline void *
nemo_pointer_get_user_data(struct nemo_pointer *nemo_pointer)
{
	return wl_proxy_get_user_data((struct wl_proxy *) nemo_pointer);
}

static inline void
nemo_pointer_destroy(struct nemo_pointer *nemo_pointer)
{
	wl_proxy_destroy((struct wl_proxy *) nemo_pointer);
}

static inline void
nemo_pointer_set_cursor(struct nemo_pointer *nemo_pointer, uint32_t serial, struct wl_surface *surface, int32_t id, int32_t hotspot_x, int32_t hotspot_y)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_pointer,
			 NEMO_POINTER_SET_CURSOR, serial, surface, id, hotspot_x, hotspot_y);
}

static inline void
nemo_pointer_release(struct nemo_pointer *nemo_pointer)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_pointer,
			 NEMO_POINTER_RELEASE);

	wl_proxy_destroy((struct wl_proxy *) nemo_pointer);
}

#ifndef NEMO_KEYBOARD_KEYMAP_FORMAT_ENUM
#define NEMO_KEYBOARD_KEYMAP_FORMAT_ENUM
enum nemo_keyboard_keymap_format {
	NEMO_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP = 0,
	NEMO_KEYBOARD_KEYMAP_FORMAT_XKB_V1 = 1,
};
#endif /* NEMO_KEYBOARD_KEYMAP_FORMAT_ENUM */

#ifndef NEMO_KEYBOARD_KEY_STATE_ENUM
#define NEMO_KEYBOARD_KEY_STATE_ENUM
enum nemo_keyboard_key_state {
	NEMO_KEYBOARD_KEY_STATE_RELEASED = 0,
	NEMO_KEYBOARD_KEY_STATE_PRESSED = 1,
};
#endif /* NEMO_KEYBOARD_KEY_STATE_ENUM */

struct nemo_keyboard_listener {
	/**
	 * keymap - (none)
	 * @format: (none)
	 * @fd: (none)
	 * @size: (none)
	 */
	void (*keymap)(void *data,
		       struct nemo_keyboard *nemo_keyboard,
		       uint32_t format,
		       int32_t fd,
		       uint32_t size);
	/**
	 * enter - (none)
	 * @serial: (none)
	 * @surface: (none)
	 * @id: (none)
	 * @keys: (none)
	 */
	void (*enter)(void *data,
		      struct nemo_keyboard *nemo_keyboard,
		      uint32_t serial,
		      struct wl_surface *surface,
		      int32_t id,
		      struct wl_array *keys);
	/**
	 * leave - (none)
	 * @serial: (none)
	 * @surface: (none)
	 * @id: (none)
	 */
	void (*leave)(void *data,
		      struct nemo_keyboard *nemo_keyboard,
		      uint32_t serial,
		      struct wl_surface *surface,
		      int32_t id);
	/**
	 * key - (none)
	 * @serial: (none)
	 * @time: (none)
	 * @surface: (none)
	 * @id: (none)
	 * @key: (none)
	 * @state: (none)
	 */
	void (*key)(void *data,
		    struct nemo_keyboard *nemo_keyboard,
		    uint32_t serial,
		    uint32_t time,
		    struct wl_surface *surface,
		    int32_t id,
		    uint32_t key,
		    uint32_t state);
	/**
	 * modifiers - (none)
	 * @serial: (none)
	 * @surface: (none)
	 * @id: (none)
	 * @mods_depressed: (none)
	 * @mods_latched: (none)
	 * @mods_locked: (none)
	 * @group: (none)
	 */
	void (*modifiers)(void *data,
			  struct nemo_keyboard *nemo_keyboard,
			  uint32_t serial,
			  struct wl_surface *surface,
			  int32_t id,
			  uint32_t mods_depressed,
			  uint32_t mods_latched,
			  uint32_t mods_locked,
			  uint32_t group);
	/**
	 * repeat_info - (none)
	 * @rate: (none)
	 * @delay: (none)
	 */
	void (*repeat_info)(void *data,
			    struct nemo_keyboard *nemo_keyboard,
			    int32_t rate,
			    int32_t delay);
};

static inline int
nemo_keyboard_add_listener(struct nemo_keyboard *nemo_keyboard,
			   const struct nemo_keyboard_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) nemo_keyboard,
				     (void (**)(void)) listener, data);
}

#define NEMO_KEYBOARD_RELEASE	0

static inline void
nemo_keyboard_set_user_data(struct nemo_keyboard *nemo_keyboard, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) nemo_keyboard, user_data);
}

static inline void *
nemo_keyboard_get_user_data(struct nemo_keyboard *nemo_keyboard)
{
	return wl_proxy_get_user_data((struct wl_proxy *) nemo_keyboard);
}

static inline void
nemo_keyboard_destroy(struct nemo_keyboard *nemo_keyboard)
{
	wl_proxy_destroy((struct wl_proxy *) nemo_keyboard);
}

static inline void
nemo_keyboard_release(struct nemo_keyboard *nemo_keyboard)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_keyboard,
			 NEMO_KEYBOARD_RELEASE);

	wl_proxy_destroy((struct wl_proxy *) nemo_keyboard);
}

struct nemo_touch_listener {
	/**
	 * down - (none)
	 * @serial: (none)
	 * @time: (none)
	 * @surface: (none)
	 * @id: (none)
	 * @x: (none)
	 * @y: (none)
	 */
	void (*down)(void *data,
		     struct nemo_touch *nemo_touch,
		     uint32_t serial,
		     uint32_t time,
		     struct wl_surface *surface,
		     int32_t id,
		     wl_fixed_t x,
		     wl_fixed_t y);
	/**
	 * up - (none)
	 * @serial: (none)
	 * @time: (none)
	 * @surface: (none)
	 * @id: (none)
	 * @dx: (none)
	 * @dy: (none)
	 */
	void (*up)(void *data,
		   struct nemo_touch *nemo_touch,
		   uint32_t serial,
		   uint32_t time,
		   struct wl_surface *surface,
		   int32_t id,
		   wl_fixed_t dx,
		   wl_fixed_t dy);
	/**
	 * motion - (none)
	 * @time: (none)
	 * @surface: (none)
	 * @id: (none)
	 * @x: (none)
	 * @y: (none)
	 */
	void (*motion)(void *data,
		       struct nemo_touch *nemo_touch,
		       uint32_t time,
		       struct wl_surface *surface,
		       int32_t id,
		       wl_fixed_t x,
		       wl_fixed_t y);
	/**
	 * frame - (none)
	 */
	void (*frame)(void *data,
		      struct nemo_touch *nemo_touch);
	/**
	 * cancel - (none)
	 */
	void (*cancel)(void *data,
		       struct nemo_touch *nemo_touch);
};

static inline int
nemo_touch_add_listener(struct nemo_touch *nemo_touch,
			const struct nemo_touch_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) nemo_touch,
				     (void (**)(void)) listener, data);
}

#define NEMO_TOUCH_RELEASE	0

static inline void
nemo_touch_set_user_data(struct nemo_touch *nemo_touch, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) nemo_touch, user_data);
}

static inline void *
nemo_touch_get_user_data(struct nemo_touch *nemo_touch)
{
	return wl_proxy_get_user_data((struct wl_proxy *) nemo_touch);
}

static inline void
nemo_touch_destroy(struct nemo_touch *nemo_touch)
{
	wl_proxy_destroy((struct wl_proxy *) nemo_touch);
}

static inline void
nemo_touch_release(struct nemo_touch *nemo_touch)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_touch,
			 NEMO_TOUCH_RELEASE);

	wl_proxy_destroy((struct wl_proxy *) nemo_touch);
}

#ifdef  __cplusplus
}
#endif

#endif
