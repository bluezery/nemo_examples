#include <stdio.h> // snprintf
#include <string.h> // strdup
#include <math.h>   // PI
#include <unistd.h> // write, usleep
#include <errno.h> // errno
#include <stdlib.h> // malloc

#include <curl/curl.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "view.h"
#include "log.h"
#include "util.h"
#include "nemolist.h"

#if 0
// Refer : http://wiki.openstreetmap.org/wiki/FAQ
// meters per pixel when latitude is 0 (equator)
// meters per pixel  = _osm_scale_meter[zoom] * cos (latitude)
const double _osm_scale_meter[] =
{
   78206, 39135.758482, 19567.879241, 9783.939621, 4891.969810,
   2445.984905, 1222.992453, 611.496226, 305.748113, 152.874057, 76.437028,
   38.218514, 19.109257, 9.554629, 4.777314, 2.388657, 1.194329, 0.597164,
   0.29858
};

// Scale in meters
const double _scale_tb[] =
{
   10000000, 5000000, 2000000, 1000000, 500000, 200000, 100000, 50000,
   20000, 10000, 5000, 2000, 1000, 500, 500, 200, 100, 50, 20, 10, 5, 2, 1
};

// URL References
// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
// http://wiki.openstreetmap.org/wiki/Tileserver
static double
_scale_cb(double lon, double lat, int zoom)
{
   if ((zoom < 0) ||
       (zoom >= (int)(sizeof(_osm_scale_meter) / sizeof(_osm_scale_meter[0])))
      )
     return 0;
   return _osm_scale_meter[zoom] / cos(lat * M_PI / 180.0);
}

static char *
_map_url_get_mapnik(int x, int y, int zoom)
{
   char buf[PATH_MAX];
   // ((x+y+zoom)%3)+'a' is requesting map images from distributed
   // tile servers (eg., a, b, c)
   snprintf(buf, sizeof(buf),
           "http://%c.tile.openstreetmap.org/%d/%d/%d.png",
            ((x + y + zoom) % 3) + 'a', zoom, x, y);
   return strdup(buf);
}

static char *
_map_url_get_osmarender(int x, int y, int zoom)
{
   char buf[PATH_MAX];
   snprintf(buf, sizeof(buf),
            "http://%c.tah.openstreetmap.org/Tiles/tile/%d/%d/%d.png",
            ((x + y + zoom) % 3) + 'a', zoom, x, y);
   return strdup(buf);
}

static char *
_map_url_get_cyclemap(int x, int y, int zoom)
{
   char buf[PATH_MAX];
   snprintf(buf, sizeof(buf),
            "http://%c.tile.opencyclemap.org/cycle/%d/%d/%d.png",
            ((x + y + zoom) % 3) + 'a', zoom, x, y);
   return strdup(buf);
}

static char *
_map_url_get_mapquest_aerial(int x, int y, int zoom)
{
   char buf[PATH_MAX];
   snprintf(buf, sizeof(buf),
           "http://oatile%d.mqcdn.com/naip/%d/%d/%d.png",
            ((x + y + zoom) % 4) + 1, zoom, x, y);
   return strdup(buf);
}
#endif
static char *
_map_url_get_mapquest(unsigned int zoom, unsigned int x, unsigned int y)
{
   char buf[PATH_MAX];
   snprintf(buf, sizeof(buf),
            "http://otile%d.mqcdn.com/tiles/1.0.0/osm/%d/%d/%d.png",
            ((x + y + zoom) % 4) + 1, zoom, x, y);
   return strdup(buf);
}

#if 0
static size_t
_curl_write_func(void *data, size_t item_size, size_t num_items, void *userdata)
{
    FILE *fp = userdata;
    RET_IF(!fp, 0);

    //size_t size = item_size * num_items;
    size_t size = item_size * num_items;
    size_t offset = 0;
    while (size > 0) {
        ssize_t done = fwrite(data + offset, 1, size, fp);
        if (done < 0) {
            ERR("%s", strerror(errno));
            if ((errno != EAGAIN) && (errno != EINTR)) break;
        } else {
            size -= done;
            offset += done;
        }
    }
    if (fflush(fp)) {
        ERR("%s", strerror(errno));
    }
    return offset;
}
#endif

CURLM *curlm;
List *curlm_fd_lists;
struct nemolist file_download_lists;
Timer *curlm_timer;

typedef struct _FileDownloader FileDownloader;
typedef void (*FileDownloaderEnd)(FileDownloader *fd, const char *filename, void *data);

struct _FileDownloader {
    struct nemolist link;
    CURL *curl;
    char *url;
    FILE *fp;
    char *filename;
    FileDownloaderEnd callback_end;
    void *data_end;
};

