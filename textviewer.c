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

typedef struct _Context Context;
struct _Context
{
    struct nemocanvas *canvas;
    struct talenode *set_node;
    struct talenode *text_node;

    int width, height;
    struct {
        double r, g, b, a;
    } bg;
    Text **text;
    int line_len;
    double line_space;

    cairo_matrix_t matrix;
    struct pathone *group;
    struct pathone *btn_one, *font_family_one, *font_size_one, *font_color_one;
    double btn_radius;

    int timer_cnt;
    double timer_diff_s;
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
_redraw_texts(Context *ctx, cairo_surface_t *surf)
{
    cairo_t *cr = cairo_create(surf);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, ctx->bg.r, ctx->bg.g, ctx->bg.b, ctx->bg.a);
    cairo_paint(cr);
    _draw_texts(cr, ctx->text, ctx->line_len, ctx->line_space);
    cairo_destroy(cr);
}

static struct nemopath *
_path_create(double w, double h)
{
    struct nemopath *path;

    path = nemopath_create();
    nemopath_curve_to(path,
            0, 10,
            0, 0,
            10, 0);
    nemopath_line_to(path,
            w, 0);
    nemopath_curve_to(path,
            w, 0,
            w + 10,  0,
            w + 10, 10);
    nemopath_line_to(path,
            w + 10, h + 10);
    nemopath_curve_to(path,
            w + 10, h + 10,
            w + 10, h + 20,
            w, h + 20);
    nemopath_line_to(path,
            10, h + 20);
    nemopath_curve_to(path,
            10, h + 20,
            0, h + 20,
            0, h + 10);
    nemopath_close_path(path);
    /*
    nemopath_scale(path, scale, scale);
    nemopath_translate(path, x, y);
    */

    return path;
}

static void
_font_menu_animator(struct nemotimer *timer, void *data)
{
    Context *ctx = data;
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    struct nemotale *tale  = nemocanvas_get_userdata(canvas);
    int w = ctx->width;
    int h = ctx->height;

    ctx->timer_cnt++;
    nemotale_handle_canvas_update_event(NULL, canvas, tale);

    struct nemopath *path;
    double scale = 0.1 + ctx->timer_diff_s * ctx->timer_cnt;

    nemotale_path_scale(ctx->font_family_one, scale, scale);
    nemotale_path_scale(ctx->font_size_one, scale, scale);
    nemotale_path_scale(ctx->font_color_one, scale, scale);

    nemotale_node_clear_path(ctx->set_node);
	nemotale_node_damage_all(ctx->set_node);
	nemotale_path_update_one(ctx->group);
	nemotale_node_render_path(ctx->set_node, ctx->group);

	nemotale_composite(tale, NULL);
    nemotale_handle_canvas_flush_event(NULL, canvas, NULL);

    if (ctx->timer_cnt < 60)  {
        nemotimer_set_timeout(timer, 1000.0/60);
    } else
        ctx->timer_cnt = 0;
}

static void
_transform_event(struct taletransition *trans, void *context, void *data)
{
	struct pathone *one = (struct pathone *)data;

	nemotale_path_transform_dirty(one);
}
static void
_font_menu_animation(Context *ctx)
{
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    struct nemotale *tale  = nemocanvas_get_userdata(canvas);
    struct taletransition *trans;

#if 1
    trans = nemotale_transition_create(0, 1200);
    nemotale_transition_attach_timing(trans, 1.0f, NEMOEASE_CUBIC_OUT_TYPE);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_handle_canvas_update_event, canvas, tale);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_node_handle_path_damage_event, ctx->set_node, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_damage_event, ctx->set_node, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            _transform_event, NULL, ctx->font_family_one);
    /*
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event, NULL, ctx->font_family_one);
            */
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_update_event, NULL, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_render_event, ctx->set_node, ctx->group);

    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_composite_event, tale, NULL);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_canvas_flush_event, canvas, tale);

    nemotale_transition_attach_signal(trans, NTPATH_DESTROY_SIGNAL(ctx->group));
    nemotale_transition_attach_signal(trans, NTPATH_DESTROY_SIGNAL(ctx->font_family_one));

    /*
    nemotale_transition_attach_dattr(trans, NTPATH_ATWIDTH(ctx->font_family_one),
            1.0f, 1000);
    */
    nemotale_dispatch_transition_timer_event(tool, trans);
