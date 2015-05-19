#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>    // stat
#include <sys/stat.h>  // mkdir, stat
#include <sys/types.h> // mkdir, stat
#include <libgen.h>    // dirname
#include <stdarg.h>

#include <time.h>       // timer_create, timer_settime
#include <signal.h>     // sigaction
#include <cairo.h>

#include <nemotool.h>
#include <nemolist.h>
#include <nemotimer.h>

#include <curl/curl.h>
#include "log.h"
#include "util.h"
//#include <pixmanhelper.h>

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

char **
_file_load(const char *file, int *line_len)
{

    FILE *fp;
    char **line = NULL;
    int idx = 0;
    char *buffer = NULL;
    size_t buffer_len;

    RET_IF(!file || !line_len, NULL);

    fp = fopen(file, "r");
    if (!fp) {
        ERR("%s", strerror(errno));
        return NULL;
    }

    buffer_len = 2048; // adequate size for normal file case.
    buffer = (char *)malloc(sizeof(char) * buffer_len);

    while (getline(&buffer, &buffer_len, fp) >= 0) {
        line = (char **)realloc(line, sizeof(char *) * (idx + 1));
        line[idx] = strdup(buffer);
        idx++;
    }
    *line_len = idx;

    free(buffer);
    fclose(fp);
    return line;
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

/*******************************************
 * Nemo Connection *
 * *****************************************/
struct _NemoCon {
    struct nemolist link;

    struct nemotool *tool;
    CURL *curl;

    char *url;

    size_t data_size;
    void *data;
    struct nemotask fd_task;
    NemoCon_Data_Callback data_callback;
    void *data_userdata;
    NemoCon_End_Callback end_callback;
    void *end_userdata;
};

CURLM *_curlm;
List *_curlm_fd_lists;
struct nemotimer *_curlm_timer;

struct nemolist _nemocon_lists;

void
nemocon_destroy(NemoCon *con)
{
    curl_easy_cleanup(con->curl);
    if (con->url) free(con->url);
    nemolist_remove(&con->link);
    if (con->data) free(con->data);
    free(con);
}

static void _nemocon_perform(NemoCon *con);

static void
_nemocon_fd_callback(struct nemotask *task, uint32_t events)
{
    NemoCon *con = container_of(task, NemoCon, fd_task);
    _nemocon_perform(con);
}

static void
_nemocon_timer_callback(struct nemotimer *timer, void *userdata)
{
    NemoCon *con = userdata;
    _nemocon_perform(con);
}

static void
_nemocon_set_fd(NemoCon *con)
{
    CURLMcode code;
    int max_fd;
    fd_set fd_read;
    fd_set fd_write;
    fd_set fd_err;

    FD_ZERO(&fd_read);
    FD_ZERO(&fd_write);
    FD_ZERO(&fd_err);

    code = curl_multi_fdset(_curlm, &fd_read, &fd_write, &fd_err, &max_fd);
    if (code != CURLM_OK) {
        ERR("curl multi fdset failed: %s", curl_multi_strerror(code));
        return;
    }

    List *l, *tmp;
    void *fdp;
    LIST_FOR_EACH_SAFE(_curlm_fd_lists, l, tmp, fdp) {
        nemotool_unwatch_fd(con->tool, (int)(intptr_t)fdp);
        _curlm_fd_lists = list_data_remove(_curlm_fd_lists, fdp);
    }

    int fd = 0;
    for (fd = 0 ; fd < max_fd ; fd++) {
        uint32_t events = 0;
        if (FD_ISSET(fd, &fd_read)) events |= EPOLLIN;
        if (FD_ISSET(fd, &fd_write)) events |= EPOLLOUT;
        if (FD_ISSET(fd, &fd_err)) events |= EPOLLERR;
        if (!events) continue;
        nemotool_watch_fd(con->tool, fd, events, &(con->fd_task));
        _curlm_fd_lists = list_data_insert(_curlm_fd_lists, (void *)(intptr_t)fd);
    }
}

#define MIN_TIMEOUT 100
static void
_nemocon_set_timeout(NemoCon *con)
{
    CURLMcode code;
    long tout;
    code = curl_multi_timeout(_curlm, &tout);
    if (code == CURLM_OK) {
        if (tout <= 0 || tout >= MIN_TIMEOUT) tout = MIN_TIMEOUT;
    } else {
        ERR("curl multi timeout failed: %s", curl_multi_strerror(code));
        tout = MIN_TIMEOUT;
    }
    if (_curlm_timer) nemotimer_set_timeout(_curlm_timer, tout);
}

static void
_nemocon_perform(NemoCon *con)
{
    CURLMcode code;
    int still_running;
    code = curl_multi_perform(_curlm, &still_running);
    if (code == CURLM_CALL_MULTI_PERFORM) {
        LOG("perform again");
    } else if (code != CURLM_OK) {
        ERR("curl multi perform failed: %s", curl_multi_strerror(code));
        return;
    }

    int n_msg;
    CURLMsg *msg;
    while ((msg = curl_multi_info_read(_curlm, &n_msg))) {
        if (msg->msg == CURLMSG_DONE) {
            NemoCon *con, *tmp;
            nemolist_for_each_safe(con, tmp, &_nemocon_lists, link) {
                if (con->curl == msg->easy_handle) {
                    LOG("completed: url(%s)", con->url);
                    // FIXME: callback is needed!!
                    if (con->end_callback)
                        con->end_callback(con, con->data, con->data_size, con->end_userdata);
                    nemocon_destroy(con);
                }
            }
        }
    }

    if (!still_running) {
        LOG("Multi perform ended");
        if (_curlm_timer) nemotimer_destroy(_curlm_timer);
        _curlm_timer = NULL;

        List *l, *tmp;
        void *fdp;
        LIST_FOR_EACH_SAFE(_curlm_fd_lists, l, tmp, fdp) {
            ERR("%d", (int)(intptr_t)fdp);
            nemotool_unwatch_fd(con->tool, (int)(intptr_t)fdp);
            _curlm_fd_lists = list_data_remove(_curlm_fd_lists, fdp);
        }
        return;
    }

    _nemocon_set_fd(con);
    _nemocon_set_timeout(con);
}

void
nemocon_run(NemoCon *con)
{
    _nemocon_set_fd(con);

    if (!_curlm_timer) _curlm_timer = nemotimer_create(con->tool);
    nemotimer_set_callback(_curlm_timer, _nemocon_timer_callback);
    nemotimer_set_userdata(_curlm_timer, con);
    _nemocon_set_timeout(con);
}

static size_t
_nemocon_write_function(char *ptr, size_t size, size_t nememb, void *userdata)
{
    NemoCon *con = userdata;
    if (!ptr || (size == 0) || (nememb == 0)) return 0;
    if (con->data_callback)
        con->data_callback(con, ptr, size*nememb, con->data_userdata);

    con->data = realloc(con->data, con->data_size + size*nememb);
    memcpy(con->data + con->data_size, ptr, size*nememb);
    con->data_size += (size * nememb);
    return size * nememb;
}

NemoCon *
nemocon_create(struct nemotool *tool)
{
    CURLMcode mcode;
    CURLcode code;

    if (!_curlm) {
        code = curl_global_init(CURL_GLOBAL_ALL);
        if (code) {
            ERR("curl global init failed: %s", curl_easy_strerror(code));
            return false;
        }

        _curlm = curl_multi_init();
        if (!_curlm) {
            ERR("curl multi init failed");
            curl_global_cleanup();
            return NULL;
        }
        nemolist_init(&_nemocon_lists);
    }

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    NemoCon *con = calloc(sizeof(NemoCon), 1);

    // if (code = curl_easy_setopt(curl, CURLOPT_VERBOSE, 1)) // debugging
    //    ERR("%s", curl_easy_strerror(code));
#if 0
    //code = curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, _curl_progress_func);
    code = curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, NULL);
    if (code) ERR("%s", curl_easy_strerror(code));

    //code = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, _curl_header_func);
    code = curl_easy_setopt(curl, CURLOPT_HEADERDATA, NULL);
    if (code) ERR("%s", curl_easy_strerror(code));
#endif
    code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _nemocon_write_function);
    if (code) ERR("%s", curl_easy_strerror(code));
    code = curl_easy_setopt(curl, CURLOPT_WRITEDATA, con);
    if (code) ERR("%s", curl_easy_strerror(code));

    code = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30);
    if (code) ERR("%s", curl_easy_strerror(code));
    code = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    if (code) ERR("%s", curl_easy_strerror(code));

    mcode = curl_multi_add_handle(_curlm, curl);
    if (mcode != CURLM_OK) {
        ERR("curl multi add handle failed: %s", curl_multi_strerror(mcode));
        curl_easy_cleanup(curl);
        free(con);
        return NULL;
    }

    con->curl = curl;
    con->tool = tool;
    con->fd_task.dispatch = _nemocon_fd_callback;
    nemolist_insert(&_nemocon_lists, &(con->link));
    return con;
}

void
nemocon_set_url(NemoCon *con, const char *url)
{
    CURLcode code;
    code = curl_easy_setopt(con->curl, CURLOPT_URL, url);
    if (code) ERR("%s", curl_easy_strerror(code));

    if (con->url) free(con->url);
    con->url = strdup(url);
}

void
nemocon_set_end_callback(NemoCon *con, NemoCon_End_Callback callback, void *userdata)
{
    con->end_callback = callback;
    con->end_userdata = userdata;
}

void
nemocon_set_data_callback(NemoCon *con, NemoCon_Data_Callback callback, void *userdata)
{
    con->data_callback = callback;
    con->data_userdata = userdata;
}
#if 0
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

#endif