static bool
_file_download_init()
{
    CURLcode ret;
    ret = curl_global_init(CURL_GLOBAL_ALL);
    if (ret) {
        ERR("curl global init failed: %s", curl_easy_strerror(ret));
        return false;
    }

    curlm = curl_multi_init();
    if (!curlm) return false;

    nemolist_init(&file_download_lists);
    return true;
}

static void _file_download_destroy(FileDownloader *fd);

static void
_file_download_read_info()
{
    int n_msg;

    CURLMsg *msg;
    while ((msg = curl_multi_info_read(curlm, &n_msg))) {
        if (msg->msg == CURLMSG_DONE) {
            FileDownloader *fd, *tmp;
            nemolist_for_each_safe(fd, tmp, &file_download_lists, link) {
                if (fd->curl == msg->easy_handle) {
                    LOG("completed %s", fd->filename);
                    fclose(fd->fp);
                    fd->fp = NULL;
                    // FIXME: callback is needed!!
                    if (fd->callback_end) fd->callback_end(fd, fd->filename, fd->data_end);
                    //_file_download_destroy(fd);
                }
            }
        }
    }
}

static bool
_file_download_perform()
{
    CURLMcode code;
    int still_running;
    code = curl_multi_perform(curlm, &still_running);
    if (code == CURLM_CALL_MULTI_PERFORM) {
        LOG("perform again");
        return true;
    } else if (code != CURLM_OK) {
        ERR("curl multi perform failed: %s", curl_multi_strerror(code));
        return true;
        // END
        //break;
    }
    if (!still_running) {
        LOG("Multi perform ended");
        if (curlm_timer) timer_destroy(curlm_timer);
        curlm_timer = NULL;
        List *l, *tmp;
        FdHandler *fdh;
        LIST_FOR_EACH_SAFE(curlm_fd_lists, l, tmp, fdh) {
            fd_handler_destroy(fdh);
            list_data_remove(curlm_fd_lists, fdh);
        }
        return true;
        // END
        //break;
    }
    _file_download_read_info();

    return true;
}

static bool
_file_download_timer(void *data)
{
    return _file_download_perform();
}

static bool
_file_download_fd_handler(uint32_t events, void *data)
{
    return _file_download_perform();
}

static void
_file_download_set_fd(View *view)
{
    CURLMcode code;
    int max_fd;
    fd_set fd_read;
    fd_set fd_write;
    fd_set fd_err;

    FD_ZERO(&fd_read);
    FD_ZERO(&fd_write);
    FD_ZERO(&fd_err);

    code = curl_multi_fdset(curlm, &fd_read, &fd_write, &fd_err, &max_fd);
    if (code != CURLM_OK) {
        ERR("curl multi fdset failed: %s", curl_multi_strerror(code));
        return;
    }

    List *l, *tmp;
    FdHandler *fdh;
    LIST_FOR_EACH_SAFE(curlm_fd_lists, l, tmp, fdh) {
        fd_handler_destroy(fdh);
        list_data_remove(curlm_fd_lists, fdh);
    }

    for (int fd = 0 ; fd < max_fd ; fd++) {
        uint32_t events = 0;
        if (FD_ISSET(fd, &fd_read)) events |= EPOLLIN;
        //if (FD_ISSET(fd, &fd_write)) events |= EPOLLOUT;
        //if (FD_ISSET(fd, &fd_err)) events |= EPOLLERR;
        if (!events) break;
        fdh = fd_handler_attach(view, fd, events, _file_download_fd_handler, view);
        curlm_fd_lists = list_data_insert(curlm_fd_lists, fdh);
    }
}

static void
_file_download_set_timeout(View *view)
{
    CURLMcode code;
    long tout;
    code = curl_multi_timeout(curlm, &tout);
    if (code == CURLM_OK) {
        if (tout < 0) tout = 0;
    } else {
        ERR("curl multi timeout failed: %s", curl_multi_strerror(code));
        tout = 100;
    }
    if (curlm_timer) timer_destroy(curlm_timer);
    curlm_timer = timer_attach(view, tout, _file_download_timer, NULL);
}