#else

    ctx->timer_diff_s = 0.9/60.0;

    struct nemotimer *timer = nemotimer_create(tool);
    nemotimer_set_timeout(timer, 1000.0/60);
    nemotimer_set_userdata(timer, ctx);
    nemotimer_set_callback(timer, _font_menu_animator);
    ctx->timer_cnt = 0;
#endif
}

static void
nemotemp_handle_canvas_end_event(struct taletransition *trans, void *_context, void *_data)
{
    Context *ctx = _context;
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotale *tale = nemocanvas_get_userdata(canvas);

    nemotale_handle_canvas_update_event(NULL, canvas, tale);

    double w = nemotale_get_width(tale);
    double h = nemotale_get_height(tale);
    struct pathone *one;
    struct nemopath *path;
    double scale = 0.1;

    path = _path_create(300, 100);
    one = nemotale_path_create_path(NULL);
    nemotale_path_set_id(one, "font_family");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one),
            colors[1].r, colors[1].g, colors[1].b, 0.8);
    nemotale_path_set_stroke_color(NTPATH_STYLE(one), 1.0f, 1.0f, 1.0f, 1.0f);
    nemotale_path_set_stroke_width(NTPATH_STYLE(one), 0.1f);
    nemotale_path_use_path(one, path);
    nemotale_path_set_pivot_type(one, NEMOTALE_PATH_PIVOT_RATIO);
    nemotale_path_set_pivot(one, 0.5, 1);
    nemotale_path_scale(one, scale, scale);
    nemotale_path_translate(one, w/2 - 150, h/2 -100);
    nemotale_path_attach_one(ctx->group, one);
    ctx->font_family_one = one;

    path = _path_create(140, 100);

    one = nemotale_path_create_path(NULL);
    nemotale_path_set_id(one, "font_size");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one),
            colors[1].r, colors[1].g, colors[1].b, 0.8);
    nemotale_path_set_stroke_color(NTPATH_STYLE(one), 1.0f, 1.0f, 1.0f, 1.0f);
    nemotale_path_set_stroke_width(NTPATH_STYLE(one), 0.1f);
    nemotale_path_use_path(one, path);
    nemotale_path_set_pivot_type(one, NEMOTALE_PATH_PIVOT_RATIO);
    nemotale_path_set_pivot(one, 1, 0);
    nemotale_path_scale(one, scale, scale);
    nemotale_path_translate(one, w/2 - 150, h/2 + 30);
    nemotale_path_attach_one(ctx->group, one);
    ctx->font_size_one = one;

    path = _path_create(140, 100);
    one = nemotale_path_create_path(NULL);
    nemotale_path_set_id(one, "font_color");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_set_fill_color(NTPATH_STYLE(one),
            colors[1].r, colors[1].g, colors[1].b, 0.8);
    nemotale_path_set_stroke_color(NTPATH_STYLE(one), 1.0f, 1.0f, 1.0f, 1.0f);
    nemotale_path_set_stroke_width(NTPATH_STYLE(one), 0.1f);
    nemotale_path_use_path(one, path);
    nemotale_path_set_pivot_type(one, NEMOTALE_PATH_PIVOT_RATIO);
    nemotale_path_set_pivot(one, 0, 0);
    nemotale_path_scale(one, scale, scale);
    nemotale_path_translate(one, w/2 + 10, h/2 + 30);
    nemotale_path_attach_one(ctx->group, one);
    ctx->font_color_one = one;

    nemotale_path_destroy_one(ctx->btn_one);
    nemotale_node_clear_path(ctx->set_node);
	nemotale_node_damage_all(ctx->set_node);
    nemotale_path_update_one(ctx->group);
    nemotale_node_render_path(ctx->set_node, ctx->group);

	nemotale_composite(tale, NULL);
    nemotale_handle_canvas_flush_event(NULL, canvas, NULL);

    _font_menu_animation(ctx);
}

