#include <float.h>
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
#include "util.h"

typedef struct _Color Color;
struct _Color
{
    double r;
    double g;
    double b;
    double a;
};

// http://www.materialpalette.com/
Color mcolors[] = {
    {0, 0, 0, 1},        // black    // 0
    {1, 1, 1, 1},        // white
    {243/255., 66/255., 53/255.,1},     // RED
    {232/255., 29/255., 98/255.,1},     // PINK
    {155/255., 38/255., 175/255.,1},    // PURPLE
    {102/255., 57/255., 182/255.,1},    // DEEP PURPLE //5
    {62/255., 80/255., 180/255.,1},     // INDIGO
    {32/255., 149/255., 242/255.,1},    // BLUE
    {1/255., 168/255., 243/255.,1},     // LIGHT BLUE
    {0, 187/255., 211/255.,1},          // CYAN
    {0, 149/255., 135/255.,1},          // TEAL    // 10
    {75/255., 174/255., 79/255.,1},     // GREEN
    {138/255., 194/255., 73/255.,1},    // LIGHT GREEN
    {204/255., 219/255., 56/255.,1},    // LIME
    {254/255., 234/255., 58/255.,1},    // YELLOW
    {254/255., 192/255., 6/255.,1},     // AMBER   // 15
    {254/255., 151/255., 0/255.,1},     // ORANGE
    {254/255., 86/255., 33/255.,1},     // DEEP ORANGE
    {120/255., 84/255., 71/255.,1},     // BROWN
    {157/255., 157/255., 157/255.,1},   // GREY
    {95/255., 124/255., 138/255.,1},    // BLUE GREY   // 20
    {32/255., 32/255., 32/255.,1},      // Primary Text
    {113/255., 113/255., 113/255.,1},   // Secondary Text
    {181/255., 181/255., 181/255.,1},   // Divider

    // FIXME: more colors
    {211/255., 212/255., 223/255.,1},    // 24 Marble Swiri
    { 66/255., 140/255., 240/255.,1},    // 25 Aqua Splash
};

typedef struct _Theme Theme;
struct _Theme {
    Color color1;
    Color color2;
    Color color3;
    Color color4;
    Color color5;
};

// http://www.dtelepathy.com/blog/inspiration/beautiful-color-palettes-for-your-next-web-project
Theme themes[] = {
    {{105/255.,210/255.,213/255.,1},{167/255.,219/255.,219/255.,1},
     {224/255.,228/255.,204/255.,1},{243/255.,134/255.,48/255.,1},
     {250/255.,105/255.,0/255.,1}},   // Giant Goldfish
    {{233/255.,76/255.,111/255.,1},{84/255.,39/255.,51/255.,1},
     {90/255.,106/255.,98/255.,1},{198/255.,213/255.,205/255.,1},
     {253/255.,242/255.,0/255.,1}},    // Cardsox
    {{219/255.,51/255.,64/255.,1},{232/255.,183/255.,26/255.,1},
     {247/255.,234/255.,200/255.,1},{31/255.,218/255.,154/255.,1},
     {40/255.,171/255.,227/255.,1}}, // Campfire
    {{208/255.,201/255.,31/255.,1},{133/255.,196/255.,185/255.,1},
     {0/255.,139/255.,186/255.,1},{223/255.,81/255.,76/255.,1},
     {220/255.,64/255.,59/255.,1}},   // Aladin
    {{0/255.,200/255.,248/255.,1},{89/255.,196/255.,197/255.,1},
     {255/255.,195/255.,60/255.,1},{251/255.,226/255.,180/255.,1},
     {255/255.,76/255.,101/255.,1}}, // Chrome Sports
    {{94/255.,65/255.,47/255.,1},{252/255.,235/255.,182/255.,1},
     {120/255.,192/255.,168/255.,1},{240/255.,120/255.,24/255.,1},
     {240/255.,168/255.,48/255.,1}}, // Popular New Guinea
    {{222/255.,77/255.,78/255.,1},{218/255.,70/255.,36/255.,1},
     {222/255.,89/255.,58/255.,1},{255/255.,208/255.,65/255.,1},
     {110/255.,158/255.,207/255.,1}},   // Barni Design
    {{177/255.,235/255.,0/255.,1},{83/255.,187/255.,244/255.,1},
     {255/255.,133/255.,203/255.,1},{255/255.,67/255.,46/255.,1},
     {255/255.,172/255.,0/255.,1}},   // Instapuzzle
    // TO be continued......
};

/**********************************/
/****** Text Area *****************/
/**********************************/
typedef struct _TextArea TextArea;
struct _TextArea
{
    Text **texts;
    int len;
    struct {
        double x;
        double y;
        double w;
        double h;
    } content;

