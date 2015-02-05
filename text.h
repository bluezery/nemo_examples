#ifndef __TEXT_H__
#define __TEXT_H__

// open
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// close
#include <unistd.h>

// mmap, munmap
#include <sys/mman.h>

// bool type
#include <stdbool.h>

// errno type
#include <errno.h>

#include <hb-ft.h>
#include <hb-ot.h>
#include <freetype.h>

#include <cairo.h>
#include <cairo-ft.h>


#include "log.h"

typedef struct _File_Map
{
    char *data;
    size_t len;
} File_Map;

typedef struct _Font
{
    double size; // pixel size
    unsigned int upem;

    hb_font_t *hb_font;
    const char **shapers;

    FT_Face ft_face;

    // Drawing Font
    cairo_scaled_font_t *cairo_font;
    double cairo_scale; // used for converting size from upem space-> pixel space
    double height, max_width;
} Font;

typedef struct _Glyph {
    FT_Vector *points;
    unsigned long unicode;
    int num_points;
    short *contours;
    int num_contours;
    int height;
    double r, g, b, a;
    int line_width;
    char *tags;

    unsigned long code;
} Glyph;

typedef struct _Cairo_Text {
    double width, height;

    unsigned int num_glyphs;
    cairo_glyph_t *glyphs;

    unsigned int num_clusters;
    cairo_text_cluster_t *clusters;

    cairo_text_cluster_flags_t cluster_flags;
    cairo_scaled_font_t *font;
} Cairo_Text;

typedef struct _Text
{
    char *utf8;
    unsigned int utf8_len;

    bool vertical;
    bool backward;

    // Drawing Texts
    int line_num;
    double line_space;
    double width, height;
    Cairo_Text **cairo_texts;
} Text;


FT_Library _ft_lib;


static void
_file_map_destroy(File_Map *fmap)
{
    RET_IF(!fmap);
    if (munmap(fmap->data, fmap->len) < 0)
        ERR("munmap failed");
}

static File_Map *
_file_map_create(const char *file)
{
    RET_IF(!file, NULL);

    char *data;
    struct stat st;
    int fd;
    size_t len;

    if ((fd = open(file, O_RDONLY)) <= 0) {
        ERR("open failed");
        return NULL;
    }

    if (fstat(fd, &st) == -1) {
        ERR("fstat failed");
        close(fd);
        return NULL;
    }

    if (st.st_size == 0 && S_ISREG(st.st_mode)) {
        ERR("faile size is 0");
        close(fd);
        return NULL;
    }
    len = st.st_size;
    data = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        ERR("mmap failed: %s, len:%lu, fd:%d", strerror(errno), len, fd);
        close(fd);
        return NULL;
    }
    close(fd);

    File_Map *fmap = (File_Map *)malloc(sizeof(File_Map));
    fmap->data = data;
    fmap->len = len;
    return fmap;
}

static bool
_font_init()
{
    if (_ft_lib) return true;
    if (FT_Init_FreeType(&_ft_lib)) return false;
    return true;
}

static void
_font_shutdown()
{
    if (_ft_lib) FT_Done_FreeType(_ft_lib);
    _ft_lib = NULL;
}

static void
_font_destroy(Font *font)
{
    RET_IF(!font);
    if (font->hb_font) hb_font_destroy(font->hb_font);
    if (font->ft_face) FT_Done_Face(font->ft_face);
    if (font->cairo_font) cairo_scaled_font_destroy(font->cairo_font);
    free(font);
}
static FT_Face
_font_ft_create(const char *file, unsigned int idx)
{
    FT_Face ft_face;

    if (FT_New_Face(_ft_lib, file, idx, &ft_face)) return NULL;

    return ft_face;
}

