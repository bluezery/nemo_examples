#ifndef __CAIRO_WAYLAND_H__
#define __CAIRO_WAYLAND_H__

#define _GNU_SOURCE
#include <wayland-client.h>
#include <cairo.h>
//#include <xkbcommon/xkbcommon.h>

#include <stdlib.h>  // free, calloc, mkostemp, malloc, getenv
#include <errno.h>
#include <string.h>   // strerror
#include <unistd.h> // close, unlink
#include <sys/mman.h>  // mmap
#include <fcntl.h>      // posix_fallocate

#include "log.h"

typedef struct _Window {
    struct wl_display *display;
    struct wl_compositor *compositor;
    struct wl_shell *shell;
    struct wl_shm *shm;

    struct wl_shm_pool *pool;
    struct wl_surface *main_surface;
    struct wl_buffer *main_buffer;
    unsigned char *main_map;

    struct wl_seat *seat;
#if 0
    int32_t pointer_hotspot_x;
    int32_t pointer_hotspot_y;
    struct wl_pointer *pointer;
    struct wl_buffer *pointer_buffer;
    struct wl_surface *pointer_surface;
    struct wl_surface *pointer_target_surface;
#endif
} Window;

static void _wl_log(const char *format, va_list args)
{
    fprintf(stderr, format, args);
}

/*
static void
xdg_shell_ping(void *data, struct xdg_shell *shell, uint32_t serial)
{
    wl_proxy_marshal((struct wl_proxy *)xdg_shell, 3, serial);
}

static const struct xdg_shell_listener xdg_shell_listener = {
	xdg_shell_ping,
};
*/

/*
static void
_pointer_enter(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
    Window *display = data;
    display->pointer_target_surface = surface;
    wl_surface_attach(display->pointer_surface, display->pointer_buffer, 0, 0);
    wl_surface_commit(display->pointer_surface);
    wl_pointer_set_cursor(wl_pointer, serial, display->pointer_surface,
            display->pointer_hotspot_x, display->pointer_hotspot_y);
}

static void
_pointer_leave(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *wl_surface)
{
}

static void
_pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
}

static void
_pointer_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
    void (*callback)(uint32_t);

    Window *display = data;
    callback = wl_surface_get_user_data(display->pointer_target_surface);
    if (callback) callback(button);
}

static void _pointer_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

static const struct wl_pointer_listener _pointer_listener = {
    .enter = _pointer_enter,
    .leave = _pointer_leave,
    .motion = _pointer_motion,
    .button = _pointer_button,
    .axis = _pointer_axis
};
*/

static void
_registry_listener_global(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
    RET_IF(!registry);
    RET_IF(!interface);

    Window *display = data;

    //LOG("id:%d interface:%s version:%d", id, interface, version);
    if (!strcmp(interface, wl_compositor_interface.name)) {
        display->compositor = wl_registry_bind(registry, id,
                &wl_compositor_interface, 3);
    } else if (!strcmp(interface, wl_shm_interface.name)) {
        display->shm = wl_registry_bind(registry, id,
                &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_shell_interface.name) == 0) {
        display->shell = wl_registry_bind(registry, id,
                &wl_shell_interface, 1);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        /*
        display->seat = wl_registry_bind(registry, id,
                &wl_seat_interface, 1);
        display->pointer = wl_seat_get_pointer(display->seat);
        wl_pointer_add_listener(display->pointer, &_pointer_listener, display);
        */
    } else if (strcmp(interface, "xdg_shell") == 0) {
        //LOG("XDG");
    }
}

static void
_registry_listener_global_remove(void *data, struct wl_registry *registry, uint32_t id)
{
    return;
}

static void
_surface_listner_enter(void *data, struct wl_surface *wl_surfacce, struct wl_output *wl_output)
{
    ERR("");
    return;
}

static void
_surface_listner_leave(void *data, struct wl_surface *wl_surfacce, struct wl_output *wl_output)
{
    ERR("");
    return;
}

static void
_shell_surface_listener_ping(void *data, struct wl_shell_surface *shell_surface, uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
}

static void
_shell_surface_listener_configure(void *data, struct wl_shell_surface *wl_shell_surface, uint32_t edges, int32_t width, int32_t height)
{

}

static const struct wl_registry_listener _registry_listener = {
    _registry_listener_global,
    _registry_listener_global_remove,
};

static const struct wl_surface_listener _surface_listener = {
	_surface_listner_enter,
	_surface_listner_leave
};

static const struct wl_shell_surface_listener _shell_surface_listener = {
    _shell_surface_listener_ping,
    _shell_surface_listener_configure
};

static struct wl_display *
_display_create(const char *name, void *data)
{
    struct wl_display *display;
    struct wl_registry *registry;

    display = wl_display_connect(name);
    if (!display) {
        ERR("wl display failed:%s ", strerror(errno));
        return NULL;
    }
    registry = wl_display_get_registry(display);
    if (!registry) {
        ERR("wl display get registry failed");
        wl_display_disconnect(display);
        return NULL;
    }
    wl_registry_add_listener(registry, &_registry_listener, data);

	if (wl_display_roundtrip(display) < 0) {
	    ERR("wl display roundtrip failed: %s", strerror(errno));
	    wl_registry_destroy(registry);
	    wl_display_disconnect(display);
        return NULL;
	}
	wl_registry_destroy(registry);

    return display;
}

