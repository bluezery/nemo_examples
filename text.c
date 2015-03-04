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

#include "util.h"
#include "log.h"
#include "text.h"
#include "nemolist.h"

#define EPSILON 0.001
#define ROUND(a)
#define EQUAL(a, b) ((a >b) ? ((a-b) < EPSILON) : ((b-a) < EPSILON))

typedef struct _File_Map File_Map;
struct _File_Map {
    char *data;
    size_t len;
};

struct _Font {
    struct nemolist link;

    // font config proerties
    char *filepath;
    int idx;
    char *font_family;
    char *font_style;
    unsigned int font_slant;
    unsigned int font_weight;
    unsigned int font_spacing;
    unsigned int font_width;

    // free type
    FT_Face ft_face;

    // Harfbuzz
    hb_font_t *hb_font;
    const char **shapers;

    cairo_scaled_font_t *cairo_font;
};

struct nemolist _font_list;

typedef struct _Glyph Glyph;
struct _Glyph {
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
};

typedef struct _Cairo_Text Cairo_Text;
struct _Cairo_Text {
    double width, height;

    unsigned int num_glyphs;
    cairo_glyph_t *glyphs;

    unsigned int num_clusters;
    cairo_text_cluster_t *clusters;

    cairo_text_cluster_flags_t cluster_flags;
    cairo_scaled_font_t *font;
};

struct _Text
{
    // Harbufbuzz
    hb_buffer_t *hb_buffer;
    char *utf8;
    unsigned int utf8_len;
    hb_direction_t hb_dir;
    hb_script_t hb_script;
    hb_language_t hb_lang;
    bool kerning;
    MyFont *font;

    // CSS property
    double anchor;  // start:0, middle:0.5, 1.0:end
    int fill_r, fill_g, fill_b, fill_a; // if fill_a is 0, this will not be applied
    int stroke_r, stroke_g, stroke_b, stroke_a; // if stroke_a is 0, this will not be applied
    double stroke_width;
    int letter_space;
    int word_space;
    unsigned int decoration;

    // fontconfig property
    char *font_family;
    char *font_style;
    int font_size;   // pixel size;
    int font_slant;
    int font_weight;
    int font_width;
    int font_spacing;

    double hint_width, hint_height;
    // Drawing Texts
    bool dirty;
    bool ellipsis;
    int wrap;
    bool auto_resize;
    int line_num;
    double line_space;
    double width, height;
    Cairo_Text **cairo_texts;
    double cairo_scale;
};

FT_Library _ft_lib;
FcConfig *_font_config;

#ifdef DEBUG
static void
_dump_cairo_font_extents(cairo_font_extents_t extents)
{
    LOG("ascent:%lf, descent:%lf, height:%lf, max_x_adv:%lf, max_y_adv:%lf",
         extents.ascent, extents.descent, extents.height, extents.max_x_advance,
         extents.max_y_advance);
}

static void
_dump_text(Text *text)
{
    LOG("[%d]%s", text->utf8_len, text->utf8);
    LOG("vertical:%d backward: %d", text->vertical, text->backward);
    LOG("num glyphs:[%3d]", text->num_glyphs);
    int i = 0;
    for(i = 0 ; i < text->num_glyphs ; i++) {
        LOG("[%3d]: index:%6lX, x:%3.3lf y:%3.3lf", i,
            (text->glyphs)[i].index, (text->glyphs)[i].x, (text->glyphs)[i].y);
    }
    LOG("num clusters:[%3d] cluster flags[%3d]", text->num_clusters, text->cluster_flags);
    int i = 0;
    for(i = 0 ; i < text->num_clusters ; i++) {
        LOG("[%3d]: num_bytes:%3d, num_glyphs:%3d", i,
            (text->clusters)[i].num_bytes, (text->clusters)[i].num_glyphs);
    }
}

static void
_dump_hb_glyph(hb_glyph_info_t info, hb_glyph_position_t pos, hb_direction_t dir, hb_buffer_content_type_t type, hb_font_t *font)
{
    LOG("codepoint:%4x, mask:%4x, cluster:%d, x_advance:%3d, y_advance:%3d, x_offset:%3d, y_offset:%3d",
        info.codepoint, info.mask, info.cluster,
        pos.x_advance, pos.y_advance, pos.x_offset, pos.y_offset);

    if (font) {
        hb_codepoint_t code;
        hb_codepoint_t pcode = 0;
        char name[256], str[256];

        // Harfbuzz codepoint
        if (type == HB_BUFFER_CONTENT_TYPE_UNICODE) { // before shaping, get codepoint from unicode
            hb_font_get_glyph(font, info.codepoint, 0, &code);
        } else {  // itself already codepint.
            code = info.codepoint;
        }
        hb_font_get_glyph_name(font, code, name, 255);
        hb_font_glyph_to_string(font, code, str , 255); // Glyph ID
        LOG("glyph_name:[%s] glyph_string:[%s]", name, str);

        // Origin
        hb_position_t org_x, org_y, orgh_x, orgh_y, orgv_x, orgv_y;
        hb_font_get_glyph_origin_for_direction(font, code, dir, &org_x, &org_y);
        hb_font_get_glyph_h_origin(font, code, &orgh_x, &orgh_y);
        hb_font_get_glyph_v_origin(font, code, &orgv_x, &orgv_y);
        LOG("Origin CurretDir[x:%d, y:%d] / hori(x:%d, y:%d)  vert(x:%d, y:%d)",
            org_x, org_y, orgh_x, orgh_y, orgv_x, orgv_y);

        // Advance
        hb_position_t h_adv, v_adv;
        hb_font_get_glyph_advance_for_direction(font, code, dir, &h_adv, &v_adv);
        LOG("Advance[hori:%d vert:%d] / hori(%d), vert(%d)",
            h_adv, v_adv,
            hb_font_get_glyph_h_advance(font, code), hb_font_get_glyph_v_advance(font, code));

        // Extents
        hb_glyph_extents_t extd, ext;
        hb_font_get_glyph_extents_for_origin(font, code, dir, &extd);
        hb_font_get_glyph_extents(font, code, &ext);
        LOG("Extents CurrentDir[x_bearing:%d, y_bearing:%d, width:%d, height:%d] [x_bearing:%d, y_bearing:%d, width:%d, height:%d]",
            extd.x_bearing, extd.y_bearing, extd.width, extd.height,
            ext.x_bearing, ext.y_bearing, ext.width, ext.height);

        // Kerning
        if (pcode != 0) {
            hb_position_t x, y;
            hb_position_t xx = 0, yy = 0;
            if (HB_DIRECTION_IS_HORIZONTAL(dir))
                xx = hb_font_get_glyph_h_kerning(font, code, pcode);
            else
                yy = hb_font_get_glyph_v_kerning(font, code, pcode);
            hb_font_get_glyph_kerning_for_direction(font, code, pcode, dir, &x, &y);
            LOG("kerning CurrentDir[x:%d y:%d] [x:%d, y:%d]", x, y, xx, yy);
        }
        pcode = code;
#if 0
        LOG("============= contour =============");
        unsigned int idx = 0;
        hb_position_t cx, cy, cxx, cyy;
        while (hb_font_get_glyph_contour_point_for_origin(font, code, idx, dir, &cx, &cy)) {
            if (!hb_font_get_glyph_contour_point(font, code, idx, &cxx, &cyy)) {
                cxx = 0;
                cyy = 0;
            }
            LOG("\t[%2d] cx: %d(%d), cy: %d(%d)", idx, cx, cxx, cy, cyy);
            idx++;
        }
        LOG("===================================");
#endif
    }
}

