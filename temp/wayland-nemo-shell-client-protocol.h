#ifndef NEMO_SHELL_CLIENT_PROTOCOL_H
#define NEMO_SHELL_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct nemo_shell;
struct nemo_surface;

extern const struct wl_interface nemo_shell_interface;
extern const struct wl_interface nemo_surface_interface;

#ifndef NEMO_SHELL_VERSION_ENUM
#define NEMO_SHELL_VERSION_ENUM
enum nemo_shell_version {
	NEMO_SHELL_VERSION_CURRENT = 1,
};
#endif /* NEMO_SHELL_VERSION_ENUM */

#ifndef NEMO_SHELL_SURFACE_TYPE_ENUM
#define NEMO_SHELL_SURFACE_TYPE_ENUM
enum nemo_shell_surface_type {
	NEMO_SHELL_SURFACE_TYPE_NORMAL = 0,
	NEMO_SHELL_SURFACE_TYPE_FOLLOW = 1,
	NEMO_SHELL_SURFACE_TYPE_OVERLAY = 2,
};
#endif /* NEMO_SHELL_SURFACE_TYPE_ENUM */

struct nemo_shell_listener {
	/**
	 * ping - (none)
	 * @serial: (none)
	 */
	void (*ping)(void *data,
		     struct nemo_shell *nemo_shell,
		     uint32_t serial);
};

static inline int
nemo_shell_add_listener(struct nemo_shell *nemo_shell,
			const struct nemo_shell_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) nemo_shell,
				     (void (**)(void)) listener, data);
}

#define NEMO_SHELL_USE_UNSTABLE_VERSION	0
#define NEMO_SHELL_GET_NEMO_SURFACE	1
#define NEMO_SHELL_PONG	2

static inline void
nemo_shell_set_user_data(struct nemo_shell *nemo_shell, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) nemo_shell, user_data);
}

static inline void *
nemo_shell_get_user_data(struct nemo_shell *nemo_shell)
{
	return wl_proxy_get_user_data((struct wl_proxy *) nemo_shell);
}

static inline void
nemo_shell_destroy(struct nemo_shell *nemo_shell)
{
	wl_proxy_destroy((struct wl_proxy *) nemo_shell);
}

static inline void
nemo_shell_use_unstable_version(struct nemo_shell *nemo_shell, int32_t version)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_shell,
			 NEMO_SHELL_USE_UNSTABLE_VERSION, version);
}

static inline struct nemo_surface *
nemo_shell_get_nemo_surface(struct nemo_shell *nemo_shell, struct wl_surface *surface, uint32_t type)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) nemo_shell,
			 NEMO_SHELL_GET_NEMO_SURFACE, &nemo_surface_interface, NULL, surface, type);

	return (struct nemo_surface *) id;
}

static inline void
nemo_shell_pong(struct nemo_shell *nemo_shell, uint32_t serial)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_shell,
			 NEMO_SHELL_PONG, serial);
}

#ifndef NEMO_SURFACE_PICK_TYPE_ENUM
#define NEMO_SURFACE_PICK_TYPE_ENUM
enum nemo_surface_pick_type {
	NEMO_SURFACE_PICK_TYPE_SCALE = 0,
	NEMO_SURFACE_PICK_TYPE_ROTATE = 1,
	NEMO_SURFACE_PICK_TYPE_RESIZE = 2,
};
#endif /* NEMO_SURFACE_PICK_TYPE_ENUM */

#ifndef NEMO_SURFACE_LAYER_TYPE_ENUM
#define NEMO_SURFACE_LAYER_TYPE_ENUM
enum nemo_surface_layer_type {
	NEMO_SURFACE_LAYER_TYPE_BACKGROUND = 0,
	NEMO_SURFACE_LAYER_TYPE_SERVICE = 1,
	NEMO_SURFACE_LAYER_TYPE_OVERLAY = 2,
	NEMO_SURFACE_LAYER_TYPE_INTERFACE = 3,
};
#endif /* NEMO_SURFACE_LAYER_TYPE_ENUM */

struct nemo_surface_listener {
	/**
	 * configure - (none)
	 * @width: (none)
	 * @height: (none)
	 */
	void (*configure)(void *data,
			  struct nemo_surface *nemo_surface,
			  int32_t width,
			  int32_t height);
};