// if backend is 1, it's freetype, else opentype
static hb_font_t *
_font_hb_create(const char *file, unsigned int idx, int backend)
{
    File_Map *map = NULL;
    hb_blob_t *blob;
    hb_face_t *face;
    hb_font_t *font;
    double upem;

    RET_IF(!file, NULL);

    map = _file_map_create(file);
    if (!map->data || !map->len) return NULL;

    blob = hb_blob_create(map->data, map->len,
            HB_MEMORY_MODE_READONLY_MAY_MAKE_WRITABLE, map,
            (hb_destroy_func_t)_file_map_destroy);
    if (!blob) {
        _file_map_destroy(map);
        return NULL;
    }

    face = hb_face_create (blob, idx);
    hb_blob_destroy(blob);
    if (!face) return NULL;
    upem = hb_face_get_upem(face);

    font = hb_font_create(face);
    hb_face_destroy(face);
    if (!font) return NULL;

    hb_font_set_scale(font, upem, upem);

    if (backend == 1)
        hb_ft_font_set_funcs(font); // FIXME: text width is weired for this backend
    else
        hb_ot_font_set_funcs(font);

#if 0 // Custom backend, not used yet.
    hb_font_set_funcs(font2, func, NULL, NULL);
#endif
    return font;
}

static cairo_scaled_font_t *
_font_cairo_scaled_create(const char *file, double height, unsigned int upem)
{
    RET_IF(!file, NULL);
    RET_IF(height <= 0, NULL);
    RET_IF(upem <= 0, NULL);

    cairo_font_face_t *cairo_face;
    cairo_matrix_t ctm, font_matrix;
    cairo_font_options_t *font_options;
    cairo_scaled_font_t *scaled_font;
    FT_Face ft_face;
    double scale;

    ft_face = _font_ft_create(file, 0);
    if (ft_face) {
        cairo_face = cairo_ft_font_face_create_for_ft_face(ft_face, 0);
        if (cairo_font_face_set_user_data(cairo_face, NULL, ft_face,
                    (cairo_destroy_func_t)FT_Done_Face)
            || cairo_font_face_status(cairo_face) != CAIRO_STATUS_SUCCESS) {
            ERR("cairo ft font face create for ft face failed");
            cairo_font_face_destroy(cairo_face);
            FT_Done_Face(ft_face);
            return NULL;
        }
    } else {
        ERR("Something wrong. Testing font is just created");
        cairo_face = cairo_toy_font_face_create("@cairo:sans",
                CAIRO_FONT_SLANT_NORMAL,
                CAIRO_FONT_WEIGHT_NORMAL);
    }

    // It's pixel size, not font size
    //scale = height * (upem)/(ft_face->max_advance_height);
    scale = height * upem/(ft_face->max_advance_height);
    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&font_matrix, scale, scale);

    font_options = cairo_font_options_create();
    cairo_font_options_set_hint_style(font_options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_hint_metrics(font_options, CAIRO_HINT_METRICS_OFF);
    cairo_font_options_set_antialias(font_options, CAIRO_ANTIALIAS_DEFAULT);

    if (cairo_font_options_get_antialias(font_options) == CAIRO_ANTIALIAS_SUBPIXEL) {
        cairo_font_options_set_subpixel_order(font_options, CAIRO_SUBPIXEL_ORDER_DEFAULT);
        /* cairo internal functions
        _cairo_font_options_set_lcd_filter(font_options, CAIRO_LCD_FILTER_DEFAULT);
        _cairo_font_options_set_round_glyph_positions(font_options, CAIRO_ROUND_GLYPH_POS_DEFAULT);
        */
    }

    scaled_font = cairo_scaled_font_create (cairo_face, &font_matrix, &ctm, font_options);
    cairo_font_options_destroy(font_options);
    cairo_font_face_destroy(cairo_face);

    return scaled_font;
}