    int line_space;
    struct {
        int left;
        int top;
        int right;
        int bottom;
    } margin;
    struct {
        char *family;
        char *style;
        int size;
        Color color;
    } font;

    cairo_surface_t *surf;

    // Common
    Color bg_color;
    double bg_alpha;
    bool dirty;

    int w, h;
};

TextArea *
_textarea_create(Text **texts, int texts_len)
{
    RET_IF(!texts, NULL);
    RET_IF(texts_len <= 0, NULL);

    TextArea *ta;
    ta = calloc(sizeof(TextArea), 1);
    ta->texts = texts;
    ta->len = texts_len;
    return ta;
}

TextArea *
_textarea_create_from_file(const char *filename)
{
    RET_IF(!filename, NULL);

    char **line_txt;
    Text **texts;
    int line_len;
    int i = 0;

    // Read a file
    line_txt = _file_load(filename, &line_len);
    if (!line_txt || !line_txt[0] || line_len <= 0) {
        ERR("Err: line_txt is NULL or no string or length is 0");
        return NULL;
    }

    texts = malloc(sizeof(Text *) * line_len);
    for (i = 0 ; i < line_len ; i++) {
        texts[i] = _text_create(line_txt[i]);
        free(line_txt[i]);
    }
    free(line_txt);

    return _textarea_create(texts, line_len);
}

void
_textarea_font_family_set(TextArea *ta, const char *family)
{
    RET_IF(!ta);
    RET_IF(!family);
    ta->font.family = strdup(family);
}

const char *
_textarea_font_family_get(TextArea *ta)
{
    RET_IF(!ta, NULL);
    return ta->font.family;
}

void
_textarea_font_style_set(TextArea *ta, const char *style)
{
    RET_IF(!ta);
    RET_IF(!style);
    ta->font.style = strdup(style);
}

const char *
_textarea_font_style_get(TextArea *ta)
{
    RET_IF(!ta, NULL);
    return ta->font.style;
}

void
_textarea_font_size_set(TextArea *ta, int font_size)
{
    RET_IF(!ta);
    RET_IF(font_size <= 0);
    ta->font.size = font_size;
}

int
_textarea_font_size_get(TextArea *ta)
{
    RET_IF(!ta, 0);
    return ta->font.size;
}

void
_textarea_destroy(TextArea *ta)
{
    RET_IF(!ta);
    int i = 0;
    for (i = 0 ; i < ta->len ; i++) {
        _text_destroy(ta->texts[i]);
    }
    free(ta);
}

void
_textarea_line_space_set(TextArea *ta, double line_space)
{
    RET_IF(!ta);
    ta->line_space = line_space;
}

double
_textarea_line_space_get(TextArea *ta)
{
    RET_IF(!ta, 0);
    return ta->line_space;
}

void
_textarea_bg_color_set(TextArea *ta, Color c)
{
    RET_IF(!ta);
    ta->bg_color = c;
}

Color
_textarea_bg_color_get(TextArea *ta)
{
    Color c = {0, 0, 0};
    RET_IF(!ta, c);
    return ta->bg_color;
}

void
_textarea_margin_set(TextArea *ta, int margin_left, int margin_top, int margin_right, int margin_bottom)
{
    RET_IF(!ta);
    ta->margin.left = margin_left;
    ta->margin.top = margin_top;
    ta->margin.right = margin_right;
    ta->margin.bottom = margin_bottom;
}

void
_textarea_margin_get(TextArea *ta, int *margin_left, int *margin_top, int *margin_right, int *margin_bottom)
{
    RET_IF(!ta);
    if (!margin_left) *margin_left = ta->margin.left;
    if (!margin_top) *margin_top = ta->margin.top;
    if (!margin_right) *margin_right = ta->margin.right;
    if (!margin_bottom) *margin_bottom = ta->margin.bottom;
}

void
_textarea_scroll(TextArea *ta, int x, int y)
{
    RET_IF(!ta);

    ta->content.x += x;
    ta->content.y += y;

    if (ta->content.y >= 0)
        ta->content.y = 0;
    else if (ta->content.h <= ta->h)
        ta->content.y = 0;
    else if (ta->content.y <= -(ta->content.h - ta->h))
        ta->content.y = -(ta->content.h - ta->h);

    if (ta->content.x >= 0)
        ta->content.x = 0;
    else if (ta->content.w <= ta->w)
        ta->content.x = 0;
    else if (ta->content.x <= -(ta->content.w - ta->w))
        ta->content.x = -(ta->content.w - ta->w);
}

void
_textarea_content_pos_get(TextArea *ta, int *x, int *y)
{
    RET_IF(!ta);
    if (x) *x = ta->content.x;
    if (y) *y = ta->content.y;
}

