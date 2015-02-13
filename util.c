#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>    // stat
#include <sys/stat.h>  // mkdir, stat
#include <sys/types.h> // mkdir, stat
#include <libgen.h>    // dirname
#include "log.h"

#include <time.h>       // timer_create, timer_settime
#include <signal.h>     // sigaction
#include <cairo.h>

#include "nemolist.h"
#include "util.h"
#include "pixmanhelper.h"

/****************************************************/
/* String */
/***************************************************/
char*
_strdup_printf(const char *fmt, ...)
{
    va_list ap;
    char *str = NULL;
    unsigned int size = 0;

    while (1) {
        int n;
        va_start(ap, fmt);
        n = vsnprintf(str, size, fmt, ap);
        va_end(ap);

        if (n < 0) {
            ERR("vsnprintf failed");
            if (str) free(str);
            break;
        }
        if (n < size) break;

        size = n + 1;
        str = realloc(str, size);
    }
    return str;
}

/****************************************************/
/* File */
/***************************************************/
bool
_file_is_dir(const char *file)
{
    struct stat st;
    int r = stat(file, &st);
    if (r) {
        LOG("%s:%s", strerror(errno), file);
        return false;
    }
    return S_ISDIR(st.st_mode);
}

bool
_file_exist(const char *file)
{
    struct stat st;
    int r = stat(file, &st);
    if (r) return false;
    return true;
}

bool
_file_mkdir(const char *dir, int mode)
{
    RET_IF(!dir, false);
    bool ret;
    char *parent;

    if (_file_is_dir(dir)) {
        LOG("already exists: %s", dir);
        return true;
    }

    char *path = strdup(dir);
    parent = dirname(path);
    if (parent && strcmp(parent, ".") && strcmp(parent, "/")) {
        ret = _file_mkdir(parent, mode | 0300);
        if (!ret) {
            free(path);
            return false;
        }
    }
    free(path);

    ret = mkdir(dir, 0777);
    if (ret) {
        ERR("mkdir failed (%s): %s", dir, strerror(errno));
        return false;
    }

    if (mode != -1) {
        ret = chmod(dir, mode);
        if (ret) {
            ERR("chmod failed: %s: %s(%d)", strerror(errno), dir, mode);
            return false;
        }
    }
    return true;
}

bool
_file_mkdir_recursive(const char *file, int mode)
{
    RET_IF(!file, false);
    struct stat st;
    int r;

    r = stat(file, &st);
    if (r < 0 && errno == ENOENT) {
        bool ret;
        char *parent;

        char *path = strdup(file);
        parent = dirname(path);
        if (!parent) {
            free(path);
            return false;
        }

        if (strcmp(parent, ".") && strcmp(parent, "/")) {
            free(path);
            return true;
        }
        ret = _file_mkdir_recursive(parent, mode | 0300);

        if (!ret) {
            free(path);
            return false;
        }
        r = mkdir(path, 0777);
        if (r) {
            ERR("mkdir failed (%s): %s", path, strerror(errno));
            free(path);
            return false;
        }

        if (mode != -1) {
            r = chmod(path, mode);
            if (r) {
                ERR("chmod failed (%s) (%d): %s", path, mode, strerror(errno));
                free(path);
                return false;
            }
        }
    }
    return true;
}

/****************************************************/
/* Timer (signal implemented */
/***************************************************/
struct sigaction _sa_old;
int timer_init_cnt = 0;
struct nemolist timer_list;

struct __SigTimer
{
    struct nemolist link;
    timer_t *timerid;
    SigTimerCb callback;
    void *data;
};

static void
sigtimer_callback(int signo, siginfo_t *info, void *uctx)
{
    SigTimer *timer, *tmp;
    timer_t *timerid = info->si_value.sival_ptr;
    nemolist_for_each_safe(timer, tmp, &timer_list, link) {
        if (timer->timerid == timerid) {
            if (!timer->callback(timer->data))
                sigtimer_destroy(timer);
        } else {
            ERR("Unrecognized timerid: 0x%lx\n", (long) *timerid);
        }
    }
}

bool
sigtimer_init()
{
    if (timer_init_cnt > 0) return true;

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = sigtimer_callback;
    if (sigaction(SIGRTMIN, &sa, &_sa_old)) {
        perror("sigaction failed: ");
        return false;
    }

    timer_init_cnt++;
    nemolist_init(&timer_list);
    return true;
}

void
sigtimer_shutdown()
{
    if (timer_init_cnt <= 0) return;
    timer_init_cnt--;

    SigTimer *timer, *tmp;
    nemolist_for_each_safe(timer, tmp, &timer_list, link) {
        sigtimer_destroy(timer);
    }
    nemolist_empty(&timer_list);
    if (sigaction(SIGRTMIN, &_sa_old, NULL)) {
        perror("sigaction failed: ");
    }
}

SigTimer *
sigtimer_create(unsigned int mseconds, SigTimerCb callback, void *data)
{
    RET_IF(mseconds == 0, NULL);
    RET_IF(!callback, NULL);

    timer_t *timerid = malloc(sizeof(timer_t));
    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = timerid;
    if (timer_create(CLOCK_MONOTONIC, &sev, timerid)) {
        perror("timer_create failed: ");
        return NULL;
    }

    struct itimerspec its;
    its.it_value.tv_sec = mseconds/1000;
    its.it_value.tv_nsec = (mseconds%1000) * 1000000;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    if (timer_settime(*timerid, 0, &its, NULL)) {
        perror("timer_settime failed: ");
        timer_delete(*timerid);
        return NULL;
    }

    SigTimer *timer = calloc(sizeof(SigTimer), 1);
    timer->timerid = timerid;
    timer->callback = callback;
    timer->data = data;

    nemolist_insert(&timer_list, &timer->link);

    return timer;
}

void
sigtimer_destroy(SigTimer *timer)
{
    RET_IF(!timer);
    if (timer_delete(*(timer->timerid))) {
        perror("timer delete failed: ");
    }
    nemolist_remove(&timer->link);
    free(timer->timerid);
    free(timer);
}

/****************************************************/
/* Image */
/***************************************************/
struct _Image
{
   char *path;
   pixman_image_t *pimg;
   cairo_surface_t *surface;
   unsigned int width;
   unsigned int height;
   cairo_format_t format;
   unsigned int stride;
};

cairo_surface_t *
image_get_surface(Image *img)
{
    RET_IF(!img, NULL);
    return img->surface;
}

Image *
image_create(const char *path)
{
   RET_IF(!path, NULL);
   int pw, ph;

   // FIXME: file type recognization
   pixman_image_t *pimg = pixman_load_jpeg_file(path);
   if (!pimg) {
       pimg = pixman_load_png_file(path);
   }
   if (!pimg) {
       ERR("Image load failed: %s", path);
       return NULL;
   }

   pw = pixman_image_get_width(pimg);
   ph = pixman_image_get_height(pimg);
   unsigned char *data = (unsigned char *)pixman_image_get_data(pimg);

   cairo_format_t format = CAIRO_FORMAT_ARGB32;
   int stride = cairo_format_stride_for_width (format, pw);
   cairo_surface_t *surface = cairo_image_surface_create_for_data(data, format, pw, ph, stride);

   Image *img = calloc(sizeof(Image), 1);
   img->path = strdup(path);
   img->pimg = pimg;
   img->format = format;
   img->stride = stride;
   img->surface = surface;
   return img;
}

void
image_destroy(Image *img)
{
   RET_IF(!img);
   free(img->path);
   cairo_surface_destroy(img->surface);
   pixman_image_unref(img->pimg);
   free(img);
}