static Font *
_font_create(const char *file, unsigned int idx, double size)
{
    RET_IF(!file, NULL);

    Font *font;

    hb_font_t *hb_font;

    FT_Face ft_face;

    cairo_scaled_font_t *cairo_font;

    unsigned int upem;

    hb_font = _font_hb_create(file, 0, 0);
    if (!hb_font) return NULL;

    // Usally, upem is 1000 for OpenType Shape font, 2048 for TrueType Shape font.
    // upem(Unit per em) is used for master (or Em) space.
    upem = hb_face_get_upem(hb_font_get_face(hb_font));

    // Create freetype !!
    ft_face = _font_ft_create(file, 0);
    if (!ft_face) return NULL;

    cairo_font = _font_cairo_scaled_create(file, size, upem);
    cairo_font_extents_t extents;
    cairo_scaled_font_extents(cairo_font, &extents);

    font = (Font *)calloc(sizeof(Font), 1);
    font->size = size;
    font->upem = upem;
    font->hb_font = hb_font;
    font->shapers = NULL;  //e.g. {"ot", "fallback", "graphite2", "coretext_aat"}
    font->ft_face = ft_face;
    font->cairo_font = cairo_font;
    // Scale will be used to multiply master outline coordinates to produce
    // pixel distances on a device.
    // i.e., Conversion from mater (Em) space into device (or pixel) space.
    font->cairo_scale = (double)size/upem;
    font->height = extents.height;
    font->max_width = extents.max_x_advance;

    return font;
}

static void
_text_cairo_destroy(Cairo_Text *ct)
{
    RET_IF(!ct);
    cairo_glyph_free(ct->glyphs);
    cairo_text_cluster_free(ct->clusters);
    cairo_scaled_font_destroy(ct->font);
}

// if from or to is -1, the ignore range.
static Cairo_Text *
_text_cairo_create(Font *font, hb_buffer_t *hb_buffer, const char* utf8, size_t utf8_len,
        int from, int to, bool is_cluster)
{
    unsigned int num_glyphs;
    hb_glyph_info_t *hb_glyphs;
    hb_glyph_position_t *hb_glyph_poses;
    bool backward;

    cairo_glyph_t *glyphs;
    unsigned int num_clusters = 0;
    cairo_text_cluster_t *clusters = NULL;
    cairo_text_cluster_flags_t cluster_flags = 0;
    int i = 0;
    unsigned int j;

    RET_IF(!font || !hb_buffer || !utf8, NULL);

    hb_glyphs = hb_buffer_get_glyph_infos(hb_buffer, &num_glyphs);
    if (!hb_glyphs || !num_glyphs) return NULL;

    hb_glyph_poses = hb_buffer_get_glyph_positions(hb_buffer, NULL);
    if (!hb_glyph_poses) return NULL;

    if ((from >= 0) && (to >= 0)) {
        if (from >= num_glyphs) from = num_glyphs - 1;
        if (to >= num_glyphs)  to = num_glyphs - 1;
        if (from > to) from = to;
        num_glyphs = to - from + 1;
    } else {
        from = 0;
        to = num_glyphs - 1;
    }

    glyphs = cairo_glyph_allocate (num_glyphs + 1);
    if (!glyphs) return NULL;

    hb_position_t x = 0, y = 0;
    for (i = 0, j = from ; i < num_glyphs ; i++, j++) {
        glyphs[i].index = hb_glyphs[j].codepoint;
        glyphs[i].x =  (hb_glyph_poses[j].x_offset + x) * font->cairo_scale;
        glyphs[i].y = -(hb_glyph_poses[j].y_offset + y) * font->cairo_scale;
        x += hb_glyph_poses[j].x_advance;
        y += hb_glyph_poses[j].y_advance;
    }
    glyphs[i].index = -1;
    glyphs[i].x = x * font->cairo_scale;
    glyphs[i].y = y * font->cairo_scale;

    // FIXME: If multiline is used, do not use cluster
    // for some languages (hebrew or japanese, etc.),
    // hb_buffer should be splitted for each line.
    if (is_cluster) {

        num_clusters = 1;
        for (i = from + 1 ; i < (to + 1); i++) {
            if (hb_glyphs[i].cluster != hb_glyphs[i-1].cluster)
                num_clusters++;
        }

        clusters = cairo_text_cluster_allocate(num_clusters);
        if (!clusters) {
            cairo_glyph_free(glyphs);
            return NULL;
        }
        memset(clusters, 0, num_clusters * sizeof(clusters[0]));

        backward = HB_DIRECTION_IS_BACKWARD(hb_buffer_get_direction(hb_buffer));
        cluster_flags =
            backward ? CAIRO_TEXT_CLUSTER_FLAG_BACKWARD : (cairo_text_cluster_flags_t) 0;

        unsigned int cluster = 0;
        const char *start = utf8;
        const char *end;
        clusters[cluster].num_glyphs++;

        if (backward) {
            // FIXME: backward tests!!!
            ERR("backward is not tested yet!!");
            abort();
            for (i = num_glyphs - 2; i >= 0; i--) {
                if (hb_glyphs[i].cluster != hb_glyphs[i+1].cluster) {
                    if (hb_glyphs[i].cluster >= hb_glyphs[i+1].cluster) {
                        ERR("cluster index is not correct");
                        cairo_glyph_free(glyphs);
                        cairo_text_cluster_free(clusters);
                        return NULL;
                    }
                    start = end;
                    cluster++;
                }
                clusters[cluster].num_glyphs++;
            }
            clusters[cluster].num_bytes = utf8 + utf8_len - start;
        } else {
            for (i = from + 1 ; i < (to + 1) ; i++) {
                if (hb_glyphs[i].cluster != hb_glyphs[i-1].cluster) {
                    if (hb_glyphs[i].cluster <= hb_glyphs[i-1].cluster) {
                        ERR("cluster index is not correct");
                        cairo_glyph_free(glyphs);
                        cairo_text_cluster_free(clusters);
                        return NULL;
                    }

                    clusters[cluster].num_bytes = hb_glyphs[i].cluster - hb_glyphs[i-1].cluster;
                    end = start + clusters[cluster].num_bytes;
                    start = end;
                    cluster++;
                }
                clusters[cluster].num_glyphs++;
            }
            clusters[cluster].num_bytes = utf8 + utf8_len - start;
        }
    }

    Cairo_Text *ct = (Cairo_Text *)calloc(sizeof(Cairo_Text), 1);
    //ct->width = glyphs[i].x ;
    //ct->height = font->size;
    ct->num_glyphs = num_glyphs;
    ct->glyphs = glyphs;
    ct->num_clusters = num_clusters;
    ct->clusters = clusters;
    ct->cluster_flags = cluster_flags;
    ct->font = cairo_scaled_font_reference(font->cairo_font);

    return ct;
}