void
_textarea_resize(TextArea *ta, int w, int h)
{
    RET_IF(!ta);

    ta->w = w;
    ta->h = h;
}

static void
_texts_draw(Text **texts, int line_len, double line_space, cairo_t *cr, double x, double y, double margin_left, double margin_top, double *w, double *h)
{
    double max_w = 0, tot_h = 0;
    int i = 0;
    cairo_save(cr);
    // Draw multiple texts
    cairo_translate(cr, x, y);
    cairo_translate(cr, margin_left, margin_top);
    for (i = 0 ; i < line_len ; i++) {
        if (!texts[i]) continue;
        bool vertical;
        _text_get_direction(texts[i], &vertical, NULL);
        if (vertical) {
            if (i) cairo_translate (cr, line_space, 0);
        }
        else {
            if (i) cairo_translate (cr, 0, line_space);
        }

        // draw cairo
        _text_draw(texts[i], cr);

        double tw, th;
        tw = _text_get_width(texts[i]);
        th = _text_get_height(texts[i]);
        if (vertical) cairo_translate (cr, tw, 0);
        else cairo_translate (cr, 0, th);

        if (tw > max_w) max_w = tw;
        tot_h += th;
    }
    cairo_restore(cr);

    if (w) *w = max_w;
    if (h) *h = tot_h;
}

void
_textarea_attach(TextArea *ta, cairo_surface_t *surf)
{
    RET_IF(!ta);
    ta->surf = surf;
}

void
_textarea_render(TextArea *ta)
{
    RET_IF(!ta);
    RET_IF(!ta->surf);
    RET_IF(cairo_surface_status(ta->surf));

    int i = 0;
    for (i = 0 ; i < ta->len ; i++) {
        if (ta->font.family)
            _text_set_font_family(ta->texts[i], ta->font.family);
        if (ta->font.style)
            _text_set_font_style(ta->texts[i], ta->font.style);
        if (ta->font.size > 0)
            _text_set_font_size(ta->texts[i], ta->font.size);
        if (ta->font.color.a > 0)
            _text_set_fill_color(ta->texts[i],
                    ta->font.color.r, ta->font.color.g,
                    ta->font.color.b, ta->font.color.a);
    }

    cairo_t *cr;
    cr = cairo_create(ta->surf);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr,
            ta->bg_color.r, ta->bg_color.g, ta->bg_color.b, ta->bg_color.a);
    cairo_paint(cr);
    _texts_draw(ta->texts, ta->len, ta->line_space, cr,
            ta->content.x, ta->content.y,
            ta->margin.left, ta->margin.top, &(ta->content.w), &(ta->content.h));
    cairo_destroy(cr);
}

static void
_textarea_content_size_get(TextArea *ta, double *w, double *h)
{
    // FIXME: if it is not render, size is not defined yet.
    RET_IF(!ta);
    if (w) *w = ta->content.w;
    if (h) *h = ta->content.h;
}

static void
_textarea_font_color_set(TextArea *ta, Color c)
{
    RET_IF(!ta);
    ta->font.color = c;
}

Color
_textarea_font_color_get(TextArea *ta)
{
    Color c = {0, 0, 0, 0};
    RET_IF(!ta, c);
    return ta->font.color;
}


typedef struct _Context Context;
struct _Context
{
    struct nemocanvas *canvas;

    int width, height;
    double event_prev_y;

    struct talenode *text_node;
    TextArea *ta;

    struct talenode *set_node;
    cairo_matrix_t matrix;
    struct pathone *group;
    struct pathone *btn_one, *font_family_one, *font_size_one, *font_color_one;

    List *font_list;
    struct talenode *node_ff, *node_fs, *node_fc;
    TextArea *ta_ff, *ta_fs, *ta_fc;
    double btn_radius;

    int timer_cnt;
    double timer_diff_s;
};

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

#if 0
static void
_font_menu_animator(struct nemotimer *timer, void *data)
{
    Context *ctx = data;
    struct nemocanvas *canvas = ctx->canvas;
    //struct nemotool *tool = nemocanvas_get_tool(canvas);
    struct nemotale *tale  = nemocanvas_get_userdata(canvas);

    ctx->timer_cnt++;
    nemotale_handle_canvas_update_event(NULL, canvas, tale);

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

    if (ctx->timer_cnt < 20)  {
        nemotimer_set_timeout(timer, 1000.0/60);
    } else
        ctx->timer_cnt = 0;
}
#endif

static void
_transform_event(struct taletransition *trans, void *context, void *data)
{
	struct pathone *one = (struct pathone *)data;

	nemotale_path_transform_dirty(one);
}

