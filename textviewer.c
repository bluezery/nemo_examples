// errno type
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <fontconfig/fontconfig.h>

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

int main(int argc, char *argv[])
{
    Text **text;
    char **line_txt;
    int line_len;

    if (argc != 2 || !argv[1]) {
        ERR("Usage: show [file name]");
        return 0;
    }

    if (!_font_init()) {
        ERR("_font_init failed");
        return -1;
    }

    /*
    Font *font;
    font = _font_load(NULL, "Bold", 70, -1, -1, -1, -1);
    */

    // Read a file
    line_txt = _read_file(argv[1], &line_len);
    if (!line_txt || !line_txt[0] || line_len <= 0) {
        ERR("Err: line_txt is NULL or no string or length is 0");
        return -1;
    }

    double line_space = 0;
    // Create Text
    text = (Text **)malloc(sizeof(Text *) * line_len);
    for (int i = 0 ; i < line_len ; i++) {
        text[i] = _text_create(line_txt[i]); //NULL, NULL, NULL, NULL, line_space, 0, 3, true);
        //_text_set_font_family(text[i], "LiberationMono");
        //_text_set_direction(text[i], false, true);
        //_text_set_width(text[i], 200);
        //_text_set_height(text[i], 130);
        //_text_set_ellipsis(text[i], true);
        _text_set_font_size(text[i], 50);
        //_text_set_wrap(text[i], 1);
        free(line_txt[i]);
    }
    free(line_txt);

    // calculate width, height
    int w = 0, h = 0;
    /*
    for (int i = 0 ; i < line_len ; i++) {
        double tw, th;
        _text_get_size(text[i], &tw, &th);
        if (w < tw)
            w = tw;
        h += th;
    }*/
    w = 700;
    h = 600;

    View *v;
    cairo_t *cr;
    view_init();
    v = view_create(w, h, 255, 255, 255, 255);
    cr = view_get_cairo(v);

    // Draw multiple texts
    double margin_left = 0, margin_top = 0;
    //double margin_right = 0, margin_bottom = 0;
    cairo_save(cr);
    cairo_translate(cr, margin_left, margin_top);
    ERR("%d", line_len);
    for (int i = 0 ; i < line_len ; i++) {
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

        // Draw text bounding box
        cairo_save(cr);
        double tw, th;
        tw = _text_get_width(text[i]);
        th = _text_get_height(text[i]);
        //_text_get_size(text[i], &tw, &th);
        cairo_rectangle(cr, 0, 0, tw, th);
        cairo_move_to(cr, 100, 30);
        cairo_set_line_width(cr, 1);
        cairo_set_source_rgba(cr, 1, 0, 0, 1);
        cairo_stroke(cr);
        cairo_restore(cr);

        if (vertical) cairo_translate (cr, tw, 0);
        else cairo_translate (cr, 0, th);
    }
    cairo_restore(cr);

    view_do(v);
    view_destroy(v);

    view_shutdown();

    _text_destroy(text[0]);
    free(text);

    _font_shutdown();

    return 0;
}