// if return -1, no glyph can be exist within given width.
static int
_text_hb_get_idx_within_width(hb_buffer_t *buffer, unsigned int start, double width, double scale)
{
    RET_IF(!buffer, -1);
    RET_IF(start < 0, -1);

    unsigned int num_glyphs;
    hb_glyph_position_t *glyph_poses;
    double w = 0, prev_w = 0;
    unsigned int i = 0;

    glyph_poses = hb_buffer_get_glyph_positions(buffer, &num_glyphs);
    if (!glyph_poses) return 0;

    for (i = start; i < num_glyphs ; i++) {
        w += ((glyph_poses[i].x_offset + glyph_poses[i].x_advance) * scale);
        if (w >= width) break;
        prev_w = w;
    }

    return i - 1;
}

static hb_buffer_t *
_text_hb_create(hb_buffer_t *_hb_buffer, const char *utf8, unsigned int utf8_len,
        const char *dir, const char *script, const char *lang,
        const char *features, const char **hb_shapers, hb_font_t *hb_font)
{
    hb_buffer_t *hb_buffer;
    hb_feature_t *hb_features = NULL;
    unsigned int num_features = 0;

    RET_IF(!utf8 || utf8_len <= 0 || !hb_font, NULL);

    if (!_hb_buffer) hb_buffer = hb_buffer_create ();
    else hb_buffer = _hb_buffer;

    if (!hb_buffer_allocation_successful(hb_buffer)) {
        ERR("hb buffer create failed");
        return NULL;
    }
    hb_buffer_clear_contents(hb_buffer);

    // Each utf8 char is mapped into each glyph's unicode. (e.g. 한(ED 95 9C) => U+D55C)
    hb_buffer_add_utf8(hb_buffer, utf8, utf8_len, 0, utf8_len);

    if (dir) { // "LTR" (Left to Right), "RTL", "TTB", 'BTT", and lower case
        hb_direction_t hb_dir = hb_direction_from_string(dir, -1);
        hb_buffer_set_direction(hb_buffer, hb_dir);
    }
    if (script) { // ISO=15024 tag
        // "latn" (LATIN), "hang" (HANGUL), "Hira" (HIRAGANA), "Kana" (KATAKANA)
        // "Deva" (DEVANAGARI (Hindi language)), "Arab" (ARABIC), "Hebr" (HEBREW)
        hb_script_t hb_script = hb_script_from_string(script, -1);
        hb_buffer_set_script(hb_buffer, hb_script);
    }
    if (lang) {
        //LANG = getenv("LANG");
        hb_language_t hb_lang = hb_language_from_string(lang, -1);
        hb_buffer_set_language(hb_buffer, hb_lang);
    }

    hb_buffer_flags_t hb_flags = HB_BUFFER_FLAG_DEFAULT;
    // HB_BUFFER_FLAG_BOT | HB_BUFFER_FLAG_EOT | HB_BUFFER_FLAG_PRESERVE_DEFAULT_IGNORABLES;
    hb_buffer_set_flags(hb_buffer, hb_flags);

    // Default hb_buffer property setup (i.e. script assumption if script is unset)
    hb_buffer_guess_segment_properties(hb_buffer);
    // *********** Buffer End ***********//

    if (features) { // i.e. "kern,aalt=2"
        // CSS  font-feature-setting (except 'normal', 'inherited')
        // e.g. kern, kern=0, +kern, -kern, kern[3:5], kern[3], aalt=2
#if 0 // strtok version with realloc
        char *tok;
        char *org = strdup(features);
        while ((tok = strtok(org, ","))) {
            printf("tok: %s\n", tok);
            num_features++;
            hb_features = (hb_feature_t *)realloc(hb_features, sizeof(hb_feature_t) * num_features);
            if (!hb_feature_from_string(tok, strlen(tok), &hb_features[num_features -1])) {
                ERR("hb feature from string failed");
            }
            org = NULL;
        }
        free(org);
#else
        const char *start, *p;

        num_features++;
        p = features;
        while (p && *p) {
            if (*p == ',') num_features++;
            p++;
        }
        hb_features = (hb_feature_t *)malloc(sizeof(hb_feature_t) * num_features);

        unsigned int idx = 0;
        start = p = features;
        while (p) {
            if (*p == ',' || !*p) {
                hb_feature_from_string(start, p - start, &hb_features[idx]);
                if (!*p) break;
                start = p + 1;
                idx++;
            }
            p++;
        }
#endif
    }

#if 0// Get current available shaper list
    //e.g. "ot", "fallback", "graphite2", "coretext_aat", etc.
    const char **list = hb_shape_list_shapers();
    LOG("Available shapers: ");
    while (list && *list) {
        LOG("%s, ", *list);
        list++;
    }
    LOG("\n");
#endif

    // Shape buffer data by using buffer, features, shapers, etc..
    // Each glyph's unicode is mapped into glyph's codepoint. (e.g. U+D55C => CE0)
    if (!hb_shape_full(hb_font, hb_buffer, hb_features, num_features, hb_shapers)) {
        hb_buffer_set_length(hb_buffer, 0);
        ERR("hb shape full failed");
        if (hb_features) free(hb_features);
        hb_buffer_destroy(hb_buffer);
        return NULL;
    }
    if (hb_features) free(hb_features);

    // It can be used for cluster, seems to not be necessary for single cluster
    // hb_buffer_normalize_glyphs(hb_buffer);
    return hb_buffer;
}