void _btn_create_event(struct taletransition *trans, void *context, void *data)
{
    Context *ctx = context;
    struct nemocanvas *canvas = ctx->canvas;
    //struct nemotool *tool = nemocanvas_get_tool(canvas);
    struct nemotale *tale  = nemocanvas_get_userdata(canvas);

    ctx->timer_cnt++;
    if (ctx->timer_cnt > 50) return;
    nemotale_handle_canvas_update_event(NULL, canvas, tale);

    if (ctx->timer_cnt <= 20) {
        double scale = 0.1 + (0.9/20) * ctx->timer_cnt;
        nemotale_path_scale(ctx->font_family_one, scale, scale);
    }
    if ((ctx->timer_cnt > 10) && (ctx->timer_cnt <= 30)) {
        double scale = 0.1 + (0.9/20) * (ctx->timer_cnt - 10);
        nemotale_path_scale(ctx->font_size_one, scale, scale);
    }
    if ((ctx->timer_cnt > 20) && (ctx->timer_cnt <= 40)) {
        double scale = 0.1 + (0.9/20) * (ctx->timer_cnt - 20);
        nemotale_path_scale(ctx->font_color_one, scale, scale);
    }

    nemotale_node_clear_path(ctx->set_node);
	nemotale_node_damage_all(ctx->set_node);
	nemotale_path_update_one(ctx->group);
	nemotale_node_render_path(ctx->set_node, ctx->group);

	nemotale_composite(tale, NULL);
    nemotale_handle_canvas_flush_event(NULL, canvas, NULL);

    if ((ctx->timer_cnt == 20) || (ctx->timer_cnt == 40) ||
        (ctx->timer_cnt == 60)) {
        ctx->timer_diff_s = 0.9/20;
    }
}

static void
_view_update(Context *ctx, int width, int height)
{
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotale *tale = nemocanvas_get_userdata(canvas);
    cairo_matrix_t matrix;
    matrix.xx = 1;
    matrix.yx = 0;
    matrix.xy = 0;
    matrix.yy = 1;
    matrix.x0 = 0;
    matrix.y0 = 0;

    if ((width > 0) && (height > 0))
        nemocanvas_set_size(canvas, width, height);

    nemotale_handle_canvas_update_event(NULL, canvas, tale);

    // if size is changed
    if ((width > 0) && (height > 0)) {
        // Text layer
        nemotale_node_resize_pixman(ctx->text_node, width, height);

        double scale = width/640.;
        // Btn layer
        nemotale_node_resize_pixman(ctx->set_node, width, height);
        if (ctx->node_ff) {
            nemotale_node_resize_pixman(ctx->node_ff, scale*300., scale*90);
            nemotale_node_translate(ctx->node_ff, width/2.- scale*140, height/2. - scale*90);
            _textarea_resize(ctx->ta_ff, 300 * scale, 90 * scale);
        }
        if (ctx->node_fs) {
            nemotale_node_resize_pixman(ctx->node_fs, scale*140., scale*90);
            nemotale_node_translate(ctx->node_fs, width/2.- scale*140, height/2. + scale*40);
            _textarea_resize(ctx->ta_fs, 140 * scale, 90 * scale);
        }
        if (ctx->node_fc) {
            nemotale_node_resize_pixman(ctx->node_fc, scale*140., scale*90);
            nemotale_node_translate(ctx->node_fc, width/2.+ scale*20, height/2. + scale*40);
            _textarea_resize(ctx->ta_fc, 140 * scale, 90 * scale);
        }
        cairo_matrix_init_scale(&matrix,
                (double)width / ctx->width,
                (double)height / ctx->height);
    }

    // Text layer
    _textarea_attach(ctx->ta, nemotale_node_get_cairo(ctx->text_node));
    _textarea_render(ctx->ta);

    // Btn layer
    nemotale_node_clear_path(ctx->set_node);
    nemotale_node_damage_all(ctx->set_node);
    nemotale_path_set_parent_transform(ctx->group, &matrix);
    nemotale_path_update_one(ctx->group);
    nemotale_node_render_path(ctx->set_node, ctx->group);
    if (!ctx->btn_one) {
        double w = nemotale_get_width(tale);
        double h = nemotale_get_height(tale);
        int i = 0;
        struct talenode *node;
        TextArea *ta;
        Text **texts;

        if (!ctx->node_ff) {
            // font family
            node = nemotale_node_create_pixman(300, 90);
            nemotale_node_set_id(node, 10);
            nemotale_attach_node(tale, node);
            nemotale_node_translate(node, w/2 - 140, h/2 - 90);

            int num;
            List *fl;
            fl = _font_list_get(&num);
            ctx->font_list = fl;

            texts = malloc(sizeof(Text*) * 10);
            for (i = 0; i < 10 ; i++) {
                MyFont *font = list_idx_get_data(fl, i);
                char *str = _strdup_printf("%s:%s",
                        _font_family_get(font),
                        _font_style_get(font));
                texts[i] = _text_create(str);
            }
            ta = _textarea_create(texts, 10);
            _textarea_resize(ta, 300, 90);
            _textarea_font_size_set(ta, 30);
            _textarea_font_color_set(ta, mcolors[21]);
            ctx->node_ff = node;
            ctx->ta_ff = ta;
        }

        // font size
        if (!ctx->node_fs) {
            node = nemotale_node_create_pixman(140, 90);
            nemotale_node_set_id(node, 11);
            nemotale_attach_node(tale, node);
            nemotale_node_translate(node, w/2 - 140, h/2 + 40);

            texts = malloc(sizeof(Text*) * 10);
            for (i = 0; i < 12 ; i++) {
                char buf[3];
                if ((i == 0) || (i >= 11)) snprintf(buf, 3, " ");
                else
                    snprintf(buf, 3, "%2d", (10 + (i-1)*5));
                texts[i] = _text_create(buf);
            }
            ta = _textarea_create(texts, 12);
            _textarea_resize(ta, 140, 90);
            _textarea_font_size_set(ta, 30);
            _textarea_font_color_set(ta, mcolors[21]);
            ctx->node_fs = node;
            ctx->ta_fs = ta;
        }

        // font color
        if (!ctx->node_fc) {
            node = nemotale_node_create_pixman(140, 90);
            nemotale_node_set_id(node, 12);
            nemotale_attach_node(tale, node);
            nemotale_node_translate(node, w/2 + 20, h/2 + 40);

            texts = malloc(sizeof(Text*) * 10);
            for (i = 0; i < 10 ; i++) {
                texts[i] = _text_create("Color 1");

            }
            ta = _textarea_create(texts, 10);
            _textarea_resize(ta, 140, 90);
            _textarea_font_size_set(ta, 30);
            _textarea_font_color_set(ta, mcolors[21]);
            ctx->node_fc = node;
            ctx->ta_fc = ta;
        }
        _textarea_attach(ctx->ta_ff, nemotale_node_get_cairo(ctx->node_ff));
        _textarea_render(ctx->ta_ff);
        _textarea_attach(ctx->ta_fs, nemotale_node_get_cairo(ctx->node_fs));
        _textarea_render(ctx->ta_fs);
        _textarea_attach(ctx->ta_fc, nemotale_node_get_cairo(ctx->node_fc));
        _textarea_render(ctx->ta_fc);
    }

    nemotale_composite(tale, NULL);
    nemotale_handle_canvas_flush_event(NULL, canvas, NULL);
}