static FileDownloader *
_file_download_create(View *view, const char *src, const char *dst, unsigned int timeout, FileDownloaderEnd callback_end, void *data_end)
{
    RET_IF(!src, false);
    RET_IF(!dst, false);

    FILE *fp;
    CURLMcode code;
    CURLcode ret;
    CURL *easy;
#if 1
    if (_file_exist(dst)) {
        LOG("file already downloaded: %s", dst);
        if (callback_end) callback_end(NULL, dst, data_end);
        return NULL;
    }
#endif
    fp = fopen(dst, "w+");
    if (!fp) {
        ERR("%s", strerror(errno));
        return NULL;
    }

    easy = curl_easy_init();
    if (!easy) {
        return NULL;
    }
    // if (ret = curl_easy_setopt(easy, CURLOPT_VERBOSE, 1)) // debugging
    //    ERR("%s", curl_easy_strerror(ret));
    ret = curl_easy_setopt(easy, CURLOPT_URL, src);
    if (ret) ERR("%s", curl_easy_strerror(ret));
    ret = curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, NULL);
    if (ret) ERR("%s", curl_easy_strerror(ret));
    ret = curl_easy_setopt(easy, CURLOPT_WRITEDATA, fp);
    if (ret) ERR("%s", curl_easy_strerror(ret));
    //ret = curl_easy_setopt(easy, CURLOPT_PROGRESSFUNCTION, _curl_progress_func);
    ret = curl_easy_setopt(easy, CURLOPT_PROGRESSDATA, NULL);
    if (ret) ERR("%s", curl_easy_strerror(ret));
    //ret = curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, _curl_header_func);
    ret = curl_easy_setopt(easy, CURLOPT_HEADERDATA, NULL);
    if (ret) ERR("%s", curl_easy_strerror(ret));
    ret = curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, timeout);
    if (ret) ERR("%s", curl_easy_strerror(ret));
    ret = curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1);
    if (ret) ERR("%s", curl_easy_strerror(ret));

    code = curl_multi_add_handle(curlm, easy);
    if (code != CURLM_OK) {
        ERR("curl multi add handle failed: %s", curl_multi_strerror(code));
        curl_easy_cleanup(easy);
        return NULL;
    }

    FileDownloader *fd = malloc(sizeof(FileDownloader));
    fd->curl = easy;
    fd->fp = fp;
    fd->url = strdup(src);
    fd->filename = strdup(dst);
    fd->callback_end = callback_end;
    fd->data_end = data_end;
    LOG("%s", fd->url);
    nemolist_insert(&file_download_lists, &fd->link);

#if 0
    _file_download_set_fd(view);
    _file_download_set_timeout(view);
#endif

    return fd;
}

static void
_file_download_destroy(FileDownloader *fd)
{
    CURLMcode code;
    RET_IF(!fd);
    if (fd->curl) {
        code = curl_multi_remove_handle(curlm, fd->curl);
        if (code != CURLM_OK)
            ERR("curl multi remove handle failed: %s",
                    curl_multi_strerror(code));
        curl_easy_cleanup(fd->curl);
    }
    if (fd->fp)fclose(fd->fp);
    free(fd->filename);
    free(fd->url);
    nemolist_remove(&fd->link);
}

static void
_file_download_cleanup()
{
    FileDownloader *fd, *tmp;
    nemolist_for_each_safe(fd, tmp, &file_download_lists, link) {
        _file_download_destroy(fd);
    }
    nemolist_empty(&file_download_lists);

    if (curlm) curl_multi_cleanup(curlm);
    curl_global_cleanup();
}

static void
_file_download_do()
{
    CURLMcode code;
    int still_running;
    while (1) {

        int max_fd;
        fd_set fd_read;
        fd_set fd_write;
        fd_set fd_err;

        FD_ZERO(&fd_read);
        FD_ZERO(&fd_write);
        FD_ZERO(&fd_err);

        int r;
        code = curl_multi_fdset(curlm, &fd_read, &fd_write, &fd_err, &max_fd);
        if (code != CURLM_OK) {
            ERR("curl multi fdset failed: %s", curl_multi_strerror(code));
            break;
        }

        struct timeval twait;
        long tout;
        code = curl_multi_timeout(curlm, &tout);
        if (code == CURLM_OK) {
            if (tout >= 0) {
                twait.tv_sec = tout / 1000;
                if (twait.tv_sec > 1) twait.tv_sec =1;
                else {
                    twait.tv_usec = (tout % 1000) * 1000;
                }
            }
        } else {
            ERR("curl multi timeout failed: %s", curl_multi_strerror(code));
            twait.tv_usec = 100000; // minimum: 100 milliseconds.
        }

        // FIXME: Use nemo's internal select loop
        if (max_fd == -1) {
            // Wait at least 100 milliseconds.
            struct timeval twait = {0, 100000};
            r = select(0, NULL, NULL, NULL, &twait);
        } else {
            r = select(max_fd + 1, &fd_read, &fd_write, &fd_err, &twait);
        }
        if (r == -1) {
            ERR("select failed: %d", r);
            break;
        }

        code = curl_multi_perform(curlm, &still_running);
        if (code == CURLM_CALL_MULTI_PERFORM) {
            LOG("perform again");
            continue;
        } else if (code != CURLM_OK) {
            ERR("curl multi perform failed: %s", curl_multi_strerror(code));
            break;
        }
        if (!still_running) {
            LOG("Multi perform ended");
            break;
        }
        _file_download_read_info();
    }
}

