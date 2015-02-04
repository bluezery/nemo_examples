#include "cairo_view.h"
#include <stdio.h> // snprintf
#include <string.h> // strdup
#include <math.h>   // PI
#include <unistd.h> // write, usleep

#include <curl/curl.h>
#include <stdbool.h>

#include "log.h"
#include "util.h"
#include "nemolist.h"
#include "pixmanhelper.h"

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


CURLM *curlm;

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
    return true;
}

static void
_file_download_cleanup()
{
    if (curlm) curl_multi_cleanup(curlm);
    curl_global_cleanup();
}

typedef struct _FileDownloader {
    struct nemolist link;
    CURL *curl;
    char *url;
    char *filename;
    int ux, uy;

} FileDownloader;

static FileDownloader *
_file_download_create(const char *src, const char *dst, unsigned int timeout)
{
    RET_IF(!src, false);
    RET_IF(!dst, false);

    FILE *fp;
    CURLMcode retm;
    CURLcode ret;
    CURL *easy;

    if (_file_exist(dst)) {
        LOG("file already downloaded: %s", dst);
        FileDownloader *fd = malloc(sizeof(FileDownloader));
        fd->url = strdup(src);
        fd->filename = strdup(dst);
        return fd;
    }
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
    ret = curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, _curl_write_func);
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

    retm = curl_multi_add_handle(curlm, easy);
    if (retm) {
        ERR("curl multi add handle failed: %s", curl_multi_strerror(retm));
        curl_easy_cleanup(easy);
        return NULL;
    }
    FileDownloader *fd = malloc(sizeof(FileDownloader));
    fd->curl = easy;
    fd->url = strdup(src);
    fd->filename = strdup(dst);
    return fd;
}

static void
_file_download_do()
{
    CURLMcode retm;
    int still_running;
    while (1) {
        struct timeval twait;
        long tout;
        retm = curl_multi_timeout(curlm, &tout);
        if (retm == CURLM_OK) {
            if (tout >= 0) {
                twait.tv_sec = tout / 1000;
                if (twait.tv_sec > 1) twait.tv_sec =1;
                else {
                    twait.tv_usec = (tout % 1000) * 1000;
                }
            }
        } else {
            twait.tv_usec = 100000; // minimum: 100 milliseconds.
        }




        int max_fd;
        fd_set fd_read;
        fd_set fd_write;
        fd_set fd_err;

        FD_ZERO(&fd_read);
        FD_ZERO(&fd_write);
        FD_ZERO(&fd_err);

        int r;

        retm = curl_multi_fdset(curlm, &fd_read, &fd_write, &fd_err, &max_fd);

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

        retm = curl_multi_perform(curlm, &still_running);
        if (retm == CURLM_CALL_MULTI_PERFORM) {
            LOG("perform again");
            continue;
        } else if (retm != CURLM_OK) {
            ERR("curl multi perform failed: %s", curl_multi_strerror(retm));
            break;
        }
        if (still_running) {
            LOG("still_running");
        } else {
            LOG("perform end");
            break;
        }
    }

    //retm = curl_multi_remove_handle(curlm, curl);
    //curl_easy_cleanup(curl);
    //fclose(fp);
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
    pixman_image_t *pimg;
} TileItem;

static int tileitem_size = 255;

typedef struct _Image
{
   char *path;
   pixman_image_t *pimg;
   cairo_surface_t *surface;
   unsigned int width;
   unsigned int height;
   cairo_format_t format;
   unsigned int stride;
} Image;

static Image *
_image_create(const char *path)
{
   RET_IF(!path, NULL);
   int pw, ph;

   // FIXME: file type recognization
   pixman_image_t *pimg = pixman_load_jpeg_file(path);

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

static void
_image_destroy(Image *img)
{
   RET_IF(!img);
   free(img->path);
   cairo_surface_destroy(img->surface);
   pixman_image_unref(img->pimg);
   free(img);
}


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

int main()
{
    cairo_surface_t *surf;
    cairo_t *cr;
    cairo_user_data_key_t key;

    int w = 600, h = 600;

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
    double lon = 126.9761088, lat = 37.2929565;
    int tx, ty;

    // Get left,top position in the tile coordinate
    _map_region_to_tilecoord(zoom, lon, lat, &tx, &ty);
    //_map_tilecoord_to_region(5, x, y, &lon, &lat);

    //LOG("%lf: %lf %lf <-> %u %u", zoom, lon, lat, x, y);

    surf = _cairo_surface_create(0, w, h, &key);
    RET_IF(!surf, -1);
    cr = _cairo_create(surf, 255, 255, 255, 255);
    RET_IF(!surf, -1);
    LOG("%s", path);
    // Tile Index
    int tix, tiy;
    tix = tx/tileitem_size;

    // left, top position in the user coordinate (inside window)
    int ux, uy;
    ux = (w - tileitem_size)/2;
    LOG("tix:%d ux:%d, tx:%d", tix, ux, tx);

    while (tix >= 0) {
        tix--;
        if (tix < 0) {
            tix = 0;
            break;
        }
        ux -= tileitem_size;
        if (ux < 0) break;
    }

    // Create download list
    struct nemolist downloads;
    nemolist_init(&downloads);
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
            LOG("%s", url);
            char *file = _strdup_printf("%s/%0.2lf.%d.%d.jpg", path, zoom, tix, tiy);

            FileDownloader *fd = _file_download_create(url, file, 0);
            free(file);
            free(url);
            fd->ux = ux;
            fd->uy = uy;
            nemolist_insert(&downloads, &fd->link);
        }
    }

    //  Download do
    _file_download_do();

    // Load downloaded files
    FileDownloader *fd;
    nemolist_for_each(fd, &downloads, link) {
         Image *img = _image_create(fd->filename);
         cairo_set_source_surface(cr, img->surface, fd->ux, fd->uy);
         cairo_paint(cr);
    }

    _Cairo_Render func = cairo_surface_get_user_data (surf, &key);
    RET_IF(!func, -1);
    if (func) func(cr, w, h);

    cairo_surface_destroy(surf);
    cairo_destroy(cr);

    _file_download_cleanup();
    free(path);

    return 0;
}