static void
_font_menu_anim_end(struct taletransition *trans, void *context, void *data)
{
    Context *ctx = context;
    _view_update(ctx, 0, 0);
}

static void
_font_menu_anim_begin(Context *ctx)
{
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    struct nemotale *tale  = nemocanvas_get_userdata(canvas);
    struct taletransition *trans;

#if 1
    trans = nemotale_transition_create(0, 1000);
    nemotale_transition_attach_timing(trans, 0.3f, NEMOEASE_CUBIC_OUT_TYPE);
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
    nemotale_transition_attach_event(trans,
            NEMOTALE_TRANSITION_EVENT_POSTUPDATE,
            _btn_create_event, ctx, NULL);
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
            _font_menu_anim_end, ctx, tale);

    nemotale_transition_attach_signal(trans, NTPATH_DESTROY_SIGNAL(ctx->group));
    nemotale_transition_attach_signal(trans, NTPATH_DESTROY_SIGNAL(ctx->font_family_one));

    nemotale_dispatch_transition_timer_event(tool, trans);
#else

    ctx->timer_diff_s = 0.9/20.0;

    struct nemotimer *timer = nemotimer_create(tool);
    nemotimer_set_timeout(timer, 1000.0/60);
    nemotimer_set_userdata(timer, ctx);
    nemotimer_set_callback(timer, _font_menu_animator);
    ctx->timer_cnt = 0;
#endif
}

static void
_btn_anim_end(struct taletransition *trans, void *_context, void *_data)
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
            mcolors[15].r, mcolors[15].g, mcolors[15].b, 0.8);
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
            mcolors[11].r, mcolors[11].g, mcolors[11].b, 0.8);
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
            mcolors[5].r, mcolors[5].g, mcolors[5].b, 0.8);
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
    ctx->btn_one = NULL;
    nemotale_node_clear_path(ctx->set_node);
	nemotale_node_damage_all(ctx->set_node);
    nemotale_path_update_one(ctx->group);
    nemotale_node_render_path(ctx->set_node, ctx->group);

	nemotale_composite(tale, NULL);
    nemotale_handle_canvas_flush_event(NULL, canvas, NULL);

    _font_menu_anim_begin(ctx);
}

