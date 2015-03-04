// errno type
#include <nemotale.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <fontconfig/fontconfig.h>

#include <talegl.h>
#include <nemotool.h>
#include <nemocanvas.h>
#include <nemoegl.h>
#include <nemotimer.h>
#include <pixmanhelper.h>
#include <mischelper.h>

#include <pathshape.h>
#include <pathstyle.h>
#include <talemisc.h>

#include "talehelper.h"
#include "view.h"
#include "text.h"
#include "log.h"

/*
static void
calc_surface_size(cairo_scaled_font_t *scaled_font,
        int line_len, cairo_glyph_line *l, double line_space,
        double margin_left, double margin_right, double margin_top, double margin_bottom,
        unsigned int vertical,
        double *width, double *height)
{
    double w, h;

    cairo_font_extents_t font_extents;
    cairo_scaled_font_extents(scaled_font, &font_extents);

    if (vertical) {
        w  = line_len * (font_extents.height + 0) - line_space;
        h  = 0;
    } else {
        h = line_len * (font_extents.height + 0) - line_space;
        w = 0;
    }
    for (int i = 0 ; i < line_len ; i++) {
        double x_advance, y_advance;
        x_advance = l->glyphs[l->num_glyphs].x;
        y_advance = l->glyphs[l->num_glyphs].y;
        if (vertical)
            h = MAX(h, y_advance);
        else
            w = MAX(w, x_advance);
        l++;
    }
    w += margin_left + margin_right;
    h += margin_top + margin_bottom;
    *width = w;
    *height = h;
    //LOG("w:%lf h:%lf", w, h);
}
*/

char **_read_file(const char *file, int *line_len)
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

static void _dispatch_event(struct nemotale *tale, struct talenode *node, uint32_t type, struct taleevent *event)
{
    struct taletap *taps[16];
    int ntaps;
    ntaps = nemotale_get_taps(tale, taps, type);
    printf("type[%d], ntaps: %d, device[%ld] serial[%d] time[%d] value[%d] x[%lf] y[%lf] dx[%lf] dy[%lf]\n", type, ntaps, event->device, event->serial, event->time, event->value, event->x, event->y, event->dx, event->dy);
}

typedef struct Textviewer
{
    struct nemocanvas *canvas;
    cairo_t *cr;
    Text **text;
    int line_len;
    double line_space;
    double r, g, b, a;
} Textviewer;

static void _draw_texts(cairo_t *cr, Text **text, int line_len, double line_space)
{
    int i = 0;
    // Draw multiple texts
    double margin_left = 0, margin_top = 0;
    //double margin_right = 0, margin_bottom = 0;
    cairo_save(cr);
    cairo_translate(cr, margin_left, margin_top);
    for (i = 0 ; i < line_len ; i++) {
        if (!text[i]) continue;
        bool vertical;
        _text_get_direction(text[i], &vertical, NULL);
        if (vertical) {
            if (i) cairo_translate (cr, line_space, 0);
        }
        else {
            if (i) cairo_translate (cr, 0, line_space);
        }
        // draw cairo
        _text_draw(text[i], cr);

        double tw, th;
        tw = _text_get_width(text[i]);
        th = _text_get_height(text[i]);
#if 1
        // Draw text bounding box
        cairo_save(cr);
        //_text_get_size(text[i], &tw, &th);
        cairo_rectangle(cr, 0, 0, tw, th);
        //LOG("%lf %lf", tw, th);
        cairo_move_to(cr, 100, 30);
        cairo_set_line_width(cr, 1);
        cairo_set_source_rgba(cr, 1, 0, 0, 1);
        cairo_stroke(cr);
        cairo_restore(cr);
#endif
        if (vertical) cairo_translate (cr, tw, 0);
        else cairo_translate (cr, 0, th);
    }
    cairo_restore(cr);
}