static struct wl_shm_pool *
_shm_pool_create(struct wl_shm *shm, unsigned int size)
{
    RET_IF(!shm, NULL);
    RET_IF(size == 0, NULL);

    int fd;
    unsigned char *map;
    struct wl_shm_pool *pool;

    static const char temp[] = "/test-shared-XXXXXX";
    const char *path = getenv("XDG_RUNTIME_DIR");

    if (!path) {
        ERR("XDG_RUNTIME_DIR is not set");
        return NULL;
    }

    char *name = malloc(strlen(path) + sizeof(temp));
    if (!name) {
        ERR("memory allocation failed");
    }
    strcpy(name, path);
    strcat(name, temp);

    fd = mkostemp(name, O_CLOEXEC);
    LOG("path:%s, temp:%s, name:%s", path, temp, name);
    if (fd >= 0) unlink(name);
    free(name);

	if (posix_fallocate(fd, 0, size)) {
        ERR("posix fallocate failed! fd:%d len:%d\n", fd, size);
	    close(fd);
        return NULL;
    }

    map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        ERR("mmap failed");
        close(fd);
        return NULL;
    }

    pool = wl_shm_create_pool(shm, fd, size);
    if (!pool) {
        ERR("wl shm create pool failed");
        munmap(map, size);
        close(fd);
        return NULL;
    }

    wl_shm_pool_set_user_data(pool, map);

    return pool;
}

static struct wl_buffer *
_buffer_create(struct wl_shm_pool *pool, unsigned int w, unsigned int h, unsigned int stride)
{
    RET_IF(!pool, NULL);
    RET_IF(w <=0 || h <= 0, NULL);

    struct wl_buffer *buffer;

    buffer = wl_shm_pool_create_buffer(pool, 0, w, h, stride, WL_SHM_FORMAT_ARGB8888);
    //wl_buffer_add_listener(buffer, &shm_surface_buffer_listener, NULL);

    return buffer;
}

/*
static void
_pointer_init(Window *display, unsigned w, unsigned h, int32_t hotspot_x, int32_t hotspot_y)
{
    RET_IF(!display);
    display->pointer_hotspot_x = hotspot_x;
    display->pointer_hotspot_y = hotspot_y;
    display->pointer_surface = wl_compositor_create_surface(display->compositor);

    if (!display->pointer_surface) {
        ERR("wl compositor create surface failed");
        return;
    }

    display->pointer_buffer = _buffer_create(display->pool, w, h, 0);
    if (!display->pointer_buffer) {
        wl_surface_destroy(display->pointer_surface);
        ERR("_buffer_create failed");
        return;
    }
    wl_pointer_set_user_data(display->pointer, display);
}
*/

static struct wl_surface *
_surface_main_create(struct wl_compositor *compositor, struct wl_shell *shell)
{
    struct wl_surface *surface;
    struct wl_shell_surface *shell_surface;
	surface = wl_compositor_create_surface(compositor);
	if (!surface) {
	    ERR("wl compositor create surface failed");
	    return NULL;
    }

    shell_surface = wl_shell_get_shell_surface(shell, surface);
    if (!shell_surface) {
        ERR("wl shell get shell surface failed");
        return NULL;
    }
    wl_shell_surface_add_listener(shell_surface,
            &_shell_surface_listener, NULL);
    wl_shell_surface_set_toplevel(shell_surface);
    wl_surface_set_user_data(surface, shell_surface);
	wl_surface_add_listener(surface, &_surface_listener, NULL);

	return surface;
}

static Window *
_window_create(unsigned int w, unsigned int h, unsigned int stride)
{
    Window *window;

    window = (Window *)calloc(sizeof(Window), 1);

    wl_log_set_handler_client(_wl_log);

    window->display = _display_create(NULL, window);
    if (!window->compositor || !window->shm ||
        !window->shell ) {
        ERR("compositor or shm or shell or eat is not received");
	    wl_display_disconnect(window->display);
        return NULL;
    }
    window->pool = _shm_pool_create(window->shm, h * stride);
    window->main_map = wl_shm_pool_get_user_data(window->pool);

    window->main_buffer = _buffer_create(window->pool, w, h, stride);
    window->main_surface = _surface_main_create(window->compositor, window->shell);

    wl_surface_attach(window->main_surface, window->main_buffer, 0, 0);
    wl_surface_commit(window->main_surface);
    //_pointer_init(display, 100, 59, 10, 35);

    return window;
}

static void
_window_loop(Window *window)
{
#if 1
    while (1) {
        if (wl_display_dispatch(window->display) < 0) {
            ERR("wl display dispatch failed");
            break;
        }
    }
#else // difference??
    while (1) {
        wl_display_dispatch_pending(display->display);
        ret = wl_display_flush(display->display);
        if (ret < 0 && errno == EAGAIN) {
            ERR("%s", strerror(errno));
        } else if (ret < 0) {
            break;
        }
    }
#endif
}

static void
_window_set_buffer(Window *window, unsigned char *data, unsigned int size)
{
    memcpy(window->main_map, data, size);
}

#endif