static void
_animation(Context *ctx)
{
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    struct nemotale *tale  = nemocanvas_get_userdata(canvas);
    struct taletransition *trans;
    trans = nemotale_transition_create(0, 1200);
    nemotale_transition_attach_timing(trans, 1.0f, NEMOEASE_CUBIC_OUT_TYPE);

    /*
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_START,
            nemotemp_handle_canvas_start_event, ctx, NULL);*/
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_handle_canvas_update_event, canvas, tale);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_PREUPDATE,
            nemotale_node_handle_path_damage_event, ctx->set_node, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_damage_event, ctx->set_node, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_transform_event, NULL, ctx->btn_one);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_path_update_event, NULL, ctx->group);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_node_handle_path_render_event, ctx->set_node, ctx->group);

    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_composite_event, tale, NULL);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            nemotale_handle_canvas_flush_event, canvas, tale);
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_END,
            nemotemp_handle_canvas_end_event, ctx, NULL);

    nemotale_transition_attach_signal(trans, NTPATH_DESTROY_SIGNAL(ctx->group));

    nemotale_transition_attach_dattr(trans, NTPATH_TRANSFORM_ATX(ctx->btn_one),
            1.0f, ctx->width/2 - ctx->btn_radius);
    nemotale_transition_attach_dattr(trans, NTPATH_TRANSFORM_ATY(ctx->btn_one),
            1.0f, ctx->height/2 - ctx->btn_radius);
    nemotale_dispatch_transition_timer_event(tool, trans);
}