static inline int
nemo_surface_add_listener(struct nemo_surface *nemo_surface,
			  const struct nemo_surface_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) nemo_surface,
				     (void (**)(void)) listener, data);
}

#define NEMO_SURFACE_DESTROY	0
#define NEMO_SURFACE_MOVE	1
#define NEMO_SURFACE_PICK	2
#define NEMO_SURFACE_MISS	3
#define NEMO_SURFACE_FOLLOW	4
#define NEMO_SURFACE_EXECUTE	5
#define NEMO_SURFACE_SET_SIZE	6
#define NEMO_SURFACE_SET_INPUT	7
#define NEMO_SURFACE_SET_PIVOT	8
#define NEMO_SURFACE_SET_ANCHOR	9
#define NEMO_SURFACE_SET_LAYER	10
#define NEMO_SURFACE_SET_PARENT	11
#define NEMO_SURFACE_SET_FULLSCREEN	12
#define NEMO_SURFACE_UNSET_FULLSCREEN	13

static inline void
nemo_surface_set_user_data(struct nemo_surface *nemo_surface, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) nemo_surface, user_data);
}

static inline void *
nemo_surface_get_user_data(struct nemo_surface *nemo_surface)
{
	return wl_proxy_get_user_data((struct wl_proxy *) nemo_surface);
}

static inline void
nemo_surface_destroy(struct nemo_surface *nemo_surface)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_surface,
			 NEMO_SURFACE_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) nemo_surface);
}

static inline void
nemo_surface_move(struct nemo_surface *nemo_surface, struct nemo_seat *seat, uint32_t serial)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_surface,
			 NEMO_SURFACE_MOVE, seat, serial);
}

static inline void
nemo_surface_pick(struct nemo_surface *nemo_surface, struct nemo_seat *seat, uint32_t serial0, uint32_t serial1, uint32_t type)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_surface,
			 NEMO_SURFACE_PICK, seat, serial0, serial1, type);
}

static inline void
nemo_surface_miss(struct nemo_surface *nemo_surface)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_surface,
			 NEMO_SURFACE_MISS);
}

static inline void
nemo_surface_follow(struct nemo_surface *nemo_surface, int32_t x, int32_t y, int32_t degree, uint32_t delay, uint32_t duration)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_surface,
			 NEMO_SURFACE_FOLLOW, x, y, degree, delay, duration);
}

static inline void
nemo_surface_execute(struct nemo_surface *nemo_surface, const char *name, const char *cmds)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_surface,
			 NEMO_SURFACE_EXECUTE, name, cmds);
}

static inline void
nemo_surface_set_size(struct nemo_surface *nemo_surface, uint32_t width, uint32_t height)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_surface,
			 NEMO_SURFACE_SET_SIZE, width, height);
}

static inline void
nemo_surface_set_input(struct nemo_surface *nemo_surface, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_surface,
			 NEMO_SURFACE_SET_INPUT, x, y, width, height);
}

static inline void
nemo_surface_set_pivot(struct nemo_surface *nemo_surface, int32_t px, int32_t py)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_surface,
			 NEMO_SURFACE_SET_PIVOT, px, py);
}

static inline void
nemo_surface_set_anchor(struct nemo_surface *nemo_surface, wl_fixed_t ax, wl_fixed_t ay)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_surface,
			 NEMO_SURFACE_SET_ANCHOR, ax, ay);
}

static inline void
nemo_surface_set_layer(struct nemo_surface *nemo_surface, uint32_t type)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_surface,
			 NEMO_SURFACE_SET_LAYER, type);
}

static inline void
nemo_surface_set_parent(struct nemo_surface *nemo_surface, struct nemo_surface *parent)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_surface,
			 NEMO_SURFACE_SET_PARENT, parent);
}

static inline void
nemo_surface_set_fullscreen(struct nemo_surface *nemo_surface, struct wl_output *output)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_surface,
			 NEMO_SURFACE_SET_FULLSCREEN, output);
}

static inline void
nemo_surface_unset_fullscreen(struct nemo_surface *nemo_surface)
{
	wl_proxy_marshal((struct wl_proxy *) nemo_surface,
			 NEMO_SURFACE_UNSET_FULLSCREEN);
}

#ifdef  __cplusplus
}
#endif

#endif