static void
_btn_anim_begin(Context *ctx)
{
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    struct nemotale *tale  = nemocanvas_get_userdata(canvas);
    struct taletransition *trans;
    trans = nemotale_transition_create(0, 600);
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
            _btn_anim_end, ctx, NULL);

    nemotale_transition_attach_signal(trans, NTPATH_DESTROY_SIGNAL(ctx->group));

    nemotale_transition_attach_dattr(trans, NTPATH_TRANSFORM_ATX(ctx->btn_one),
            1.0f, ctx->width/2 - ctx->btn_radius);
    nemotale_transition_attach_dattr(trans, NTPATH_TRANSFORM_ATY(ctx->btn_one),
            1.0f, ctx->height/2 - ctx->btn_radius);
    nemotale_dispatch_transition_timer_event(tool, trans);
}

static cnt = 0;

static void
_main_exit_anim(struct nemotimer *timer, void *data)
{
    Context *ctx = data;
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    struct nemotale *tale = nemocanvas_get_userdata(canvas);

    _view_update(ctx, 200./cnt, 200./cnt);

    printf("%d\n", cnt);
    cnt++;
    if (cnt >= 200) {
        nemotool_exit(tool);
    } else
        nemotimer_set_timeout(timer, 1000./60.);

}

static void
_main_exit(Context *ctx)
{
    struct nemocanvas *canvas = ctx->canvas;
    struct nemotool *tool = nemocanvas_get_tool(canvas);
    //nemotool_exit(tool);

    struct nemotimer *timer = nemotimer_create(tool);
    nemotimer_set_timeout(timer, 1000./60.);
    nemotimer_set_userdata(timer, ctx);
    nemotimer_set_callback(timer, _main_exit_anim);
}

