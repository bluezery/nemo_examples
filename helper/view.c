#include <cairo.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "log.h"
#include "nemolist.h"
#include "wl_window.h"
#include "view.h"

struct __View
{
    cairo_surface_t *surf;
    cairo_t *cr;
    int w, h, stride;
    Wl_Window *win;
};

View *
view_create(int w, int h, unsigned int br, unsigned int bg, unsigned int bb, unsigned int ba)
{
    cairo_surface_t *surf;
    cairo_t *cr;

    surf = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
    cairo_status_t status = cairo_surface_status(surf);
    if (!surf || status != CAIRO_STATUS_SUCCESS) {
        ERR("error:%s", cairo_status_to_string(status));
        if (surf) cairo_surface_destroy(surf);
        return NULL;
    }

    cr = cairo_create(surf);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, br / 255., bg / 255., bb / 255., ba / 255.);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr, 0, 0, 0, 1);

    int stride;
    stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, w);

    Wl_Window *win = wl_window_create(w, h, stride);
    if (!win){
        ERR("wl_window_create failed");
        cairo_destroy(cr);
        cairo_surface_destroy(surf);
        return NULL;
    }
    unsigned char *data = cairo_image_surface_get_data(surf);
    wl_window_set_buffer(win, data, h * stride, w, h);

    View *view = calloc(sizeof(View), 1);
    view->surf = surf;
    view->cr = cr;
    view->w = w;
    view->h = h;
    view->stride = stride;
    view->win = win;
    return view;
}

void
view_destroy(View *view)
{
    RET_IF(!view);
    wl_window_destroy(view->win);
    cairo_destroy(view->cr);
    cairo_surface_destroy(view->surf);
    free(view);
}

void
view_update(View *view)
{
    unsigned char *data = cairo_image_surface_get_data(view->surf);
    wl_window_set_buffer(view->win, data, view->h * view->stride, view->w, view->h);
}

void
view_do(View *view)
{
    RET_IF(!view);

    view_update(view);
    wl_window_loop(view->win);
}

cairo_t *
view_get_cairo(View *view)
{
    RET_IF(!view, NULL);
    return view->cr;
}

cairo_surface_t *
view_get_surface(View *view)
{
    RET_IF(!view, NULL);
    return view->surf;
}

bool
view_init()
{
    if (!fd_handler_init()) return false;
    if (!wl_window_init()) {
        fd_handler_shutdown();
        return false;
    }
    return true;
}

void
view_shutdown()
{
    wl_window_shutdown();
    fd_handler_shutdown();
}

/*************************************/
/** Fd Handler **/
/*************************************/
struct _FdHandler
{
    struct nemolist link;
    int epfd;
    int fd;
    FdCallback callback;
    void *data;
};

struct nemolist fd_handler_list;

bool
fd_handler_init()
{
    nemolist_init(&fd_handler_list);
    return true;
}

void
fd_handler_shutdown()
{
    FdHandler *fdh, *tmp;
    nemolist_for_each_safe(fdh, tmp, &fd_handler_list, link) {
        fd_handler_destroy(fdh);
    }
    nemolist_empty(&fd_handler_list);
}

void
fd_handler_destroy(FdHandler *fdh)
{
    RET_IF(!fdh);

    if (epoll_ctl(fdh->epfd, EPOLL_CTL_DEL, fdh->fd, NULL) < 0)
        perror("epoll_ctl failed: ");
    nemolist_remove(&fdh->link);
    free(fdh);
}

bool
fd_handler_call(int epfd, int fd, uint32_t events)
{
    FdHandler *fdh, *tmp;
    nemolist_for_each_safe(fdh, tmp, &fd_handler_list, link) {
        if ((epfd == fdh->epfd) && (fdh->fd == fd)) {
            if (!fdh->callback(events, fdh->data)) {
                fd_handler_destroy(fdh);
            }
            return true;
        }
    }
    return false;
}

FdHandler *
fd_handler_attach(View *view, unsigned int fd, uint32_t events, FdCallback callback, void *data)
{
    RET_IF(!view, NULL);
    RET_IF(!callback, NULL);

    int epfd;
    struct epoll_event ep;

    epfd = wl_window_get_epoll_fd(view->win);

    ep.events = events;
    ep.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ep) < 0) {
        perror("epoll_ctl failed: ");
        return NULL;
    }

    FdHandler *fdh = calloc(sizeof(FdHandler), 1);
    fdh->callback = callback;
    fdh->data = data;
    fdh->fd = fd;
    fdh->epfd = epfd;
    nemolist_insert(&fd_handler_list, &fdh->link);
    return fdh;
}

/*************************************/
/** Timer Handler **/
/*************************************/
struct _Timer
{
    struct nemolist link;
    FdHandler *fdh;
    Callback callback;
    void *data;
};

struct nemolist timer_list;

bool
timer_init()
{
    nemolist_init(&timer_list);
    return true;
}

void
timer_shutdown()
{
    Timer *timer, *tmp;
    nemolist_for_each_safe(timer, tmp, &timer_list, link) {
        timer_destroy(timer);
    }
    nemolist_empty(&timer_list);
}

void
timer_destroy(Timer *timer)
{
    RET_IF(!timer);
    fd_handler_destroy(timer->fdh);
    free(timer);
}

static bool
_timer_callback(uint32_t events, void *data)
{
    Timer *timer = data;
    if (!timer->callback(timer->data)) {
        timer_destroy(timer);
        return false;
    }
    return true;
}

Timer *
timer_attach(View *view, unsigned int mseconds, Callback callback, void *data)
{
    RET_IF(!view, NULL);
    RET_IF(!mseconds, NULL);
    RET_IF(!callback, NULL);

    int fd;
    fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (fd < 0) {
        perror("timerfd_create failed: ");
        return NULL;
    }

    struct itimerspec its;
    its.it_value.tv_sec = mseconds/1000;
    its.it_value.tv_nsec = (mseconds%1000) * 1000000;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    if (timerfd_settime(fd, 0, &its, NULL) < 0) {
        perror("timerfd_settime failed: ");
        close(fd);
        return NULL;
    }

    Timer *timer = calloc(sizeof(Timer), 1);
    timer->callback = callback;
    timer->data = data;
    timer->fdh = fd_handler_attach(view, fd, EPOLLIN, _timer_callback, timer);
    nemolist_insert(&timer_list, &timer->link);

    return timer;
}