static void
_text_destroy(Text *text)
{
    RET_IF(!text);
    free(text->utf8);
    for (unsigned int i = 0 ; i < text->line_num ; i++) {
        _text_cairo_destroy((text->cairo_texts)[i]);
    }
    free(text->cairo_texts);
    free(text);
}

static void
_str_ellipsis_append(char **_str, unsigned int *_str_len, unsigned int idx)
{
    char *str = *_str;
    unsigned int j = 0;
    unsigned int i = 0 ;
    unsigned int str_len;
    while (str[i]) {
        if (idx == j) break;
        if ((str[i] & 0xF0) == 0xF0)  { // 4 bytes
            i+=4;
        } else if ((str[i] & 0xE0) == 0xE0) { // 3 bytes
            i+=3;
        } else if ((str[i] & 0xC0) == 0xC0) { // 2 bytes
            i+=2;
        } else { // a byte
            i+=1;
        }
        j++;
    }
    str_len = i+3;
    str = (char *)realloc(str, sizeof(char) * (str_len + 1));
    str[i] = 0xE2;
    str[i+1] = 0x80;
    str[i+2] = 0xA6;
    str[i+3] = '\0';
    *_str_len = str_len;
}

// You can restrict width and maximum number of line and set ellipsis.
// if width or line is below or equal to 0, it's useless)
static Text *
_text_create(Font *font, const char *utf8,
        const char *dir, const char *script, const char *lang, const char *features,
        double line_space,
        double width, unsigned int line_num, bool ellipsis)
{
    RET_IF(!font || !utf8, NULL);

    Text *text;

    unsigned int utf8_len = 0;
    unsigned int str_len = 0;
    char *str = NULL;

    hb_buffer_t *hb_buffer;

    unsigned int num_glyphs;

    Cairo_Text **cairo_texts;

    utf8_len = strlen(utf8);
    if (utf8_len == 0) {
        ERR("utf8 length is 0");
        return NULL;
    }

    str = (char *)malloc(sizeof(char) * utf8_len);
#if 1
    // Remove special characters (e.g. line feed, backspace, etc.)
    for (unsigned int i = 0 ; i < utf8_len ; i++) {
        if (utf8[i] >> 31 ||
            (utf8[i] > 0x1F)) { // If it is valid character
            str[str_len] = utf8[i];
            str_len++;
        }
    }
    if (!str || str_len <=0 ) return NULL;
    str = (char *)realloc(str, sizeof(char) * (str_len + 1));
    str[str_len] = '\0';
#else
    str = strdup(utf8);
    str_len = utf8_len;
#endif

    hb_buffer = _text_hb_create(NULL, str, str_len, dir, script, lang, features, font->shapers, font->hb_font);
    num_glyphs = hb_buffer_get_length(hb_buffer);

    if (line_num <= 0) { // line num is not specified
        line_num = 1410065407; // infinite
    }
    if (width <= 0) { // width is not specified
        width = 1410065407; // infinite
    }

    // Calculate  line_num & ellipsis position.
    unsigned int ln = 1;
    double tw = 0;
    double lw = 0;
    unsigned int i = 0;
    double glyph_w = 0;
    hb_glyph_position_t *glyph_poses;
    glyph_poses = hb_buffer_get_glyph_positions(hb_buffer, NULL);
    for (i = 0; i < num_glyphs ; i++) {
        glyph_w = ((glyph_poses[i].x_offset + glyph_poses[i].x_advance) * font->cairo_scale);
        lw += glyph_w;
        tw += glyph_w;
        if (lw >= width) {
            i--;
            ln++;
        }
        if (ln > line_num) {
            ln--;
            break;
        }
        if (lw >= width) lw = 0;
    }
    if (tw < width) width = tw;

    line_num = ln;
    if (ellipsis && (i < num_glyphs)) { // ellipsis is needed
        // If width is too small for ellipsis glyph, change last glyph as ellipsis.
        if ((lw - glyph_w + font->max_width) > width)  i--;

        _str_ellipsis_append(&str, &str_len, i);
        // shaping again with ellipsis
        hb_buffer = _text_hb_create(hb_buffer, str, str_len,
                dir, script, lang, features, font->shapers, font->hb_font);
        num_glyphs = hb_buffer_get_length(hb_buffer);
    }

    cairo_texts = (Cairo_Text **)malloc(sizeof(Cairo_Text *) * line_num);

    if ((line_num == 1) && (i <= num_glyphs)) {// if exact single line
        // FIXME: This should be here because  _text_hb_get_idx_within_width()
        // function cannot solve double precison for width.
        cairo_texts[0] = _text_cairo_create(font, hb_buffer, str, str_len, 0, i, true);
    } else {
        unsigned int from = 0, to;
        for (unsigned int i = 0 ; i < line_num ; i++) {
            to = _text_hb_get_idx_within_width(hb_buffer, from, width, font->cairo_scale);
            cairo_texts[i] = _text_cairo_create(font, hb_buffer, str, str_len, from, to, false);
            from = to + 1;
        }
    }

    text = (Text *)malloc(sizeof(Text));
    text->utf8 = str;
    text->utf8_len = str_len;
    text->vertical = HB_DIRECTION_IS_VERTICAL(hb_buffer_get_direction(hb_buffer));
    text->backward = HB_DIRECTION_IS_BACKWARD(hb_buffer_get_direction(hb_buffer));
    text->line_num = line_num;
    text->line_space = line_space;
    text->width = width;
    text->height = (font->height *line_num) + (line_space * (line_num -1));
    text->cairo_texts = cairo_texts;

    hb_buffer_destroy(hb_buffer);
    return text;
}