static void
_dump_harfbuzz(hb_buffer_t *buffer, hb_font_t *font)
{
    hb_face_t *face;
    unsigned int x_ppem, y_ppem;
    int x_scale, y_scale;
    unsigned int num_glyphs;
    hb_segment_properties_t prop;
    hb_direction_t dir = hb_buffer_get_direction(buffer);
    hb_buffer_content_type_t type = hb_buffer_get_content_type(buffer);
    hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer, &num_glyphs);
    hb_glyph_position_t *poses = hb_buffer_get_glyph_positions(buffer, NULL);
    hb_buffer_get_segment_properties(buffer, &prop);
    hb_script_t script = hb_buffer_get_script(buffer);
    hb_language_t lang = hb_buffer_get_language(buffer);

    hb_font_get_ppem(font, &x_ppem, &y_ppem);
    hb_font_get_scale(font, &x_scale, &y_scale);
    face = hb_font_get_face(font);
    LOG("========================== MyFont info ===============================");
    LOG("\tx_ppem: %u, y_ppem: %u", x_ppem, y_ppem);
    LOG("\tx_scale: %d, y_scale: %d", x_scale, y_scale);
    LOG("\tidx:%u upem:%u glyph_count:%u", hb_face_get_index(face), hb_face_get_upem(face),
            hb_face_get_glyph_count(face));
    LOG("========================== Buffer info ===============================");
    LOG("\tcontent_type: %d [unicode = 1, glyphs] Length:%d", type, hb_buffer_get_length(buffer));
    LOG("\tdir: %d [LTR = 4, RTL, TTB, BTT]", dir);
    LOG("\tcode point: %d",  hb_buffer_get_replacement_codepoint(buffer));
    LOG("\tflag: %u [DEFAULT = 0, BOT = 1u, EOT = 2u, IGNORE = 4u",  hb_buffer_get_flags(buffer));
    LOG("\tScript:%c%c%c%c, Language:%s",  script >> 24, script >> 16,
        script >> 8, script, hb_language_to_string(lang));
    LOG("\tProperty (Dir:%d, script:%c%c%c%c, language:%s)", prop.direction,
        prop.script >> 24, prop.script >> 16, prop.script >> 8, prop.script,
        hb_language_to_string(prop.language));

    unsigned int i = 0;
    for (i = 0 ; i < num_glyphs ; i++) {
        LOG("==================== [%d] Glyph info =========================", i);
        _dump_hb_glyph(infos[i], poses[i], dir, type, font);
    }
}

static void
_dump_cairo_glyph(cairo_glyph_t *glyph, unsigned int num_glyph)
{
    LOG("========================== cairo glyphs info ===============================");
    unsigned int i = 0;
    for (i = 0 ; i < num_glyph ; i++) {
        LOG("[%2d] index: %6lX, x:%7.2lf y:%7.2lf",
            i, glyph[i].index, glyph[i].x, glyph[i].y);
    }
}

static void
_dump_cairo_text_cluster(cairo_text_cluster_t *cluster, unsigned int num_cluster)
{
    LOG("========================== cairo clusters info ===============================");
    unsigned int i = 0;
    for (i = 0 ; i < num_cluster ; i++) {
        LOG("[%2d] num_bytes: %d, num_glyphs: %d",
            i, cluster[i].num_bytes, cluster[i].num_glyphs);
    }
}

static void
_dump_ft_face(FT_Face ft_face)
{
    LOG("===================== MyFont Face Info ==================");
    LOG("has vertical: %ld", FT_HAS_VERTICAL(ft_face));
    LOG("has horizontal: %ld", FT_HAS_HORIZONTAL(ft_face));
    LOG("has kerning: %ld", FT_HAS_KERNING(ft_face));
    LOG("sfnt: %ld", FT_IS_SFNT(ft_face));
    LOG("trick: %ld", FT_IS_TRICKY(ft_face));
    LOG("scalable: %ld", FT_IS_SCALABLE(ft_face));
    LOG("fixed_width: %ld", FT_IS_FIXED_WIDTH(ft_face));
    LOG("CID: %ld", FT_IS_CID_KEYED(ft_face)); // CID(Character Identifier MyFont),
    LOG("bitmap: %ld", FT_HAS_FIXED_SIZES(ft_face));
    LOG("color: %ld", FT_HAS_COLOR(ft_face));
    LOG("multiple master: %ld", FT_HAS_MULTIPLE_MASTERS(ft_face));
    LOG("w:%ld h:%ld hbx:x %ld, hby:%ld, hd:%ld, vbx:%ld, vby:%ld, vd:%ld",
            ft_face->glyph->metrics.width, ft_face->glyph->metrics.height,
            ft_face->glyph->metrics.horiBearingX, ft_face->glyph->metrics.horiBearingY,
            ft_face->glyph->metrics.horiAdvance,
            ft_face->glyph->metrics.vertBearingX, ft_face->glyph->metrics.vertBearingY,
            ft_face->glyph->metrics.vertAdvance);
    LOG("max_advance_width; %d max_advance_height: %d",
        ft_face->max_advance_width, ft_face->max_advance_height);
    LOG("========================================================");
}

#if 0
// FIXME: make scaled font dump!!
static void
_dump_cairo_scaled_font(cairo_scaled_font_t *font)
{
    cairo_matrix_t ctm, scale_matirx;
    cairo_font_extents extents;

    cairo_scaled_font_extents(font, &extents);
    cairo_scaled_font_get_ctm(font, &ctm);
    cairo_scaled_font_get_scale_matrix(font, &scale_matrix);

}
#endif
#endif

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

bool
_font_init()
{
    if (_ft_lib) return true;
    if (FT_Init_FreeType(&_ft_lib)) return false;
    _font_config = FcInitLoadConfigAndFonts();
    if (!_font_config) {
        ERR("FcInitLoadConfigAndFonts failed");
        FT_Done_FreeType(_ft_lib);
        return false;
    }
    if (!FcConfigSetRescanInterval(_font_config, 0)) {
        ERR("FcConfigSetRescanInterval failed");
    }
    nemolist_init(&_font_list);

    return true;
}

