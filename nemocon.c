#include <stdint.h>
#include <string.h>
#include <curl/curl.h>
#include <nemotool.h>
#include <nemolist.h>
#include <nemotimer.h>
#include <sys/epoll.h>

#include "log.h"
#include "util.h"


typedef struct _NemoCon NemoCon;
typedef void (*NemoCon_Data_Callback)(NemoCon *con, char *data, size_t size, void *userdata);
typedef void (*NemoCon_End_Callback)(NemoCon *con, void *userdata);

struct _NemoCon {
    struct nemolist link;

    struct nemotool *tool;
    CURL *curl;

    char *src;

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
    if (con->src) free(con->src);
    nemolist_remove(&con->link);
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
                    LOG("completed: src(%s)", con->src);
                    // FIXME: callback is needed!!
                    if (con->end_callback)
                        con->end_callback(con, con->end_userdata);
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

    code = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30);
    if (code) ERR("%s", curl_easy_strerror(code));
    code = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    if (code) ERR("%s", curl_easy_strerror(code));

    mcode = curl_multi_add_handle(_curlm, curl);
    if (mcode != CURLM_OK) {
        ERR("curl multi add handle failed: %s", curl_multi_strerror(mcode));
        curl_easy_cleanup(curl);
        return NULL;
    }

    NemoCon *con = calloc(sizeof(NemoCon), 1);
    con->curl = curl;
    con->tool = tool;
    con->fd_task.dispatch = _nemocon_fd_callback;
    nemolist_insert(&_nemocon_lists, &(con->link));
    return con;
}

void
nemocon_set_src(NemoCon *con, const char *src)
{
    CURLcode code;
    code = curl_easy_setopt(con->curl, CURLOPT_URL, src);
    if (code) ERR("%s", curl_easy_strerror(code));

    if (con->src) free(con->src);
    con->src = strdup(src);
}

void
nemocon_set_end_callback(NemoCon *con, NemoCon_End_Callback callback, void *userdata)
{
    con->end_callback = callback;
    con->end_userdata = userdata;
}

static size_t
_nemocon_write_function(char *ptr, size_t size, size_t nememb, void *userdata)
{
    NemoCon *con = userdata;
    if (!ptr || (size == 0) || (nememb == 0)) return 0;
    con->data_callback(con, ptr, size*nememb, con->data_userdata);
    return size * nememb;
}

void
nemocon_set_data_callback(NemoCon *con, NemoCon_Data_Callback callback, void *userdata)
{
    CURLcode code;
    code = curl_easy_setopt(con->curl, CURLOPT_WRITEFUNCTION, _nemocon_write_function);
    if (code) ERR("%s", curl_easy_strerror(code));
    code = curl_easy_setopt(con->curl, CURLOPT_WRITEDATA, con);
    if (code) ERR("%s", curl_easy_strerror(code));

    con->data_callback = callback;
    con->data_userdata = userdata;
}

static void
data_cb(NemoCon *con, char *data, size_t size, void *userdata)
{
    //ERR("[%p]: %p, %zud", con, data, size);
}

int main()
{
    struct nemotool *tool = nemotool_create();
    nemotool_connect_wayland(tool, NULL);

    NemoCon *con = nemocon_create(tool);
    nemocon_set_src(con, "http://www.google.co.kr");
    nemocon_set_data_callback(con, data_cb, con);

    con = nemocon_create(tool);
    nemocon_set_src(con, "http://www.naver.com");
    nemocon_set_data_callback(con, data_cb, con);

    con = nemocon_create(tool);
    nemocon_set_src(con, "http://www.daum.net");
    nemocon_set_data_callback(con, data_cb, con);

    nemocon_run(con);

    nemotool_run(tool);
    nemotool_disconnect_wayland(tool);
    nemotool_destroy(tool);

    return 0;
}

