#ifndef __TEXT_H__
#define __TEXT_H__

#include <stdbool.h>
#include <cairo.h>

typedef struct _Font Font;
typedef struct _Text Text;

bool _font_init();
void _font_shutdown();
Font *_font_load(const char *family, const char *style, int size, int slant, int weight, int width, int spacing);

void _text_destroy(Text *text);
// You can restrict width and maximum number of line and set ellipsis.
// if width or line is below or equal to 0, it's useless)
Text *_text_create(Font *font, const char *utf8, const char *dir, const char *script, const char *lang, const char *features, double line_space, double width, unsigned int line_num, bool ellipsis);
void _text_draw_cairo(cairo_t *cr, Font *font, Text *text);
void _text_get_size(Text *txt, double *w, double *h);
void _text_get_direction(Text *txt, bool *vertical, bool *backward);

#endif