static void
_text_cairo_draw(cairo_t *cr, Font *font, Text *text)
{
    cairo_save(cr);

    cairo_set_scaled_font(cr, font->cairo_font);


    cairo_font_extents_t font_extents;
    cairo_font_extents(cr, &font_extents);

    double descent;

    if (text->vertical) {
        descent = font_extents.height * (text->line_num + .5);
        cairo_translate (cr, descent, 0);
    } else {
        descent = font_extents.descent;
        cairo_translate (cr, 0, -descent);
    }

    Cairo_Text **cts = text->cairo_texts;

    for (unsigned int i = 0 ; i < text->line_num ; i++) {
        Cairo_Text *ct = cts[i];

        if (text->vertical) {
            if (i) cairo_translate (cr, -text->line_space, 0);
            cairo_translate (cr, -font_extents.height, 0);
        }
        else {
            if (i) cairo_translate (cr, 0, text->line_space);
            cairo_translate (cr, 0, font_extents.height);
        }

        // annotate
        if (0) {
            cairo_save (cr);
            /* Draw actual glyph origins */
            cairo_set_source_rgba (cr, 1., 0., 0., .5);
            cairo_set_line_width (cr, 5);
            cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

            for (unsigned i = 0; i < ct->num_glyphs; i++) {
                cairo_move_to (cr, ct->glyphs[i].x, ct->glyphs[i].y);
                cairo_rel_line_to (cr, 0, 0);
            }
            cairo_stroke (cr);

            cairo_restore (cr);
        }

        if (ct->num_clusters){
            cairo_show_text_glyphs (cr,
                    text->utf8, text->utf8_len,
                    ct->glyphs, ct->num_glyphs,
                    ct->clusters, ct->num_clusters,
                    ct->cluster_flags);
        } else if (1) {
            cairo_show_glyphs (cr, ct->glyphs, ct->num_glyphs);
        } else {
            /* This should be image suface */
            /* cairo_show_glyphs() doesn't support subpixel positioning */
            cairo_glyph_path (cr, ct->glyphs, ct->num_glyphs);
            cairo_fill (cr);
        }
    }

    cairo_restore (cr);
}