typedef struct _Tile
{
    unsigned int zoom;
    unsigned int w, h;  // Grid width, height
} Tile;

typedef struct _TileItem
{
    const char *url;
    const char *filename;
    unsigned int x, y;
} TileItem;

static int tileitem_size = 255;

static void
_map_tilecoord_to_region(unsigned int zoom,
                         int x, int y,
                         double *lon, double *lat)
{
    double size = pow(2, zoom) * tileitem_size;

    LOG("%lf", size);
    // Referece: http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
    if (lon) *lon = (x / size * 360.0) - 180;
    if (lat) {
        double n = M_PI - (2.0 * M_PI * y / size);
        *lat = 180.0 / M_PI *atan(0.5 * (exp(n) - exp(-n)));
    }
}

static void
_map_region_to_tilecoord(double zoom,
                         double lon, double lat,
                         int *x, int *y)
{
    double size = pow(2, zoom) * tileitem_size;

    if (x) *x = floor((lon + 180.0) / 360.0 * size);
    if (y) {
        *y = floor((1.0 - log(tan(lat * M_PI / 180.0)
                              + (1.0 / cos(lat * M_PI / 180.0)))
                           / M_PI) / 2.0 * size);
    }
}

typedef struct _ImageData {
    View *view;
    cairo_t *cr;
    int ux, uy;
} ImageData;

static void
_img_downloaded(FileDownloader *fd, const char *filename, void *data)
{
    //ImageData *id = data;
    //Image *img = image_create(filename);
    //LOG("Image downloaded: %s, %p", filename, img);
    //cairo_set_source_surface(id->cr, image_get_surface(img), id->ux, id->uy);
    //view_update(id->view);
    //free(id);
}

int main()
{
    cairo_t *cr;

    int w = 600, h = 600;

    // Set default download path
    const char *home = getenv("HOME");
    char *path;
    if (home) path = _strdup_printf("%s/.map", home);
    else      path = _strdup_printf("/tmp/.map");
    if (!_file_mkdir(path, 0755)) {
        ERR("file mkdir failed: %s", path);
        return -1;
    }
    _file_download_init();

    double zoom = 16;
    double lat = 37.3691131, lon = -122.0241857;
    //double lon = 126.9761088, lat = 37.2929565;
    int tx, ty;

    // Get left,top position in the tile coordinate
    _map_region_to_tilecoord(zoom, lon, lat, &tx, &ty);
    //_map_tilecoord_to_region(5, x, y, &lon, &lat);

    //LOG("%lf: %lf %lf <-> %u %u", zoom, lon, lat, x, y);

    LOG("%s", path);
    // Tile Index
    int tix, tiy;
    tix = tx/tileitem_size;

    // left, top position in the user coordinate (inside window)
    int ux, uy;
    ux = (w - tileitem_size)/2;

    while (tix >= 0) {
        tix--;
        if (tix < 0) {
            tix = 0;
            break;
        }
        ux -= tileitem_size;
        if (ux < 0) break;
    }

    view_init();
    View *v = view_create(0, w, h, 255, 255, 255, 255);
    cr = view_get_cairo(v);

    // Create download list
    for ( ; (ux < w) && (tix <= (pow(2, zoom) - 1)) ; ux += tileitem_size, tix++) {
        tiy = ty/tileitem_size;
        uy = (h - tileitem_size)/2;
        while (tiy >= 0) {
            tiy--;
            if (tiy < 0) {
                tiy = 0;
                break;
            }
            uy -= tileitem_size;
            if (uy < 0) break;
        }
        for ( ; (uy < h) && (tiy <= pow(2, zoom - 1)) ; uy += tileitem_size, tiy++) {
            char *url = _map_url_get_mapquest(zoom, tix, tiy);
            char *file = _strdup_printf("%s/%0.2lf.%d.%d.jpg", path, zoom, tix, tiy);

            ImageData *id = calloc(sizeof(ImageData), 1);
            id->ux = ux;
            id->uy = uy;
            id->cr = cr;
            id->view = v;
            FileDownloader *fd = _file_download_create(v, url, file, 0, _img_downloaded, id);

            if (!fd) {
                ERR("file download create failed: %s -> %s", url, file);
            }
            free(url);
            free(file);
        }
    }

    _file_download_do();
#if 1
    // Load downloaded files
    FileDownloader *fd;
    nemolist_for_each(fd, &file_download_lists, link) {
         Image *img = image_create(fd->filename);
         ImageData *id = fd->data_end;
         LOG("%d %d", id->ux, id->uy);
         cairo_set_source_surface(cr, image_get_surface(img), id->ux, id->uy);
         cairo_paint(cr);
    }
    view_update(v);
#endif

    view_do(v);
    view_destroy(v);
    view_shutdown();

    _file_download_cleanup();
    free(path);

    return 0;
}