static void _timer_callback(struct nemotimer *timer, void *data)
{
    Textviewer *tv = data;
    _text_set_font_size(tv->text[0], 50);

    cairo_save (tv->cr);
    cairo_set_source_rgba (tv->cr, tv->r, tv->g, tv->b, tv->a);
    cairo_set_operator (tv->cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (tv->cr);
    cairo_restore (tv->cr);

    _draw_texts(tv->cr, tv->text, tv->line_len, tv->line_space);
    nemocanvas_damage(tv->canvas, 0, 0, 0, 0);
    nemocanvas_commit(tv->canvas);
}

int main(int argc, char *argv[])
{
    Text **text;
    char **line_txt;
    int line_len;
    int w = 640, h = 640;
    double line_space = 0;
    int i = 0;

    if (argc != 2 || !argv[1]) {
        ERR("Usage: show [file name]");
        return 0;
    }

    if (!_font_init()) {
        ERR("_font_init failed");
        return -1;
    }

    // Read a file
    line_txt = _read_file(argv[1], &line_len);
    if (!line_txt || !line_txt[0] || line_len <= 0) {
        ERR("Err: line_txt is NULL or no string or length is 0");
        return -1;
    }

    text = (Text **)malloc(sizeof(Text *) * line_len);
    for (i = 0 ; i < line_len ; i++) {
        text[i] = _text_create(line_txt[i]);
        if (!text[i]) continue;
        _text_set_font_family(text[i], "LiberationMono");
        _text_set_hint_width(text[i], w);
        _text_set_wrap(text[i], 2);
        _text_set_font_size(text[i], 30);
        free(line_txt[i]);
    }
    free(line_txt);

    struct nemotool *tool;
    struct nemocanvas *canvas;

    tool = nemotool_create();
    if (!tool) return -1;

    nemotool_connect_wayland(tool, NULL);
    canvas = nemocanvas_create(tool);

    nemocanvas_set_nemosurface(canvas, NEMO_SHELL_SURFACE_TYPE_NORMAL);
    nemocanvas_set_anchor(canvas, -1.0f, 0.0f);
    nemocanvas_set_size(canvas, w, h);
    //nemocanvas_set_dispatch_resize(canvas, ..);
    nemocanvas_flip(canvas);
    //nemocanvas_clear(canvas);

    struct nemotale *tale;
    tale = nemotale_create_pixman();
    nemotale_attach_canvas(tale, canvas, _dispatch_event);
    nemotale_attach_pixman(tale,
            nemocanvas_get_data(canvas),
            nemocanvas_get_width(canvas),
            nemocanvas_get_height(canvas),
            nemocanvas_get_stride(canvas));

#if 0
    struct talemode *node;
    node = nemotale_node_create_pixman(w, h);
    nemotale_node_set_id(node, 1);
    nemotale_attach_node(tale, node);

    nemotale_node_get_cairo

    struct pathone *one = nemotale_path_create_rect(w, h);
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one), 0.0f, 0.5f, 0.5f, 0.5f);
    nemotale_path_set_stroke_color(NTPATH_STYLE(one), 0.0f, 1.0f, 1.0f, 1.0f);
    nemotale_path_set_stroke_width(NTPATH_STYLE(one), 1.0f);

    nemotale_path_update_one(one);
    nemotale_node_render_path(node, one);
#endif

    cairo_surface_t *surf;
    surf = nemotale_get_cairo(tale);

    struct Textviewer *tv;
    tv = calloc(sizeof(Textviewer), 1);
    tv->canvas = canvas;
    tv->text = text;
    tv->line_len = line_len;
    tv->line_space = line_space;
    tv->r = 1;
    tv->g = 1;
    tv->b = 1;
    tv->a = 0.5;

    cairo_t *cr;
    cr = cairo_create(surf);
    tv->cr = cr;

    // background
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, tv->r, tv->g, tv->b, tv->a);
    cairo_paint(cr);

    _draw_texts(cr, text, line_len, line_space);

    struct nemotimer *timer;
    timer = nemotimer_create(tool);
    nemotimer_set_callback(timer, _timer_callback);
    nemotimer_set_userdata(timer, tv);
    nemotimer_set_timeout(timer, 1000);

	//nemotale_composite(tale, NULL);
    nemocanvas_damage(canvas, 0, 0, 0, 0);
    nemocanvas_commit(canvas);

    nemotool_run(tool);

    nemotale_destroy(tale);

    nemotool_disconnect_wayland(tool);
    nemotool_destroy(tool);

    _text_destroy(text[0]);
    free(text);

    _font_shutdown();

    return 0;
}