static void
_tale_event(struct nemotale *tale, struct talenode *node, uint32_t type, struct taleevent *event)
{
    Context *ctx = nemotale_get_userdata(tale);
    struct nemocanvas *canvas = ctx->canvas;
    struct taletap *taps[16];
    int ntaps;
    ntaps = nemotale_get_taps(tale, taps, type);
    //ERR("type[%d], ntaps: %d, device[%ld] serial[%d] time[%d] value[%d] x[%lf] y[%lf] dx[%lf] dy[%lf]", type, ntaps, event->device, event->serial, event->time, event->value, event->x, event->y, event->dx, event->dy);
    if (type & NEMOTALE_DOWN_EVENT) {
        struct taletap *tap = nemotale_get_tap(tale, event->device, type);
        tap->item = nemotale_path_pick_one(ctx->group, event->x, event->y);

        if (ntaps == 1) {
            nemocanvas_move(canvas, taps[0]->serial);
        } else if (ntaps == 2) {
            // 1: resize, 2:rotate
            nemocanvas_pick(canvas,
                    taps[0]->serial,
                    taps[1]->serial,
                    (1 << NEMO_SURFACE_PICK_TYPE_ROTATE) |
                    (1 << NEMO_SURFACE_PICK_TYPE_SCALE));
        }
    } else if (nemotale_is_single_click(tale, event, type)) {
        if (ntaps == 1) {
            struct taletap *tap = nemotale_get_tap(tale, event->device, type);
            struct pathone *one = tap->item;
            if (one && !strcmp(NTPATH_ID(one), "btn")) {
                _animation(ctx);
            }

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
    Context *ctx = nemotale_get_userdata(tale);

    nemocanvas_set_size(canvas, width, height);
    nemotale_handle_canvas_update_event(NULL, canvas, tale);

    cairo_matrix_t matrix;
    // set btn
    nemotale_node_resize_pixman(ctx->set_node, width, height);
	cairo_matrix_init_scale(&matrix,
            (double)width / ctx->width, (double)height / ctx->height);

    nemotale_node_clear_path(ctx->set_node);
	nemotale_node_damage_all(ctx->set_node);
    nemotale_path_set_parent_transform(ctx->group, &matrix);
	nemotale_path_update_one(ctx->group);
	nemotale_node_render_path(ctx->set_node, ctx->group);

    // Text layer
    nemotale_node_resize_pixman(ctx->text_node, width, height);
    cairo_surface_t *surf;
    surf = nemotale_node_get_cairo(ctx->text_node);
    _redraw_texts(ctx, surf);

	nemotale_composite(tale, NULL);
    nemotale_handle_canvas_flush_event(NULL, canvas, NULL);
}

int main(int argc, char *argv[])
{
    Text **text;
    char **line_txt;
    int line_len;
    int w, h;
    int i = 0;
    w = 640, h = 640;

    Context *ctx;
    ctx = calloc(sizeof(Context), 1);
    ctx->width = w;
    ctx->height = h;

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

    ctx->text = text;
    ctx->line_len = line_len;
    ctx->line_space = 0;

    ctx->bg.r = colors[0].r;
    ctx->bg.g = colors[0].g;
    ctx->bg.b = colors[0].b;
    ctx->bg.a = 0.9;

    struct nemotool *tool;
    tool = nemotool_create();
    nemotool_connect_wayland(tool, NULL);

    struct nemocanvas *canvas;
    canvas = nemocanvas_create(tool);
    nemocanvas_set_size(canvas, w, h);
    nemocanvas_set_nemosurface(canvas, NEMO_SHELL_SURFACE_TYPE_NORMAL);
    nemocanvas_set_anchor(canvas, -0.5f, 0.5f);
    nemocanvas_set_dispatch_resize(canvas, _canvas_resize);
    ctx->canvas = canvas;

    struct nemotale *tale;
    tale = nemotale_create_pixman();
    nemotale_attach_canvas(tale, canvas, _tale_event);
    nemotale_set_userdata(tale, ctx);

	nemocanvas_flip(canvas);
	nemocanvas_clear(canvas);
	nemotale_attach_pixman(tale,
			nemocanvas_get_data(canvas),
			nemocanvas_get_width(canvas),
			nemocanvas_get_height(canvas),
			nemocanvas_get_stride(canvas));

    struct talenode *node;
    // text layer
    node = nemotale_node_create_pixman(w, h);
    nemotale_node_set_id(node, 1);
    nemotale_attach_node(tale, node);
    ctx->text_node = node;

    // setting layer
    node = nemotale_node_create_pixman(w, h);
    nemotale_node_set_id(node, 2);
    nemotale_attach_node(tale, node);
    ctx->set_node = node;

    struct pathone *group;
    group = nemotale_path_create_group();
    ctx->group = group;

    struct pathone *one;
    double btn_radius = 50;
    double setbtn_x = w - btn_radius * 2 - 10;
    double setbtn_y = h - btn_radius * 2- 10;
    ctx->btn_radius = btn_radius;

    one = nemotale_path_create_circle(btn_radius);
    nemotale_path_set_id(one, "btn");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_translate(one, setbtn_x, setbtn_y);
    nemotale_path_set_fill_color(NTPATH_STYLE(one),
            colors[1].r, colors[1].g, colors[1].b, 0.8);
    nemotale_path_set_stroke_color(NTPATH_STYLE(one), 1.0f, 1.0f, 1.0f, 1.0f);
    nemotale_path_set_stroke_width(NTPATH_STYLE(one), 1.0f);
    nemotale_path_attach_one(group, one);
    ctx->btn_one = one;

    nemotale_path_update_one(group);
    nemotale_node_render_path(node, group);

    cairo_surface_t *surf;
    surf = nemotale_node_get_cairo(ctx->text_node);

    _redraw_texts(ctx, surf);

	nemotale_composite(tale, NULL);
    nemotale_handle_canvas_flush_event(NULL, canvas, NULL);

    nemotool_run(tool);

    nemotale_destroy(tale);
    nemocanvas_destroy(canvas);
    nemotool_disconnect_wayland(tool);
    nemotool_destroy(tool);

    _text_destroy(text[0]);
    free(text);

    _font_shutdown();
    free(ctx);

    return 0;
}