#if 0
#include <iconv.h>

static Text *
_create_text2(const char *utf8, const char *dir, const char *script, const char *lang,
            const char *features, Font *font)
{
    Glyph **glyphs;

    unsigned int num_glyphs = 0;
    unsigned int utf8_len;

    if (!utf8 || !font) {
        ERR("utf8 is NULL or font is NULL");
        return NULL;
    }
    utf8_len = strlen(utf8);
    if (utf8_len == 0) {
        ERR("utf8 length is 0");
        return NULL;
    }

    char inbuf[256] = "가나다";
    char outbuf[256];

    size_t inbuf_left = strlen(inbuf);
    size_t outbuf_left = sizeof(outbuf);

    char *in = inbuf;
    char *out = outbuf;
    memset(outbuf, 0, 256);

    LOG("inbuf_left: %zu, outbuf_left: %zu", inbuf_left, outbuf_left);
    for (int i = 0 ; i < inbuf_left ; i++) {
        LOG("%x", in[i]);
    }

    ssize_t ret;
    iconv_t ic = iconv_open("UCS4", "UTF8");
    ret = iconv(ic, &in, &inbuf_left, &out, &outbuf_left);
    if (ret < 0) {
        ERR("iconv failed: %s", strerror(errno));
    } else {
        LOG("%zu[%s]  %zu[%s]", outbuf_left, outbuf, inbuf_left, inbuf);
    }

    LOG("inbuf_left: %zu, outbuf_left: %zu", inbuf_left, outbuf_left);
    unsigned int i = 0;

    while (outbuf[i] != '\0' || outbuf[i+1] != '\0' ||  outbuf[i+2] != '\0' || outbuf[i+3] != '\0') {
        i+=4;
        num_glyphs++;
    }
    glyphs = (Glyph **)calloc(sizeof(Glyph *), num_glyphs);

    while (outbuf[i] != '\0' || outbuf[i+1] != '\0' ||  outbuf[i+2] != '\0' || outbuf[i+3] != '\0') {
        unsigned long buf;
        unsigned long bu[4];
        bu[0] = (outbuf[i] << 24) & 0xFF000000;
        bu[1] = (outbuf[i+1] << 16) & 0xFF0000;
        bu[2] = (outbuf[i+2] << 8) & 0xFF00;
        bu[3] = outbuf[i+3] & 0xFF;
        buf = bu[0] + bu[1] + bu[2] + bu[3];
        LOG("%lx", buf);
        i+=4;
        glyphs[i/4] = (Glyph *)calloc(sizeof(Glyph), 1);
        glyphs[i/4]->unicode = buf;
    }
    return NULL;
}

