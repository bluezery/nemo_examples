#ifndef __TEXT_H__
#define __TEXT_H__

#include <stdbool.h>
#include <cairo.h>

typedef struct _Font MyFont;
typedef struct _Text Text;

bool _font_init();
void _font_shutdown();
MyFont *_font_load(const char *family, const char *style, int slant, int weight, int width, int spacing);

void _text_destroy(Text *t);
Text *_text_create(const char *utf8);
void _text_draw(Text *t, cairo_t *cr);
bool _text_set_font_family(Text *t, const char *font_family);
const char * _text_get_font_family(Text *t);
bool _text_set_font_style(Text *t, const char *font_style);
const char * _text_get_font_style(Text *t);
bool _text_set_font_size(Text *t, int font_size);
unsigned int _text_get_font_size(Text *t);
bool _text_set_font_slant(Text *t, int font_slant);
int _text_get_font_slant(Text *t);
bool _text_set_font_weight(Text *t, int font_weight);
int _text_get_font_weight(Text *t);
bool _text_set_font_width(Text *t, int font_width);
unsigned int _text_get_font_width(Text *t);
bool _text_set_font_spacing(Text *t, int font_spacing);
int _text_get_font_spacing(Text *t);
bool _text_set_script(Text *t, const char *script);
char *_text_get_script(Text *t);
bool _text_set_lang(Text *t, const char *lang);
const char *_text_get_lang(Text *t);
bool _text_set_kerning(Text *t, bool kerning);
bool _text_get_kerning(Text *t);

bool _text_set_direction(Text *t, bool vertical, bool backward); //
void _text_get_direction(Text *t, bool *vertical, bool *backward); //
bool _text_set_anchor(Text *t, double anchor);
double _text_get_anchor(Text *t);

bool _text_set_fill_color(Text *t, unsigned int r, unsigned int g, unsigned int b, unsigned int a);
void _text_get_fill_color(Text *t, unsigned int *r, unsigned int *g, unsigned int *b, unsigned int *a);
bool _text_set_stroke_color(Text *t, unsigned int r, unsigned int g, unsigned int b, unsigned int a);
void _text_get_stroke_color(Text *t, unsigned int *r, unsigned int *g, unsigned int *b, unsigned int *a);
bool _text_set_stroke_width(Text *t, double w);
double _text_get_stroke_width(Text *t);
bool _text_set_letter_space(Text *t, int space);
int _text_get_letter_space(Text *t);
bool _text_set_word_space(Text *t, int space);
int _text_get_word_space(Text *t);

bool _text_set_decoration(Text *t, unsigned int decoration);
unsigned int _text_get_decoration(Text *t);
bool _text_set_ellipsis(Text *t, bool ellipsis);
bool _text_get_ellipsis(Text *t);
bool _text_set_wrap(Text *t, int wrap);
int _text_get_wrap(Text *t);
bool _text_set_hint_width(Text *t, double width);
double _text_get_hint_width(Text *t);
bool _text_set_hint_height(Text *t, double height);
double _text_get_hint_height(Text *t);
double _text_get_width(Text *t);
double _text_get_height(Text *t);
unsigned int _text_get_line_num(Text *t);
bool _text_set_line_space(Text *t, double line_space);
double _text_get_line_space(Text *t);
bool _text_set_font_auto_resize(Text *t, bool auto_resize);
bool _text_get_font_auto_resize(Text *t);

// You can restrict width and maximum number of line and set ellipsis.
// if width or line is below or equal to 0, it's useless)
//Text *_text_create_all(MyFont *font, const char *utf8, const char *dir, const char *script, const char *lang, const char *features, double line_space, double width, unsigned int line_num, bool ellipsis);
void _text_draw_cairo(cairo_t *cr, Text *text);
void _text_get_direction(Text *t, bool *vertical, bool *bacward);

#endif