static void
_canvas_event(struct nemotale *tale, struct talenode *node, uint32_t type, struct taleevent *event)
{
    Context *ctx = nemotale_get_userdata(tale);
    struct nemocanvas *canvas = ctx->canvas;
    struct taletap *taps[16];
    int ntaps;
    ntaps = nemotale_get_taps(tale, taps, type);
    //ERR("type[%d], ntaps: %d, device[%ld] serial[%d] time[%d] value[%d] x[%lf] y[%lf] dx[%lf] dy[%lf]", type, ntaps, event->device, event->serial, event->time, event->value, event->x, event->y, event->dx, event->dy);

    if ((type & NEMOTALE_DOWN_EVENT) ||
        (type & NEMOTALE_UP_EVENT)) {
        ctx->event_prev_y = -9999;  // reset
    }

    if ((type & NEMOTALE_UP_EVENT) && (ntaps == 1) &&
        !nemotale_is_single_click(tale, event, type)) {
        struct taletap *tap = nemotale_get_tap(tale, event->device, type);
        struct pathone *one = tap->item;
        int y;
        if (one && !strcmp(NTPATH_ID(one), "font_family")) {
            _textarea_content_pos_get(ctx->ta_ff, NULL, &y);
            MyFont *font;
            int yy = y/-30;
            font = list_idx_get_data(ctx->font_list, yy);
            nemotale_handle_canvas_update_event(NULL, canvas, tale);
            nemotale_node_damage_all(ctx->set_node);
            nemotale_path_update_one(ctx->group);
            nemotale_node_render_path(ctx->set_node, ctx->group);

            const char *_family = _textarea_font_family_get(ctx->ta);
            const char *_style = _textarea_font_style_get(ctx->ta);
            const char *family = _font_family_get(font);
            const char *style = _font_style_get(font);
            if (strcmp(_family, family) ||
                strcmp(_style, style)) {
                _textarea_font_family_set(ctx->ta, family);
                _textarea_font_style_set(ctx->ta, style);
                _textarea_attach(ctx->ta,
                        nemotale_node_get_cairo(ctx->text_node));
                _textarea_render(ctx->ta);

                nemotale_composite(tale, NULL);
                nemotale_handle_canvas_flush_event(NULL, canvas, NULL);
            }

        } else if (one && !strcmp(NTPATH_ID(one),
                    "font_size")) {
            _textarea_content_pos_get(ctx->ta_fs, NULL, &y);
            int fsize = _textarea_font_size_get(ctx->ta_fs);
            int nsize = 10;
            if (y > -30) {
                nsize = 10;
            } else if ((y <= -30) && (y > -60)) {
                nsize = 15;
            } else if ((y <= -60) && (y > -90)) {
                nsize = 20;
            } else if ((y <= -90) && (y > -120)) {
                nsize = 25;
            } else if ((y <= -120) && (y > -150)) {
                nsize = 30;
            } else if ((y <= -150) && (y > -180)) {
                nsize = 35;
            } else if ((y <= -180) && (y > -210)) {
                nsize = 40;
            } else if ((y <= -210) && (y > -240)) {
                nsize = 45;
            } else if ((y <= -240) && (y > -270)) {
                nsize = 50;
            } else {
                nsize = 55;
            }
            if (fsize != nsize) {
                nemotale_handle_canvas_update_event(NULL, canvas, tale);
                nemotale_node_damage_all(ctx->set_node);
                nemotale_path_update_one(ctx->group);
                nemotale_node_render_path(ctx->set_node, ctx->group);

                _textarea_font_size_set(ctx->ta, nsize);

                _textarea_attach(ctx->ta, nemotale_node_get_cairo(ctx->text_node));
                _textarea_render(ctx->ta);
                nemotale_composite(tale, NULL);
                nemotale_handle_canvas_flush_event(NULL, canvas, NULL);
            }
        } else if (one && !strcmp(NTPATH_ID(one),
                    "font_color")) {
        }
    } else if (type & NEMOTALE_DOWN_EVENT) {
        struct taletap *tap = nemotale_get_tap(tale, event->device, type);
        tap->item = nemotale_path_pick_one(ctx->group, event->x, event->y);
        if (ntaps == 1) {
            struct pathone *one = tap->item;
            if (!one ||
                (strcmp(NTPATH_ID(one), "font_family") &&
                 strcmp(NTPATH_ID(one), "font_size") &&
                 strcmp(NTPATH_ID(one), "font_color"))) {
                nemocanvas_move(canvas, taps[0]->serial);
            }
        } else if (ntaps == 2) {
            // enable resize & rotate
            // 1: resize, 2:rotate
            nemocanvas_pick(canvas,
                    taps[0]->serial,
                    taps[1]->serial,
                    (1 << NEMO_SURFACE_PICK_TYPE_ROTATE) |
                    (1 << NEMO_SURFACE_PICK_TYPE_SCALE));
        } else if (ntaps == 3) {
            // disable resize & rotate
            nemocanvas_pick(canvas,
                    taps[0]->serial,
                    taps[1]->serial,
                    0);
        }
    } else if (nemotale_is_single_click(tale, event, type)) {
        if (ntaps == 1) {
            struct taletap *tap = nemotale_get_tap(tale, event->device, type);
            struct pathone *one = tap->item;
            if (one && !strcmp(NTPATH_ID(one), "start")) {
                _btn_anim_begin(ctx);
            }
        }
    } else {
        // scrolling
        if (ctx->event_prev_y == -9999) {
            ctx->event_prev_y = event->y; // reset
            return;
        }
        double pos_y = (event->y - ctx->event_prev_y);
        ctx->event_prev_y = event->y;
        if (ntaps == 1) {
            struct taletap *tap = nemotale_get_tap(tale, event->device, type);
            struct pathone *one = tap->item;
            if (one && !strcmp(NTPATH_ID(one), "font_family")) {
                nemotale_handle_canvas_update_event(NULL, canvas, tale);
                nemotale_node_damage_all(ctx->set_node);
                nemotale_path_update_one(ctx->group);
                nemotale_node_render_path(ctx->set_node, ctx->group);

                _textarea_attach(ctx->ta_ff, nemotale_node_get_cairo(ctx->node_ff));
                _textarea_scroll(ctx->ta_ff, 0, pos_y);
                _textarea_render(ctx->ta_ff);

                nemotale_composite(tale, NULL);
                nemotale_handle_canvas_flush_event(NULL, canvas, NULL);
            } else if (one && !strcmp(NTPATH_ID(one),
                        "font_size")) {
                nemotale_handle_canvas_update_event(NULL, canvas, tale);
                nemotale_node_damage_all(ctx->set_node);
                nemotale_path_update_one(ctx->group);
                nemotale_node_render_path(ctx->set_node, ctx->group);

                _textarea_scroll(ctx->ta_fs, 0, pos_y);
                _textarea_attach(ctx->ta_fs, nemotale_node_get_cairo(ctx->node_fs));
                _textarea_render(ctx->ta_fs);

                nemotale_composite(tale, NULL);
                nemotale_handle_canvas_flush_event(NULL, canvas, NULL);
            } else if (one && !strcmp(NTPATH_ID(one),
                        "font_color")) {
                nemotale_handle_canvas_update_event(NULL, canvas, tale);
                nemotale_node_damage_all(ctx->set_node);
                nemotale_path_update_one(ctx->group);
                nemotale_node_render_path(ctx->set_node, ctx->group);

                _textarea_scroll(ctx->ta_fc, 0, pos_y);
                _textarea_attach(ctx->ta_fc, nemotale_node_get_cairo(ctx->node_fc));
                _textarea_render(ctx->ta_fc);

                nemotale_composite(tale, NULL);
                nemotale_handle_canvas_flush_event(NULL, canvas, NULL);
            }
        } else if (ntaps == 3) {
            nemotale_handle_canvas_update_event(NULL, canvas, tale);

            nemotale_node_damage_all(ctx->set_node);
            nemotale_path_update_one(ctx->group);
            nemotale_node_render_path(ctx->set_node, ctx->group);

            _textarea_scroll(ctx->ta, 0, pos_y);
            _textarea_attach(ctx->ta, nemotale_node_get_cairo(ctx->text_node));
            _textarea_render(ctx->ta);

            nemotale_composite(tale, NULL);
            nemotale_handle_canvas_flush_event(NULL, canvas, NULL);
        }
    }
}

