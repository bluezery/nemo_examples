// errno type
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <nemotool.h>
#include <nemocanvas.h>
#include <nemotale.h>
#include <nemotimer.h>
#include "talehelper.h"

#include "text.h"
#include "log.h"

typedef struct _ColorPattern ColorPattern;
struct _ColorPattern
{
    double r;
    double g;
    double b;
};

typedef struct _Textviewer Textviewer;
struct _Textviewer
{
    struct nemocanvas *canvas;
    struct talenode *set_node;
    struct talenode *text_node;
    cairo_t *cr;
    Text **text;
    int line_len;
    double line_space;
    double r, g, b, a;
    struct pathone *setbtn_one;
    struct pathone *group;
};

ColorPattern colors[] = {
    {211/255., 212/255., 223/255.},    // 0 Marble Swiri
    { 66/255., 140/255., 240/255.},    // 1 Aqua Splash
};

char **
_read_file(const char *file, int *line_len)
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

static void
_draw_texts(cairo_t *cr, Text **text, int line_len, double line_space)
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
#if 0
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

static void
_tale_event(struct nemotale *tale, struct talenode *node, uint32_t type, struct taleevent *event)
{
    Textviewer *tv = nemotale_get_userdata(tale);
    struct nemocanvas *canvas = tv->canvas;
    struct taletap *taps[16];
    int ntaps;
    ntaps = nemotale_get_taps(tale, taps, type);
    //ERR("type[%d], ntaps: %d, device[%ld] serial[%d] time[%d] value[%d] x[%lf] y[%lf] dx[%lf] dy[%lf]", type, ntaps, event->device, event->serial, event->time, event->value, event->x, event->y, event->dx, event->dy);
    if (type & NEMOTALE_DOWN_EVENT) {
        if (ntaps == 1) {
            nemocanvas_move(canvas, taps[0]->serial);
        } else if (ntaps == 2) {
            // 1: resize, 2:rotate
            nemocanvas_pick(canvas,
                    taps[0]->serial,
                    taps[1]->serial,
                    (1 << NEMO_SURFACE_PICK_TYPE_ROTATE) |
                    (1 << NEMO_SURFACE_PICK_TYPE_SCALE));
        } else if (ntaps == 3) {
            struct nemotool *tool = nemocanvas_get_tool(canvas);
            nemotool_exit(tool);
        }
    }
}

static void
_canvas_resize(struct nemocanvas *canvas, int32_t width, int32_t height)
{
    struct nemotale *tale  = nemocanvas_get_userdata(canvas);
    Textviewer *tv = nemotale_get_userdata(tale);
    ERR("%d %d", width, height);

    nemocanvas_set_size(canvas, width, height);
    nemotale_node_resize_pixman(tv->set_node, width, height);

    cairo_matrix_t matrix;
	cairo_matrix_init_scale(&matrix,
            (double)width / 640, (double)height / 640);

    nemotale_node_clear_path(tv->set_node);
	nemotale_path_update_one(tv->group);
	nemotale_node_render_path(tv->set_node, tv->group);

	nemotale_node_damage_all(tv->set_node);
	nemotale_composite(tale, NULL);

    //nemocanvas_damage(tv->canvas, 0, 0, 0, 0);
    //nemocanvas_commit(tv->canvas);
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
    nemotale_node_damage_all(tv->text_node);

    ERR("%p");
    struct nemotale *tale = nemocanvas_get_userdata(tv->canvas);
    nemotale_composite(tale, NULL);
    nemocanvas_damage(tv->canvas, 0, 0, 0, 0);
    nemocanvas_commit(tv->canvas);
}

int main(int argc, char *argv[])
{
    Text **text;
    char **line_txt;
    int line_len;
    int w, h;
    int i = 0;
    //w = 640, h = 640;
    w = 120, h = 120;

    Textviewer *tv;
    tv = calloc(sizeof(Textviewer), 1);

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

    tv->text = text;
    tv->line_len = line_len;
    tv->line_space = 0;

    tv->r = colors[0].r;
    tv->g = colors[0].g;
    tv->b = colors[0].b;
    tv->a = 0.9;

    struct nemotool *tool;
    tool = nemotool_create();
    if (!tool) return -1;
    nemotool_connect_wayland(tool, NULL);

    struct nemocanvas *canvas;
    canvas = nemocanvas_create(tool);
    nemocanvas_set_nemosurface(canvas, NEMO_SHELL_SURFACE_TYPE_NORMAL);
    nemocanvas_set_size(canvas, w, h);
    nemocanvas_set_anchor(canvas, -1.0f, 0.0f);
    nemocanvas_set_dispatch_resize(canvas, _canvas_resize);
    nemocanvas_flip(canvas);
    //nemocanvas_clear(canvas);
    tv->canvas = canvas;

    struct nemotale *tale;
    tale = nemotale_create_pixman();
    ERR("%p", tale);
    nemotale_attach_canvas(tale, canvas, _tale_event);
    nemotale_set_userdata(tale, tv);
    nemotale_attach_pixman(tale,
            nemocanvas_get_data(canvas),
            nemocanvas_get_width(canvas),
            nemocanvas_get_height(canvas),
            nemocanvas_get_stride(canvas));
    nemocanvas_set_userdata(canvas, tale);

    struct talenode *node;
    // text layer
    node = nemotale_node_create_pixman(w, h);
    nemotale_node_set_id(node, 1);
    nemotale_attach_node(tale, node);
    tv->text_node = node;

    // setting layer
    node = nemotale_node_create_pixman(w, h);
    nemotale_node_set_id(node, 2);
    nemotale_attach_node(tale, node);
    tv->set_node = node;

    struct pathone *group;
    group = nemotale_path_create_group();

    struct pathone *one;
    double setbtn_radius = 60;
    double setbtn_x = 550;
    double setbtn_y = 550;
    one = nemotale_path_create_circle(setbtn_radius);
    nemotale_path_attach_style(one, NULL);
    //nemotale_path_translate(one, setbtn_x, setbtn_y);
    nemotale_path_set_fill_color(NTPATH_STYLE(one),
            colors[1].r, colors[1].g, colors[1].b, 0.8);
    //nemotale_path_set_stroke_color(NTPATH_STYLE(one), 0.0f, 1.0f, 1.0f, 1.0f);
    //nemotale_path_set_stroke_width(NTPATH_STYLE(one), 1.0f);
    nemotale_path_attach_one(group, one);

    nemotale_path_update_one(group);
    nemotale_node_render_path(node, group);
    tv->setbtn_one = one;
    tv->group = group;

    cairo_surface_t *surf;
    //surf = nemotale_get_cairo(tale);
    surf = nemotale_node_get_cairo(tv->text_node);

    cairo_t *cr;
    cr = cairo_create(surf);
    tv->cr = cr;

    // background
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, tv->r, tv->g, tv->b, tv->a);
    cairo_paint(cr);

    _draw_texts(cr, text, line_len, tv->line_space);

#if 0
    struct nemotimer *timer;
    timer = nemotimer_create(tool);
    nemotimer_set_callback(timer, _timer_callback);
    nemotimer_set_userdata(timer, tv);
    nemotimer_set_timeout(timer, 1000);
#endif

	nemotale_composite(tale, NULL);
    nemocanvas_damage(canvas, 0, 0, 0, 0);
    nemocanvas_commit(canvas);

    nemotool_run(tool);

    nemotale_destroy(tale);
    nemocanvas_destroy(canvas);
    nemotool_disconnect_wayland(tool);
    nemotool_destroy(tool);

    _text_destroy(text[0]);
    free(text);

    _font_shutdown();
    free(tv);

    return 0;
}