static void
_draw_glyph(Glyph *glyph, cairo_t *cr)
{
    if (!glyph) return;
    FT_Vector *points = glyph->points;
    short *contours = glyph->contours;
    char *tags = glyph->tags;
    int i, j, o, n;
	for (i = 0; i < glyph->num_points; i++) {
		points[i].y = points[i].y * -1 + glyph->height;
	}

    cairo_scale(cr, 0.1, 0.1);
#if 1
    cairo_set_line_width(cr, 5);
    cairo_set_source_rgba(cr, 0, 0, 1, 1);
	for (i = 0; i < glyph->num_points; i++) {
	    cairo_rectangle(cr, points[i].x, points[i].y, 10, 10);
	}
#endif
    cairo_set_line_width(cr, glyph->line_width);
    cairo_set_source_rgba(cr, glyph->r, glyph->g, glyph->b, glyph->a);

	for (i = 0, o = 0; i < glyph->num_contours; i++) {
		n = contours[i] - o + 1;

        cairo_move_to(cr, points[o].x, points[o].y);

		for (j = 0; j < n; j++) {
			int p0 = (j + 0) % n + o;
			int p1 = (j + 1) % n + o;
			int p2 = (j + 2) % n + o;
			int p3 = (j + 3) % n + o;

			if (tags[p0] == 0) {
			} else if (tags[p1] != 0) {
			    cairo_line_to(cr,
			            points[p1].x,
			            points[p1].y);
			} else if (tags[p2] != 0) {
			    // FIXME: quaratic bezier curve implementation
			} else if (tags[p3] != 0) {
			    cairo_curve_to(cr,
			            points[p1].x,
			            points[p1].y,
			            points[p2].x,
			            points[p2].y,
			            points[p3].x,
			            points[p3].y);
			}
		}
		o = contours[i] + 1;
	}
	cairo_stroke(cr);
}
#endif


#endif