static void
_canvas_resize(struct nemocanvas *canvas, int32_t width, int32_t height)
{
    struct nemotale *tale  = nemocanvas_get_userdata(canvas);
    Context *ctx = nemotale_get_userdata(tale);

    if ((width <= 200) || (height <= 200)) {
         _main_exit(ctx);
         return;
    }

    _view_update(ctx, width, height);
}

int main(int argc, char *argv[])
{
    Context *ctx;
    TextArea *ta;
    int width, height;

    if (argc != 2 || !argv[1]) {
        ERR("Usage: %s [file name]", argv[0]);
        return 0;
    }

    if (!_font_init()) {
        ERR("_font_init failed");
        return -1;
    }

    width = 640;
    height = 640;

    ctx = calloc(sizeof(Context), 1);
    ctx->width = 640;
    ctx->height = 640;

    struct nemotool *tool;
    tool = nemotool_create();
    nemotool_connect_wayland(tool, NULL);

    struct nemocanvas *canvas;
    canvas = nemocanvas_create(tool);
    nemocanvas_set_size(canvas, width, height);
    nemocanvas_set_nemosurface(canvas, NEMO_SHELL_SURFACE_TYPE_NORMAL);
    nemocanvas_set_anchor(canvas, -0.5f, 0.5f);
    nemocanvas_set_dispatch_resize(canvas, _canvas_resize);
    ctx->canvas = canvas;

    struct nemotale *tale;
    tale = nemotale_create_pixman();
    nemotale_attach_canvas(tale, canvas, _canvas_event);
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
    node = nemotale_node_create_pixman(width, height);
    nemotale_node_set_id(node, 1);
    nemotale_attach_node(tale, node);
    ctx->text_node = node;

    ta = _textarea_create_from_file(argv[1]);
    if (!ta) {
        ERR("_textarea_create_from_file() failed");
        _font_shutdown();
        return -1;
    }
    _textarea_resize(ta, ctx->width, ctx->height);
    _textarea_font_size_set(ta, 15);
    _textarea_font_family_set(ta, "LiberationMono");
    _textarea_bg_color_set(ta, mcolors[19]);
    _textarea_font_color_set(ta, mcolors[21]);

    _textarea_attach(ta, nemotale_node_get_cairo(ctx->text_node));
    _textarea_render(ta);
    ctx->ta = ta;

    // button layer
    node = nemotale_node_create_pixman(ctx->width, ctx->height);
    nemotale_node_set_id(node, 2);
    nemotale_attach_node(tale, node);
    ctx->set_node = node;

    struct pathone *group;
    group = nemotale_path_create_group();
    ctx->group = group;

    struct pathone *one;
    double btn_radius = 50;
    double setbtn_x = ctx->width - btn_radius * 2 - 10;
    double setbtn_y = ctx->height - btn_radius * 2- 10;
    ctx->btn_radius = btn_radius;

    one = nemotale_path_create_circle(btn_radius);
    nemotale_path_set_id(one, "start");
    nemotale_path_attach_style(one, NULL);
    nemotale_path_translate(one, setbtn_x, setbtn_y);
    nemotale_path_set_fill_color(NTPATH_STYLE(one),
            mcolors[8].r, mcolors[8].g, mcolors[8].b, 0.8);
    nemotale_path_set_stroke_color(NTPATH_STYLE(one), 1.0f, 1.0f, 1.0f, 1.0f);
    nemotale_path_set_stroke_width(NTPATH_STYLE(one), 1.0f);
    nemotale_path_attach_one(group, one);
    ctx->btn_one = one;

    nemotale_path_update_one(group);
    nemotale_node_render_path(node, group);

    // Composite
	nemotale_composite(tale, NULL);
    nemotale_handle_canvas_flush_event(NULL, canvas, NULL);
    nemotool_run(tool);

    nemotale_destroy(tale);
    nemocanvas_destroy(canvas);
    nemotool_disconnect_wayland(tool);
    nemotool_destroy(tool);

    _textarea_destroy(ta);
    _font_shutdown();
    free(ctx);

    return 0;
}
