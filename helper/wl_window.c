#define _GNU_SOURCE
#include <wayland-client.h>
//#include <xkbcommon/xkbcommon.h>

#include <stdbool.h>
#include <stdlib.h>  // free, calloc, mkostemp, malloc, getenv
#include <unistd.h> // close, unlink
#include <sys/mman.h>  // mmap
#include <fcntl.h>      // posix_fallocate
#include <sys/epoll.h>  // epoll

#include <string.h>
#include <errno.h>

#include "log.h"
#include "util.h"
#include "view.h"
#include "wl_window.h"

struct _Wl_Window {
    struct wl_display *display;
    int display_fd;
    struct wl_compositor *compositor;
    struct wl_shell *shell;
    struct wl_shm *shm;

    struct wl_shm_pool *pool;
    struct wl_surface *main_surface;
    struct wl_buffer *main_buffer;
    unsigned char *main_map;

    struct wl_seat *seat;
    int epfd;
#if 0
    int32_t pointer_hotspot_x;
    int32_t pointer_hotspot_y;
    struct wl_pointer *pointer;
    struct wl_buffer *pointer_buffer;
    struct wl_surface *pointer_surface;
    struct wl_surface *pointer_target_surface;
#endif
};

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
    Wl_Window *display = data;
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

    Wl_Window *display = data;
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

    Wl_Window *display = data;

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
    return;
}

static void
_surface_listner_leave(void *data, struct wl_surface *wl_surfacce, struct wl_output *wl_output)
{
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
        ERR("wl display failed: %s ", strerror(errno));
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
_pointer_init(Wl_Window *display, unsigned w, unsigned h, int32_t hotspot_x, int32_t hotspot_y)
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

Wl_Window *
wl_window_create(unsigned int w, unsigned int h, unsigned int stride)
{
    Wl_Window *win;

    win = (Wl_Window *)calloc(sizeof(Wl_Window), 1);

    wl_log_set_handler_client(_wl_log);

    win->display = _display_create(NULL, win);
    if (!win->display) {
        ERR("display create failed");
        free(win);
        return NULL;
    }

    if (!win->compositor || !win->shm ||
        !win->shell ) {
        ERR("compositor or shm or shell or eat is not received");
	    wl_display_disconnect(win->display);
	    free(win);
        return NULL;
    }
    win->pool = _shm_pool_create(win->shm, h * stride);
    win->main_map = wl_shm_pool_get_user_data(win->pool);

    win->main_buffer = _buffer_create(win->pool, w, h, stride);
    win->main_surface = _surface_main_create(win->compositor, win->shell);

    wl_surface_attach(win->main_surface, win->main_buffer, 0, 0);
    wl_surface_commit(win->main_surface);
    //_pointer_init(display, 100, 59, 10, 35);

    win->epfd = epoll_create1(EPOLL_CLOEXEC);

    win->display_fd = wl_display_get_fd(win->display);
    struct epoll_event ep;
    ep.events = EPOLLIN;
    ep.data.fd = win->display_fd;
    epoll_ctl(win->epfd, EPOLL_CTL_ADD, win->display_fd, &ep);

    return win;
}

void
wl_window_destroy(Wl_Window *win)
{
    RET_IF(!win);
    close(win->epfd);
    wl_shell_destroy(win->shell);
    wl_compositor_destroy(win->compositor);
    wl_seat_destroy(win->seat);

    wl_surface_destroy(win->main_surface);
    wl_buffer_destroy(win->main_buffer);
    wl_shm_destroy(win->shm);
    wl_shm_pool_destroy(win->pool);

    wl_display_disconnect(win->display);
    free(win);
}

void
wl_window_loop(Wl_Window *win)
{
    struct epoll_event ep[16];
    int ret, ep_cnt;
    while (1) {
        wl_display_dispatch_pending(win->display);
        ret = wl_display_flush(win->display);
        if (ret < 0 && errno == EAGAIN) {
            LOG("flush needed again");
            // Add into epoll to wait
            ep[0].events = EPOLLIN | EPOLLOUT;
            ep[0].data.fd = win->display_fd;
            epoll_ctl(win->epfd, EPOLL_CTL_MOD, win->display_fd, &ep[0]);
        } else if (ret < 0) {
            perror("wl display flush failed: ");
            break;
        }

        ep_cnt = epoll_wait(win->epfd, ep, sizeof(ep)/sizeof(ep[0]), -1);
        if (ep_cnt == -1) {
            perror("epoll wait failed: ");
            continue;
        }
        //LOG("cnt: %d", ep_cnt);
        int i = 0;
        for (i = 0 ; i < ep_cnt ; i++) {
            //LOG("%p", ep[i].data.ptr);
            if (ep[i].data.fd == win->display_fd) {
                if (ep[i].events & EPOLLERR || ep[i].events & EPOLLHUP) {
                    perror("epoll wait failed: ");
                    break;
                }
                if (ep[i].events & EPOLLIN) {
                    if (wl_display_dispatch(win->display) < 0) {
                        perror("wl display dispatch failed: ");
                        break;
                    }
                }
                if (ep[i].events & EPOLLOUT) {
                    ret = wl_display_flush(win->display);
                    if (ret == 0) {
                        struct epoll_event ep;
                        ep.events = EPOLLIN;
                        ep.data.fd = win->display_fd;
                        epoll_ctl(win->epfd, EPOLL_CTL_MOD, win->display_fd, &ep);
                    } else if ( ret == -1 && errno != EAGAIN) {
                        perror("wl display flush failed: ");
                        break;
                    }
                }
            } else {
                if (!fd_handler_call(win->epfd, ep[i].data.fd, ep[i].events)) {
                    ERR("invalid fd handler is added");
                }
            }
        }
    }
}

void
wl_window_set_buffer(Wl_Window *win, unsigned char *data, unsigned int size, unsigned int w, unsigned h)
{
    RET_IF(!win);
    memcpy(win->main_map, data, size);
    /*
    wl_surface_damage(win->main_surface, 0, 0, w, h);
    wl_surface_commit(win->main_surface);
    */
}

int
wl_window_get_epoll_fd(Wl_Window *win)
{
    RET_IF(!win, -1);
    return win->epfd;
}

bool
wl_window_init()
{
    //if (!sigtimer_init()) return false;
    return true;
}

void
wl_window_shutdown()
{
    //sigtimer_shutdown();
}