static void
_font_destroy(MyFont *font)
{
    RET_IF(!font);
    if (font->cairo_font) cairo_scaled_font_destroy(font->cairo_font);
    if (font->hb_font) hb_font_destroy(font->hb_font);
    if (font->ft_face) FT_Done_Face(font->ft_face);
    free(font);
}

void
_font_shutdown()
{
    MyFont *temp;
    nemolist_for_each(temp, &_font_list, link) {
        _font_destroy(temp);
    }
    nemolist_empty(&_font_list);
    if (_font_config) FcFini();
    _font_config = NULL;
    if (_ft_lib) FT_Done_FreeType(_ft_lib);
    _ft_lib = NULL;
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
_font_hb_create(const char *file, unsigned int idx)
{
    File_Map *map = NULL;
    hb_blob_t *blob;
    hb_face_t *face;
    hb_font_t *font;
    unsigned int upem;

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

    // harfbuzz should scaled up at least upem
    hb_font_set_scale(font, upem, upem);

    hb_ft_font_set_funcs(font);
    // FIXME: glyph poses (offset, advance) is not correct for old version (below than 0.3.38)!!!
    //hb_ot_font_set_funcs(font);
    return font;
}

static cairo_scaled_font_t *
_font_cairo_create(FT_Face ft_face, double size)
{
    RET_IF(!ft_face, NULL);
    RET_IF(size <= 0, NULL);

    cairo_font_face_t *cairo_face;
    cairo_matrix_t ctm, font_matrix;
    cairo_font_options_t *font_options;
    cairo_scaled_font_t *scaled_font;

    cairo_face = cairo_ft_font_face_create_for_ft_face(ft_face, 0);

    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&font_matrix, size, size);

    font_options = cairo_font_options_create();
    cairo_font_options_set_hint_style(font_options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_hint_metrics(font_options, CAIRO_HINT_METRICS_OFF);
    cairo_font_options_set_antialias(font_options, CAIRO_ANTIALIAS_DEFAULT);
    // CAIRO_ANTIALIAS_DEFAULT, CAIRO_ANTIALIAS_NONE, CAIRO_ANTIALIAS_GRAY,CAIRO_ANTIALIAS_SUBPIXEL,         CAIRO_ANTIALIAS_FAST, CAIRO_ANTIALIAS_GOOD, CAIRO_ANTIALIAS_BEST

    if (cairo_font_options_get_antialias(font_options) == CAIRO_ANTIALIAS_SUBPIXEL) {
        cairo_font_options_set_subpixel_order(font_options, CAIRO_SUBPIXEL_ORDER_DEFAULT);
        /* cairo internal functions
        _cairo_font_options_set_lcd_filter(font_options, CAIRO_LCD_FILTER_DEFAULT);
        _cairo_font_options_set_round_glyph_positions(font_options, CAIRO_ROUND_GLYPH_POS_DEFAULT);
        */
    }

    scaled_font = cairo_scaled_font_create (cairo_face, &font_matrix, &ctm, font_options);
    if (CAIRO_STATUS_SUCCESS != cairo_scaled_font_status(scaled_font)) {
        ERR("cairo scaled font create");
        cairo_scaled_font_destroy(scaled_font);
        return NULL;
    }
    cairo_font_options_destroy(font_options);
    cairo_font_face_destroy(cairo_face);

    return scaled_font;
}

static MyFont *
_font_create(const char *filepath, unsigned int idx, const char *font_family, const char *font_style, unsigned int font_slant, unsigned int font_weight, unsigned int font_spacing, unsigned int font_width)

 {
    RET_IF(!filepath || !font_family || !font_style, NULL);
    RET_IF(!_file_exist(filepath), NULL);

    MyFont *font;
    FT_Face ft_face;
    hb_font_t *hb_font;
    cairo_scaled_font_t *cairo_font;

    ft_face = _font_ft_create(filepath, 0);
    if (!ft_face) return NULL;

    hb_font = _font_hb_create(filepath, idx);
    if (!hb_font) return NULL;

    // Usally, upem is 1000 for OpenType Shape font, 2048 for TrueType Shape font.
    // upem(Unit per em) is used for master (or Em) space.
    // cairo font size is noralized as 1
    cairo_font = _font_cairo_create(ft_face,
            ft_face->units_per_EM / (double)ft_face->max_advance_height);

    font = (MyFont *)calloc(sizeof(MyFont), 1);
    font->filepath = strdup(filepath);
    font->idx = idx;
    font->font_family = strdup(font_family);
    font->font_style = strdup(font_style);
    font->font_slant = font_slant;
    font->font_weight = font_weight;
    font->font_spacing = font_spacing;
    font->font_width = font_width;
    font->ft_face = ft_face;
    font->hb_font = hb_font;
    font->cairo_font = cairo_font;
    font->shapers = NULL;  //e.g. {"ot", "fallback", "graphite2", "coretext_aat"}

    return font;
}

// font_family: e.g. NULL, "LiberationMono", "Times New Roman", "Arial", etc.
// font_style: e.g. NULL, "Regular"(Normal), "Bold", "Italic", "Bold Italic", etc.
// font_slant: e.g. FC_SLANT_ROMAN, FC_SLANT_ITALIC, etc.
// font_weight: e.g. FC_WEIGHT_LIGHT, FC_WEIGHT_REGULAR, FC_WEIGHT_BOLD, etc.
// font_width e.g. FC_WIDTH_NORMAL, FC_WIDTH_CONDENSED, FC_WIDTH_EXPANDED, etc.
// font_spacing e.g. FC_PROPORTIONAL, FC_MONO, etc.
MyFont *
_font_load(const char *font_family, const char *font_style, int font_slant, int font_weight, int font_width, int font_spacing)
{
    MyFont *font = NULL, *temp;
    FcBool ret;
    FcPattern *pattern;
    FcFontSet *set;

    if (!_font_config) return NULL;

    // CACHE POP: Find from already inserted font list
    nemolist_for_each(temp, &_font_list, link) {
        if ((font_family && !strcmp(temp->font_family, font_family)) &&
            (font_style && !strcmp(temp->font_style, font_style)) &&
            (temp->font_slant == font_slant) &&
            (temp->font_weight == font_weight) &&
            (temp->font_width == font_width) &&
            (temp->font_spacing == font_spacing)) {
            return temp;
        }
    }
    // Create pattern
    pattern = FcPatternCreate();
    if (!pattern) {
        ERR("FcPatternBuild");
        return NULL;
    }
    if (font_family) FcPatternAddString(pattern, FC_FAMILY, (unsigned char *)font_family);
    if (font_style) FcPatternAddString(pattern, FC_STYLE, (unsigned char *)font_style);
    if (font_slant >= 0)  FcPatternAddInteger(pattern, FC_SLANT, font_slant);
    if (font_weight >= 0)  FcPatternAddInteger(pattern, FC_WEIGHT, font_weight);
    if (font_width >= 0)  FcPatternAddInteger(pattern, FC_WIDTH, font_width);
    if (font_spacing >= 0)  FcPatternAddInteger(pattern, FC_SPACING, font_spacing);

    FcConfigSubstitute(_font_config, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result;
    set = FcFontSetCreate();
    FcPattern *match;
    // Find just exactly matched one
    match = FcFontMatch(_font_config, pattern, &result);
    if (match) FcFontSetAdd(set, match);

    if (!match || !set->nfont) {  // Get fallback
        LOG("Search fallback fonts");
        FcFontSet *tset;
        tset = FcFontSort(_font_config, pattern, FcTrue, NULL, &result);
        if (!tset || !tset->nfont) {
            ERR("There is no installed fonts");
            FcFontSetDestroy(set);
            return NULL;
        }
        int i = 0;
        for (i = 0 ; i < tset->nfont ; i++) {
            FcPattern *temp = FcFontRenderPrepare(NULL, pattern, tset->fonts[i]);
            if (temp) FcFontSetAdd(set, temp);
        }
        FcFontSetDestroy(tset);
    }
    FcPatternDestroy(pattern);

    if (!set->nfont || !set->fonts[0]) {
        ERR("There is no installed fonts");
        FcFontSetDestroy(set);
        return NULL;
    }

    FcChar8 *filepath = NULL;
    ret = FcPatternGetString(set->fonts[0], FC_FILE, 0, &filepath);
    if (ret != FcResultMatch || !filepath) {
        ERR("file path is not correct");
        FcFontSetDestroy(set);
        return NULL;
    }

    // CACHE POP: Search already inserted list
    nemolist_for_each(temp, &_font_list, link) {
        if (!strcmp(temp->filepath, (char *)filepath)) {
            FcFontSetDestroy(set);
            return temp;
        }
    }

    FcChar8 *_family = NULL, *_style = NULL;
    int _slant, _weight, _spacing, _width;
    FcPatternGetString(set->fonts[0], FC_FAMILY, 0, &_family);
    FcPatternGetString(set->fonts[0], FC_STYLE, 0, &_style);
    FcPatternGetInteger(set->fonts[0], FC_SLANT, 0, &_slant);
    FcPatternGetInteger(set->fonts[0], FC_WEIGHT, 0, &_weight);
    FcPatternGetInteger(set->fonts[0], FC_SPACING, 0, &_spacing);
    FcPatternGetInteger(set->fonts[0], FC_WIDTH, 0, &_width);

    font = _font_create((char *)filepath, 0, (char *)_family, (char *)_style,
            _slant, _weight, _spacing, _width);
    // CACHE PUSH
    if (font) nemolist_insert_tail(&_font_list, &font->link);
    FcFontSetDestroy(set);

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
_text_cairo_create(hb_buffer_t *hb_buffer, const char* utf8, size_t utf8_len,
        int from, int to, bool is_cluster, double scale, bool vertical,
        int letter_space, int word_space)
{
    unsigned int num_glyphs;
    hb_glyph_info_t *hb_glyphs;
    hb_glyph_position_t *hb_glyph_poses;
    bool backward;

    cairo_glyph_t *glyphs;
    int num_clusters = 0;
    cairo_text_cluster_t *clusters = NULL;
    cairo_text_cluster_flags_t cluster_flags = 0;
    int i = 0;
    int j;

    RET_IF(!hb_buffer || !utf8, NULL);

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
    int _ws = 0;
    for (i = 0, j = from ; i < num_glyphs ; i++, j++) {
        glyphs[i].index = hb_glyphs[j].codepoint;
        glyphs[i].x =  (hb_glyph_poses[j].x_offset + x) * scale;
        glyphs[i].y = (-hb_glyph_poses[j].y_offset + y) * scale;
        if (vertical) {
            glyphs[i].y += i * letter_space;
            glyphs[i].y += _ws;
            if (1 == hb_glyphs[j].codepoint) _ws += word_space;
        } else {
            glyphs[i].x += i * letter_space;
            glyphs[i].x += _ws;
            if (1 == hb_glyphs[j].codepoint) _ws += word_space;
        }
        x +=  hb_glyph_poses[j].x_advance;
        y += -hb_glyph_poses[j].y_advance;
        //LOG("[%d] %d, %d", j, hb_glyph_poses[j].x_offset, hb_glyph_poses[j].x_advance);
        //LOG("[%d] %d, %d", j, hb_glyph_poses[j].y_offset, hb_glyph_poses[j].y_advance);
        //LOG("     %d, %d %lf", x, y, scale);
    }
    glyphs[i].index = -1;
    glyphs[i].x = x * scale;
    glyphs[i].y = y * scale;

    // FIXME: If multiline is used, do not use cluster
    // for some multibyte languages (korean, hebrew or japanese, etc.),
    // You should make a hb_buffer for each line.
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
            if (num_glyphs <= 1) num_glyphs = 2;
            for (i = num_glyphs - 2 ; i ; i--) {
                if (hb_glyphs[i].cluster != hb_glyphs[i+1].cluster) {
                    if (hb_glyphs[i].cluster <= hb_glyphs[i+1].cluster) {
                        ERR("cluster index is not correct");
                        cairo_glyph_free(glyphs);
                        cairo_text_cluster_free(clusters);
                        return NULL;
                    }
                    clusters[cluster].num_bytes = hb_glyphs[i].cluster - hb_glyphs[i+1].cluster;
                    end = start + clusters[cluster].num_bytes;
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
    ct->num_glyphs = num_glyphs;
    ct->glyphs = glyphs;
    ct->num_clusters = num_clusters;
    ct->clusters = clusters;
    ct->cluster_flags = cluster_flags;

    return ct;
}

// if return -1, no glyph can be exist within given size.
static int
_text_hb_get_idx_within(hb_buffer_t *buffer, bool vertical, int wrap,
        unsigned int start, double size, double *ret_size, double scale,
        int letter_space, int word_space)
{
    RET_IF(!buffer, -1);
    RET_IF(start < 0, -1);

    unsigned int num_glyphs;
    hb_glyph_position_t *glyph_poses;
    hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buffer, NULL);
    double sz = 0, prev_sz = 0;
    unsigned int i = 0;
    int last_space_idx = -1;

    glyph_poses = hb_buffer_get_glyph_positions(buffer, &num_glyphs);
    if (!glyph_poses) return 0;

    for (i = start; i < num_glyphs ; i++) {
        if (vertical) sz -= glyph_poses[i].y_advance * scale;
        else          sz += (glyph_poses[i].x_advance) * scale;
        sz += ((i - start) ? letter_space : 0);
        if (1 == info[i].codepoint) {   // if it's apce
            last_space_idx = i;
            sz += (i - start) ? word_space : 0;
        }
        if (sz >= size) {
            if ((wrap == 1) && (last_space_idx >= 0)) { // if word wrap, use space
                i = last_space_idx + 1;
            }
            break;
        }
        prev_sz = sz;
    }

    if (ret_size) *ret_size = prev_sz;
    return i - 1;
}

static hb_buffer_t *
_text_hb_create(hb_buffer_t *_hb_buffer, const char *utf8, unsigned int utf8_len,
        hb_direction_t hb_dir, hb_script_t hb_script, hb_language_t hb_lang,
        bool kerning, hb_font_t *hb_font)
{
    hb_buffer_t *hb_buffer;

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

    unsigned int num_glyphs = hb_buffer_get_length(hb_buffer);
    // Use unicode index instead of utf8 index
    hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(hb_buffer, NULL);
    unsigned int i =0;
    for (i = 0 ; i < num_glyphs ; i++) {
        infos->cluster = i;
        infos++;
    }

    hb_buffer_set_direction(hb_buffer, hb_dir);
    hb_buffer_set_script(hb_buffer, hb_script);
    hb_buffer_set_language(hb_buffer, hb_lang);
    hb_buffer_flags_t hb_flags = HB_BUFFER_FLAG_DEFAULT;
    //HB_BUFFER_FLAG_BOT | HB_BUFFER_FLAG_EOT | HB_BUFFER_FLAG_PRESERVE_DEFAULT_IGNORABLES;
    hb_buffer_set_flags(hb_buffer, hb_flags);

    hb_buffer_guess_segment_properties(hb_buffer);

    // Currently, I consider only one feature, kerning.
    hb_feature_t *hb_features = malloc(sizeof(hb_feature_t));
    if (!kerning) {  // Turn off kerning
        hb_feature_from_string("-kern", 5, &(hb_features[0]));
    } else {
        hb_feature_from_string("+kern", 5, &(hb_features[0]));
    }

    // Shape buffer data by using buffer, features, shapers, etc..
    // Each glyph's unicode is mapped into glyph's codepoint. (e.g. U+D55C => CE0)
    if (!hb_shape_full(hb_font, hb_buffer, hb_features, 1, NULL)) {
        hb_buffer_set_length(hb_buffer, 0);
        ERR("hb shape full failed");
        hb_buffer_destroy(hb_buffer);
        return NULL;
    }
    free(hb_features);

    // It can be used for cluster, seems to not be necessary for single cluster
    //hb_buffer_normalize_glyphs(hb_buffer);
    return hb_buffer;
}

void
_text_destroy(Text *t)
{
    RET_IF(!t);

    hb_buffer_destroy(t->hb_buffer);
    free(t->utf8);

    if (t->font_family) free(t->font_family);
    if (t->font_style) free(t->font_style);

    unsigned int i = 0;
    for (i = 0 ; i < t->line_num ; i++) {
        _text_cairo_destroy((t->cairo_texts)[i]);
    }
    free(t->cairo_texts);
    free(t);
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

void
_text_draw_cairo(cairo_t *cr, Text *t)
{
    if (!t->cairo_texts) return;

    cairo_save(cr);

    cairo_set_scaled_font(cr, t->font->cairo_font);
    cairo_set_font_size(cr,
            t->font_size * t->font->ft_face->units_per_EM /
            (double)t->font->ft_face->max_advance_height);

    cairo_font_extents_t font_extents;
    cairo_font_extents(cr, &font_extents);

    bool vertical = HB_DIRECTION_IS_VERTICAL(t->hb_dir);

    if (vertical) {
        double descent = font_extents.height * (t->line_num + .5) +
            ((t->line_num -1 ) * t->line_space);
        double anchor = -t->anchor * t->height;
        cairo_translate (cr, descent, anchor);
    } else {
        double descent = font_extents.descent;
        double anchor = -t->anchor * t->width;
        cairo_translate(cr, anchor, -descent);
    }

    ERR("%d\n", t->font_size);
    unsigned int i = 0;
    for (i = 0 ; i < t->line_num ; i++) {
        Cairo_Text *ct = (t->cairo_texts)[i];

        if (vertical) {
            if (i)
                cairo_translate (cr, -t->line_space, 0);
            cairo_translate (cr, -font_extents.height, 0);
        }
        else {
            if (i)
                cairo_translate (cr, 0, t->line_space);
            cairo_translate (cr, 0, font_extents.height);
        }

#if 0
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
#endif
        /* Should be image surface*/
        if (t->fill_a > 0) {
            cairo_glyph_path (cr, ct->glyphs, ct->num_glyphs);
            cairo_set_source_rgba (cr,
                    t->fill_r/255., t->fill_g/255.,
                    t->fill_b/255., t->fill_a/255.);
            cairo_fill (cr);
        }
        if (t->stroke_a > 0) {
            cairo_glyph_path (cr, ct->glyphs, ct->num_glyphs);
            cairo_set_source_rgba (cr,
                    t->stroke_r/255., t->stroke_g/255.,
                    t->stroke_b/255., t->stroke_a/255.);
            cairo_set_line_width (cr, t->stroke_width);
            cairo_stroke(cr);
        }

        if (t->decoration) {
            cairo_save(cr);
            if (t->decoration == 1) {   // underline
                if (vertical) {
                    cairo_move_to(cr, -font_extents.height * 0.5, 0);
                    cairo_line_to(cr, -font_extents.height * 0.5, ct->height);
                } else {
                    cairo_move_to(cr, 0, 0);
                    cairo_line_to(cr, ct->width, 0);
                }
            } else if (t->decoration == 2) {    // overline
                if (vertical) {
                    cairo_move_to(cr, font_extents.height * 0.5, 0);
                    cairo_line_to(cr, font_extents.height * 0.5, ct->height);
                } else {
                    cairo_move_to(cr, 0, -font_extents.ascent);
                    cairo_line_to(cr, ct->width, -font_extents.ascent);
                }
            } else if (t->decoration == 3) {    // line through
                if (vertical) {
                    cairo_move_to(cr, 0, 0);
                    cairo_line_to(cr, 0, ct->height);
                } else {
                    cairo_move_to(cr, 0, -font_extents.descent);
                    cairo_line_to(cr, ct->width, -font_extents.descent);
                }
            }
            cairo_stroke(cr);
            cairo_restore(cr);
        }

#if 0
        if (0) { // char string
            if (0) {  // Draw text
                cairo_show_text(cr, t->utf8);
            }  else {  // Draw path
                cairo_move_to(cr, 0, 0);
                cairo_rotate(cr, 45);
                cairo_text_path(cr, t->utf8);
                cairo_fill (cr);
            }
        } else { // Glyph
            if (ct->num_clusters) {
                cairo_show_text_glyphs (cr,
                        NULL, 0,
                        ct->glyphs, ct->num_glyphs,
                        ct->clusters, ct->num_clusters,
                        ct->cluster_flags);
            } else  {
                /*  doesn't support subpixel positioning */
                cairo_show_glyphs (cr, ct->glyphs, ct->num_glyphs);
            }
        }
#endif
    }

    cairo_restore (cr);
}

Text *
_text_create(const char *utf8)
{
    RET_IF(!utf8, NULL);

    unsigned int utf8_len;
    utf8_len = strlen(utf8);
    if (!utf8_len) {
        ERR("utfh length is 0");
        return NULL;
    }

    char *str;
    unsigned int str_len = 0;
    str = (char *)malloc(sizeof(char) * (utf8_len + 1));
    // Remove special characters (e.g. line feed, backspace, etc.)
    unsigned int i = 0;
    for (i = 0 ; i < utf8_len ; i++) {
        if ((utf8[i] >> 31) ||
            (utf8[i] > 0x1F)) { // If it is valid character
            str[str_len] = utf8[i];
            str_len++;
        }
    }
    if (!str || (str_len <= 0)) {  // Add just one line
        Text *t = calloc(sizeof(Text), 1);
        t->line_num = 1;
        t->dirty = true;
        return t;
    }
    str = (char *)realloc(str, sizeof(char) * (str_len + 1));
    str[str_len] = '\0';

    Text *t = calloc(sizeof(Text), 1);
    t->utf8 = str;
    t->utf8_len = str_len;
    t->hb_dir = HB_DIRECTION_INVALID;
    t->hb_script = HB_SCRIPT_INVALID;
    t->hb_lang = HB_LANGUAGE_INVALID;
    t->font_size = 12;  // default font size
    t->font_slant = -1;
    t->font_weight = -1;
    t->font_width = -1;
    t->font_spacing = -1;
    t->stroke_width = 2;
    t->fill_a = 255;    // default is fill
    t->dirty = true;

    return t;
}

void
_text_draw(Text *t, cairo_t *cr)
{
    RET_IF(!t);

    if (!t->dirty) goto _text_draw_again;
    t->dirty = false;

    if (t->cairo_texts) {
        unsigned int i = 0;
        for (i = 0 ; i < t->line_num ; i++) {
            _text_cairo_destroy(t->cairo_texts[i]);
        }
        t->cairo_texts = NULL;
    }
    if (!t->utf8 && !t->utf8_len) {
        if (t->line_num) {
            // It's just line, user should tranlate it
            t->width = t->font_size;
            t->height = t->font_size;
        } else ERR("it's NULL string");
        return;
    }

    // font size adjustment
    if (t->hint_height && (t->font_size > t->hint_height)) t->font_size = t->hint_height;
    if (t->hint_width && (t->font_size > t->hint_width))   t->font_size = t->hint_width;

    unsigned int num_glyphs = 0;
    t->font = _font_load(t->font_family, t->font_style, t->font_slant,
            t->font_weight, t->font_width, t->font_spacing);

    // harfbuzz was scaled up as upem, scaled it down as font pixel size.
    t->cairo_scale = t->font_size / (double)t->font->ft_face->max_advance_height;

    // FIXME: Use cache for hb_buffer if possible
    t->hb_buffer = _text_hb_create(t->hb_buffer, t->utf8, t->utf8_len,
            t->hb_dir, t->hb_script, t->hb_lang, t->kerning, t->font->hb_font);
    num_glyphs = hb_buffer_get_length(t->hb_buffer);

    double maxw, maxh;
    bool vertical = HB_DIRECTION_IS_VERTICAL(t->hb_dir);
    if (vertical) {
        if (t->hint_height) maxw = t->hint_height;
        else maxw = 1410065407;
        if (t->hint_width) maxh = t->hint_width;
        else maxh = 1410065407;
    } else {
        if (t->hint_width) maxw = t->hint_width;
        else maxw = 1410065407;
        if (t->hint_height) maxh = t->hint_height;
        else maxh = 1410065407;
    }

    int line_num = 1;
    double w = 0, h = 0;
    int from = 0, to = 0;

    while (1) {
        double size;
        to = _text_hb_get_idx_within(t->hb_buffer, vertical, t->wrap,
                from, maxw, &size, t->cairo_scale, t->letter_space, t->word_space);
        t->cairo_texts = realloc(t->cairo_texts, sizeof(Cairo_Text *) * line_num);
        t->cairo_texts[line_num-1] = _text_cairo_create(t->hb_buffer,
                t->utf8, t->utf8_len, from, to, true, t->cairo_scale,
                vertical, t->letter_space, t->word_space);
        h = (line_num * t->font_size) +
            ((line_num -1 ) *t->line_space);
        if (vertical) {
            t->cairo_texts[line_num-1]->width = h;
            t->cairo_texts[line_num-1]->height = size;
        } else {
            t->cairo_texts[line_num-1]->width = size;
            t->cairo_texts[line_num-1]->height = h;
        }
        if (size > w) {
            w = size;
        }

        if (to >= (num_glyphs - 1)) {
            //LOG("end of glyph");
            break;
        }

        if (h > maxh || EQUAL(h, maxh)) { // double comparison
            //LOG("exceed height");
            if (t->auto_resize && !t->ellipsis) {
                t->font_size -= 1; // FIXME performance issue!! use binary search
                t->cairo_scale = t->font_size / (double)t->font->ft_face->max_advance_height;
                continue;
            }
            break;
        }
        if (!t->wrap) {
            //LOG("No wrap");
            if (t->auto_resize && !t->ellipsis) {
                t->font_size -= 1; // FIXME performance issue!! use binary search
                t->cairo_scale = t->font_size / (double)t->font->ft_face->max_advance_height;
                continue;
            }
            w = size;
            h = line_num * t->font_size;
            break;
        }
        from = to + 1;
        line_num++;
    }
    if (t->ellipsis && (to < (num_glyphs -1))) {
        // FIXME: ellipsis is too long than last glyph width!!!!
        _str_ellipsis_append(&(t->utf8), &(t->utf8_len), to);
        // shaping again with ellipsis
        t->hb_buffer = _text_hb_create(t->hb_buffer, t->utf8, t->utf8_len,
                t->hb_dir, t->hb_script, t->hb_lang, t->kerning, t->font->hb_font);
        num_glyphs = hb_buffer_get_length(t->hb_buffer);
        _text_cairo_destroy(t->cairo_texts[line_num-1]);
        t->cairo_texts[line_num - 1] = _text_cairo_create(t->hb_buffer,
                t->utf8, t->utf8_len, from, to, true, t->cairo_scale,
                vertical, t->letter_space, t->word_space);
    }
    if (vertical) {
        t->width = h;
        t->height = w;
    } else {
        t->width = w;
        t->height = h;
    }
    t->line_num = line_num;

_text_draw_again:
    _text_draw_cairo(cr, t);
}

static void
_text_dirty(Text *t)
{
    RET_IF(!t);
    t->dirty = true;
}

// e.g. "LiberationMono", "Times New Roman", "Arial", etc.
bool
_text_set_font_family(Text *t, const char *font_family)
{
    RET_IF(!t, false);
    if ((t->font_family && font_family &&
         !strcmp(t->font_family, font_family)) ||
        (!t->font_family && !font_family))
        return true;
    _text_dirty(t);
    if (t->font_family) free(t->font_family);

    if (font_family) t->font_family = strdup(font_family);
    else        t->font_family = NULL;
    return true;
}

const char *
_text_get_font_family(Text *t)
{
    RET_IF(!t, NULL);
    return t->font_family;
}

// e.g. "Regular"(or "Normal"), "Bold", "Italic", "Bold Italic", etc.
bool
_text_set_font_style(Text *t, const char *font_style)
{
    RET_IF(!t, false);
    if ((t->font_style && font_style &&
         !strcmp(t->font_style, font_style)) ||
        (!t->font_style && !font_style))
        return true;
    _text_dirty(t);
    if (t->font_style) free(t->font_style);

    if (font_style) t->font_style = strdup(font_style);
    else       t->font_style = NULL;
    return true;
}

const char *
_text_get_font_style(Text *t)
{
    RET_IF(!t, NULL);
    return t->font_style;
}

// -1 is unset
bool
_text_set_font_size(Text *t, int font_size)
{
    RET_IF(!t, false);
    if (t->font_size == font_size) return true;
    _text_dirty(t);
    t->font_size = font_size;
    return true;
}

unsigned int
_text_get_font_size(Text *t)
{
    RET_IF(!t, 0);
    return t->font_size;
}

// font_slant: e.g. FC_SLANT_ROMAN, FC_SLANT_ITALIC, etc.
// -1 is unset
bool
_text_set_font_slant(Text *t, int font_slant)
{
    RET_IF(!t, false);
    if (t->font_slant == font_slant) return true;
    _text_dirty(t);
    t->font_slant = font_slant;
    return true;
}

int
_text_get_font_slant(Text *t)
{
    RET_IF(!t, -1);
    return t->font_slant;
}

// font_weight: e.g. FC_WEIGHT_LIGHT, FC_WEIGHT_REGULAR, FC_WEIGHT_BOLD, etc.
// -1 is unset
bool
_text_set_font_weight(Text *t, int font_weight)
{
    RET_IF(!t, false);
    if (t->font_weight == font_weight) return true;
    _text_dirty(t);
    t->font_weight = font_weight;
    return true;
}

int
_text_get_font_weight(Text *t)
{
    RET_IF(!t, 0);
    return t->font_weight;
}

// width e.g. FC_WIDTH_NORMAL, FC_WIDTH_CONDENSED, FC_WIDTH_EXPANDED, etc.
// -1 is unset
bool
_text_set_font_width(Text *t, int font_width)
{
    RET_IF(!t, false);
    if (t->font_width == font_width) return true;
    _text_dirty(t);
    t->font_width = font_width;
    return true;
}

unsigned int
_text_get_font_width(Text *t)
{
    RET_IF(!t, -1);
    return t->font_width;
}

// spacing e.g. FC_PROPORTIONAL, FC_MONO, etc.
// -1 is unset
bool
_text_set_font_spacing(Text *t, int font_spacing)
{
    RET_IF(!t, false);
    if (t->font_spacing == font_spacing) return true;
    _text_dirty(t);
    t->font_spacing = font_spacing;
    return true;
}

int
_text_get_font_spacing(Text *t)
{
    RET_IF(!t, -1);
    return t->font_spacing;
}

bool
_text_set_direction(Text *t, bool vertical, bool backward)
{
    RET_IF(!t, false);
    if (HB_DIRECTION_IS_VERTICAL(t->hb_dir) == vertical &&
        HB_DIRECTION_IS_VERTICAL(t->hb_dir) == backward)
        return true;
    _text_dirty(t);
    if (vertical) {
        if (backward)   t->hb_dir = HB_DIRECTION_BTT;
        else            t->hb_dir = HB_DIRECTION_TTB;
    } else {
        if (backward)   t->hb_dir = HB_DIRECTION_RTL;
        else            t->hb_dir = HB_DIRECTION_LTR;
    }
    return true;
}

void
_text_get_direction(Text *t, bool *vertical, bool *backward)
{
    RET_IF(!t);
    if (t->hb_dir == HB_DIRECTION_BTT) {
        if (vertical) *vertical = true;
        if (backward) *backward = true;
    } else if (t->hb_dir == HB_DIRECTION_TTB) {
        if (vertical) *vertical = true;
        if (backward) *backward = false;
    } else if (t->hb_dir == HB_DIRECTION_RTL) {
        if (vertical) *vertical = false;
        if (backward) *backward = true;
    } else {
        if (vertical) *vertical = false;
        if (backward) *backward = false;
    }
}

// ISO=15024 tag
// "latn" (LATIN), "hang" (HANGUL), "Hira" (HIRAGANA), "Kana" (KATAKANA)
// "Deva" (DEVANAGARI (Hindi language)), "Arab" (ARABIC), "Hebr" (HEBREW), etc.
bool
_text_set_script(Text *t, const char *script)
{
    RET_IF(!t, false);
    hb_script_t hb_script = hb_script_from_string(script, -1);
    if (t->hb_script == hb_script) return true;
    _text_dirty(t);
    if (script) t->hb_script = hb_script;
    else        t->hb_script = HB_SCRIPT_INVALID;
    return true;
}

char *
_text_get_script(Text *t)
{
    RET_IF(!t, NULL);
    return _strdup_printf("%c%c%c%c", t->hb_script >> 24, t->hb_script >> 16,
            t->hb_script >> 8, t->hb_script);
}

// "C", "en", "ko", etc.
bool
_text_set_lang(Text *t, const char *lang)
{
    RET_IF(!t, false);
    hb_language_t hb_lang = hb_language_from_string(lang, -1);
    if (t->hb_lang == hb_lang) return true;
    _text_dirty(t);
    //lang = getenv("LANG");
    if (lang) t->hb_lang = hb_lang;
    else      t->hb_lang = HB_LANGUAGE_INVALID;
    return true;
}

const char *
_text_get_lang(Text *t)
{
    RET_IF(!t, NULL);
    return hb_language_to_string(t->hb_lang);
}

// CSS  font-feature-setting (except 'normal', 'inherited')
// Default Auto: When the font size is big, font kerning may look strange and it will be disabled.
// --> plz, check harfubz really do that.
bool
_text_set_kerning(Text *t, bool kerning)
{
    RET_IF(!t, false);
    if (t->kerning == !!kerning) return true;
    _text_dirty(t);
    t->kerning = !!kerning;
    return true;

}

bool
_text_get_kerning(Text *t)
{
    RET_IF(!t, false);
    return t->kerning;
}

// 0: start, 0.5: middle, 1.0: end
bool
_text_set_anchor(Text *t, double anchor)
{
    RET_IF(!t, false);
    if (t->anchor == anchor) return true;
    _text_dirty(t);
    if (anchor < 0.) anchor = 0.;
    if (anchor > 1.) anchor = 1.;
    t->anchor = anchor;

    return true;
}

double
_text_get_anchor(Text *t)
{
    RET_IF(!t, 0.);
    return t->anchor;
}

// If alpha is 0, unfill
bool
_text_set_fill_color(Text *t, unsigned int r, unsigned int g, unsigned int b, unsigned int a)
{
    RET_IF(!t, false);
    if ((t->fill_r == r) && (t->fill_g == g) &&
        (t->fill_b == b) && (t->fill_a == a)) return true;
    _text_dirty(t);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    if (a > 255) a = 255;
    t->fill_r = r;
    t->fill_g = g;
    t->fill_b = b;
    t->fill_a = a;

    return true;
}

void
_text_get_fill_color(Text *t, unsigned int *r, unsigned int *g, unsigned int *b, unsigned int *a)
{
    RET_IF(!t);
    if (r) *r = t->fill_r;
    if (g) *g = t->fill_g;
    if (b) *b = t->fill_b;
    if (a) *a = t->fill_a;
}

// If alpha is 0, unset storke
bool
_text_set_stroke_color(Text *t, unsigned int r, unsigned int g, unsigned int b, unsigned int a)
{
    RET_IF(!t, false);
    if ((t->stroke_r == r) && (t->stroke_g == g) &&
        (t->stroke_b == b) && (t->stroke_a == a)) return true;
    _text_dirty(t);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    if (a > 255) a = 255;
    t->stroke_r = r;
    t->stroke_g = g;
    t->stroke_b = b;
    t->stroke_a = a;

    return true;
}

void
_text_get_stroke_color(Text *t, unsigned int *r, unsigned int *g, unsigned int *b, unsigned int *a)
{
    RET_IF(!t);
    if (r) *r = t->stroke_r;
    if (g) *g = t->stroke_g;
    if (b) *b = t->stroke_b;
    if (a) *a = t->stroke_a;
}

bool
_text_set_stroke_width(Text *t, double w)
{
    RET_IF(!t, false);
    if (t->stroke_width == w) return true;
    _text_dirty(t);
    if (w < 0.) w = 0.;
    t->stroke_width = w;
    return true;
}

double
_text_get_stroke_width(Text *t)
{
    RET_IF(!t, 0.);
    return t->stroke_width;
}

bool
_text_set_letter_space(Text *t, int space)
{
    RET_IF(!t, false);
    if (t->letter_space == space) return true;
    _text_dirty(t);
    t->letter_space = space;
    return true;
}

int
_text_get_letter_space(Text *t)
{
    RET_IF(!t, 0);
    return t->letter_space;
}

bool
_text_set_word_space(Text *t, int space)
{
    RET_IF(!t, false);
    if (t->word_space == space) return true;
    _text_dirty(t);
    t->word_space = space;
    return true;
}

int
_text_get_word_space(Text *t)
{
    RET_IF(!t, 0);
    return t->word_space;
}

// None: 0, Underline:1, Overline: 2, Line-through: 3
bool
_text_set_decoration(Text *t, unsigned int decoration)
{
    RET_IF(!t, false);
    if (t->decoration == decoration) return true;
    _text_dirty(t);
    if (decoration > 3) decoration = 3;
    t->decoration = decoration;
    return true;
}

unsigned int
_text_get_decoration(Text *t)
{
    RET_IF(!t, 0);
    return t->decoration;
}

bool
_text_set_ellipsis(Text *t, bool ellipsis)
{
    RET_IF(!t, false);
    if (t->ellipsis == !!ellipsis) return true;
    _text_dirty(t);
    t->ellipsis = !!ellipsis;
    return true;
}

bool
_text_get_ellipsis(Text *t)
{
    RET_IF(!t, false);
    return t->ellipsis;
}

// 0: None, 1: auto (wrap prefer), 2: char warp
bool
_text_set_wrap(Text *t, int wrap)
{
    RET_IF(!t, false);
    if (t->wrap == wrap) return true;
    _text_dirty(t);
    t->wrap = wrap;
    return true;
}

int
_text_get_wrap(Text *t)
{
    RET_IF(!t, 0);
    return t->wrap;
}

bool
_text_set_hint_width(Text *t, double width)
{
    RET_IF(!t, false);
    if (t->hint_width == width) return true;
    _text_dirty(t);
    if (width < 0) width = 0;
    t->hint_width = width;
    return true;
}

double
_text_get_hint_width(Text *t)
{
    RET_IF(!t, 0);
    return t->hint_width;
}


bool
_text_set_hint_height(Text *t, double height)
{
    RET_IF(!t, false);
    if (t->hint_height == height) return true;
    _text_dirty(t);
    if (height < 0) height = 0;
    t->hint_height = height;
    return true;
}

double
_text_get_hint_height(Text *t)
{
    RET_IF(!t, 0);
    return t->hint_height;
}

double
_text_get_width(Text *t)
{
    RET_IF(!t, 0);
    return t->width;
}

double
_text_get_height(Text *t)
{
    RET_IF(!t, 0);
    return t->height;
}

unsigned int
_text_get_line_num(Text *t)
{
    RET_IF(!t, 0);
    return t->line_num;
}

bool
_text_set_line_space(Text *t, double line_space)
{
    RET_IF(!t, false);
    if (t->line_space == line_space) return true;
    _text_dirty(t);
    t->line_space = line_space;
    return true;
}

double
_text_get_line_space(Text *t)
{
    RET_IF(!t, 0);
    return t->line_space;
}

bool
_text_set_font_auto_resize(Text *t, bool auto_resize)
{
    RET_IF(!t, false);
    if (t->auto_resize == !!auto_resize) return true;
    _text_dirty(t);
    t->auto_resize = !!auto_resize;
    return true;
}

bool
_text_get_font_auto_resize(Text *t)
{
    RET_IF(!t, false);
    return t->auto_resize;
}

#if 0
#include <iconv.h>

static Text *
_create_text2(const char *utf8, const char *dir, const char *script, const char *lang,
            const char *features, MyFont *font)
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
