#include <stdio.h> // snprintf
#include <string.h> // stdup
#include <math.h>   // PI
#include <unistd.h> // write, usleep

#include <curl/curl.h>

#include "cairo_view.h"
#include "log.h"

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
_map_url_get_mapquest(int x, int y, int zoom)
{
   char buf[PATH_MAX];
   snprintf(buf, sizeof(buf),
            "http://otile%d.mqcdn.com/tiles/1.0.0/osm/%d/%d/%d.png",
            ((x + y + zoom) % 4) + 1, zoom, x, y);
   return strdup(buf);
}

CURLM *curlm;

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

int main()
{
    cairo_surface_t *surf;
    cairo_t *cr;
    cairo_user_data_key_t key;
    double w, h;

    char *url = _map_url_get_mapquest(0, 0, 2);
    LOG("%s", url);
    const char *file = "./test.jpg";

    FILE *fp = NULL;
    fp = fopen(file, "w+");
    if (!fp) {
        ERR("%s", strerror(errno));
        curl_multi_cleanup(curlm);
        curl_global_cleanup();
        return -1;
    }

    CURLMcode retm;
    CURLcode ret;
    if (curl_global_init(CURL_GLOBAL_ALL)) {
        ERR("curl global init failed");
        return -1;
    }
    curlm = curl_multi_init();

    CURL *curl = curl_easy_init();
    // ret = curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); // debugging
    ret = curl_easy_setopt(curl, CURLOPT_URL, url);
    ret = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _curl_write_func);
    ret = curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    //ret = curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, _curl_progress_func);
    ret = curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, NULL);
    //ret = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, _curl_header_func);
    ret = curl_easy_setopt(curl, CURLOPT_HEADERDATA, NULL);
    ret = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3);
    ret = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

    retm = curl_multi_add_handle(curlm, curl);
    if (retm != CURLM_OK) {
        ERR("curl multi add handle failed: %s", curl_multi_strerror(retm))
    }

    LOG("%s", url);
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

        // loop
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

    retm = curl_multi_remove_handle(curlm, curl);
    //ret = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    fclose(fp);
    w = 600, h = 600;
    surf = _cairo_surface_create(0, w, h, &key);
    RET_IF(!surf, -1);
    cr = _cairo_create(surf);
    RET_IF(!surf, -1);

    cairo_rectangle(cr, 10, 10, 100, 100);
    cairo_stroke(cr);

    pixman_image_t *pimg = pixman_load_jpeg_file(file);
    RET_IF(!pimg, -1);

    int pw, ph;
    pw = pixman_image_get_width(pimg);
    ph = pixman_image_get_height(pimg);
    unsigned char *data = (unsigned char *)pixman_image_get_data(pimg);
    LOG("pixman: %d %d", pw, ph);

    cairo_format_t format = CAIRO_FORMAT_ARGB32;
    int stride = cairo_format_stride_for_width (format, pw);
    LOG("%d", stride);
    cairo_surface_t *img = cairo_image_surface_create_for_data(data, format, pw, ph, stride);

    double ww, hh;
    ww = cairo_image_surface_get_width(img);
    hh = cairo_image_surface_get_height(img);
    LOG("cairo: %lf %lf", ww, hh);
    cairo_set_source_surface(cr, img, 0, 0);
    cairo_paint(cr);

    cairo_surface_destroy(img);

    _Cairo_Render func = cairo_surface_get_user_data (surf, &key);
    RET_IF(!func, -1);
    if (func) func(cr, w, h);

    curl_multi_cleanup(curlm);
    curl_global_cleanup();

    cairo_surface_destroy(surf);
    cairo_destroy(cr);
    return 0;
}
