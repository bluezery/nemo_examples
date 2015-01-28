#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <math.h>

#include <cairo.h>
#include <cairo-ft.h>
#include <cairo-svg.h>
#include <cairo-pdf.h>
#include <cairo-ps.h>

#include <hb-ft.h>
#include <hb-ot.h>

#include <Ecore.h>
#include <Evas.h>
#include <Ecore_Evas.h>

#include <search.h>
#include <iconv.h>

#ifdef __GNUC__
#define __UNUSED__ __attribute__((__unused__))
#else
#define __UNUSED__
#endif

#define LOG_COLOR_BLUE "\x1b[34m"
#define LOG_COLOR_RED "\x1b[31m"
#define LOG_COLOR_GREEN "\x1b[32m"
#define LOG_COLOR_END "\x1b[0m"
#define LOGFMT(COLOR, fmt, ...) do { fprintf(stderr, "[%s:%d][%s] " fmt "\n", __FILE__,  __LINE__, __func__, ##__VA_ARGS__); } while (0);
#define ERR(...) LOGFMT(LOG_COLOR_RED, ##__VA_ARGS__)
#define LOG(...) LOGFMT(LOG_COLOR_GREEN, ##__VA_ARGS__)

#define RET_IF(VAL, ...) do { if (VAL) { ERR(#VAL " is false\n"); return __VA_ARGS__; } } while (0);


#define FREETYPE 0 // Use freetype backend instead of opentype

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
    for(int i = 0 ; i < text->num_glyphs ; i++) {
        LOG("[%3d]: index:%6lX, x:%3.3lf y:%3.3lf", i,
            (text->glyphs)[i].index, (text->glyphs)[i].x, (text->glyphs)[i].y);
    }
    LOG("num clusters:[%3d] cluster flags[%3d]", text->num_clusters, text->cluster_flags);
    for(int i = 0 ; i < text->num_clusters ; i++) {
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
    LOG("========================== Font info ===============================");
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

    for (unsigned int i = 0 ; i < num_glyphs ; i++) {
        LOG("==================== [%d] Glyph info =========================", i);
        _dump_hb_glyph(infos[i], poses[i], dir, type, font);
    }
}

static void
_dump_cairo_glyph(cairo_glyph_t *glyph, unsigned int num_glyph)
{
    LOG("========================== cairo glyphs info ===============================");
    for (unsigned int i = 0 ; i < num_glyph ; i++) {
        LOG("[%2d] index: %6lX, x:%7.2lf y:%7.2lf",
            i, glyph[i].index, glyph[i].x, glyph[i].y);
    }
}

static void
_dump_cairo_text_cluster(cairo_text_cluster_t *cluster, unsigned int num_cluster)
{
    LOG("========================== cairo clusters info ===============================");
    for (unsigned int i = 0 ; i < num_cluster ; i++) {
        LOG("[%2d] num_bytes: %d, num_glyphs: %d",
            i, cluster[i].num_bytes, cluster[i].num_glyphs);
    }
}

static void
_dump_ft_face(FT_Face ft_face)
{
    LOG("===================== Font Face Info ==================");
    LOG("has vertical: %ld", FT_HAS_VERTICAL(ft_face));
    LOG("has horizontal: %ld", FT_HAS_HORIZONTAL(ft_face));
    LOG("has kerning: %ld", FT_HAS_KERNING(ft_face));
    LOG("sfnt: %ld", FT_IS_SFNT(ft_face));
    LOG("trick: %ld", FT_IS_TRICKY(ft_face));
    LOG("scalable: %ld", FT_IS_SCALABLE(ft_face));
    LOG("fixed_width: %ld", FT_IS_FIXED_WIDTH(ft_face));
    LOG("CID: %ld", FT_IS_CID_KEYED(ft_face)); // CID(Character Identifier Font),
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
_file_map_del(File_Map *fmap)
{
    RET_IF(!fmap);
    if (munmap(fmap->data, fmap->len) < 0)
        ERR("munmap failed");
}

static File_Map *
_file_map_new(const char *file)
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

FT_Library _ft_lib;

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
_font_del(Font *font)
{
    RET_IF(!font);
    if (font->hb_font) hb_font_destroy(font->hb_font);
    if (font->ft_face) FT_Done_Face(font->ft_face);
    if (font->cairo_font) cairo_scaled_font_destroy(font->cairo_font);
    free(font);
}
static FT_Face
_font_ft_new(const char *file, unsigned int idx)
{
    FT_Face ft_face;

    if (FT_New_Face(_ft_lib, file, idx, &ft_face)) return NULL;

    return ft_face;
}

// if backend is 1, it's freetype, else opentype
static hb_font_t *
_font_hb_new(const char *file, unsigned int idx, int backend)
{
    File_Map *map = NULL;
    hb_blob_t *blob;
    hb_face_t *face;
    hb_font_t *font;
    double upem;

    RET_IF(!file, NULL);

    map = _file_map_new(file);
    if (!map->data || !map->len) return NULL;

    blob = hb_blob_create(map->data, map->len,
            HB_MEMORY_MODE_READONLY_MAY_MAKE_WRITABLE, map,
            (hb_destroy_func_t)_file_map_del);
    if (!blob) {
        _file_map_del(map);
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
_font_cairo_scaled_new(const char *file, double height, unsigned int upem)
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

    ft_face = _font_ft_new(file, 0);
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
_font_new(const char *file, unsigned int idx, double size)
{
    RET_IF(!file, NULL);

    Font *font;

    hb_font_t *hb_font;

    FT_Face ft_face;

    cairo_scaled_font_t *cairo_font;

    unsigned int upem;

    hb_font = _font_hb_new(file, 0, 0);
    if (!hb_font) return NULL;

    // Usally, upem is 1000 for OpenType Shape font, 2048 for TrueType Shape font.
    // upem(Unit per em) is used for master (or Em) space.
    upem = hb_face_get_upem(hb_font_get_face(hb_font));

    // Create freetype !!
    ft_face = _font_ft_new(file, 0);
    if (!ft_face) return NULL;

    cairo_font = _font_cairo_scaled_new(file, size, upem);
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
_text_cairo_del(Cairo_Text *ct)
{
    RET_IF(!ct);
    cairo_glyph_free(ct->glyphs);
    cairo_text_cluster_free(ct->clusters);
    cairo_scaled_font_destroy(ct->font);
}

// if from or to is -1, the ignore range.
static Cairo_Text *
_text_cairo_new(Font *font, hb_buffer_t *hb_buffer, const char* utf8, size_t utf8_len,
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
_text_hb_new(hb_buffer_t *_hb_buffer, const char *utf8, unsigned int utf8_len,
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
_text_del(Text *text)
{
    RET_IF(!text);
    free(text->utf8);
    for (unsigned int i = 0 ; i < text->line_num ; i++) {
        _text_cairo_del((text->cairo_texts)[i]);
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
_text_new(Font *font, const char *utf8,
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

    hb_buffer = _text_hb_new(NULL, str, str_len, dir, script, lang, features, font->shapers, font->hb_font);
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
        hb_buffer = _text_hb_new(hb_buffer, str, str_len,
                dir, script, lang, features, font->shapers, font->hb_font);
        num_glyphs = hb_buffer_get_length(hb_buffer);
    }

    cairo_texts = (Cairo_Text **)malloc(sizeof(Cairo_Text *) * line_num);

    if ((line_num == 1) && (i <= num_glyphs)) {// if exact single line
        // FIXME: This should be here because  _text_hb_get_idx_within_width()
        // function cannot solve double precison for width.
        cairo_texts[0] = _text_cairo_new(font, hb_buffer, str, str_len, 0, i, true);
    } else {
        unsigned int from = 0, to;
        for (unsigned int i = 0 ; i < line_num ; i++) {
            to = _text_hb_get_idx_within_width(hb_buffer, from, width, font->cairo_scale);
            cairo_texts[i] = _text_cairo_new(font, hb_buffer, str, str_len, from, to, false);
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
static void
_text_cairo_draws(cairo_t *cr, Font *font, int line_len, Text *texts[],
        double line_space, double margin_left, double margin_top)

{
    cairo_save(cr);

    cairo_set_scaled_font(cr, font->cairo_font);

    cairo_translate(cr, margin_left, margin_top);

    cairo_font_extents_t font_extents;
    cairo_font_extents(cr, &font_extents);

    double descent;

    /*
    if (texts[i]->vertical) {
        descent = font_extents.height * (line_len + .5);
        cairo_translate (cr, descent, 0);
    } else*/ {
        descent = font_extents.descent;
        cairo_translate (cr, 0, -descent);
    }

    for (unsigned int i = 0 ; i < line_len ; i++) {
        Text *tt = texts[i];

        Cairo_Text *ct = tt->cairo_texts;

        if (texts[i]->vertical) {
            if (i) cairo_translate (cr, -line_space, 0);
            cairo_translate (cr, -font_extents.height, 0);
        }
        else {
            if (i) cairo_translate (cr, 0, line_space);
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
#if 1
        if (0 && cairo_surface_get_type (cairo_get_target (cr)) == CAIRO_SURFACE_TYPE_IMAGE) {
            /* cairo_show_glyphs() doesn't support subpixel positioning */
            cairo_glyph_path (cr, ct->glyphs, ct->num_glyphs);
            cairo_fill (cr);
        } else if (ct->num_clusters) {
            cairo_show_text_glyphs (cr,
                    tt->utf8, tt->utf8_len,
                    ct->glyphs, ct->num_glyphs,
                    ct->clusters, ct->num_clusters,
                    ct->cluster_flags);
        } else {
            cairo_show_glyphs (cr, ct->glyphs, ct->num_glyphs);
        }
#endif
    }

    cairo_restore (cr);
}
#endif

static cairo_status_t
stdio_write_func (void *closure,
        const unsigned char *data,
        unsigned int size)
{
    FILE *fp = stdout;
    while (size) {
        size_t ret = fwrite (data, 1, size, fp);
        size -= ret;
        data += ret;
        if (size && ferror (fp)) {
            ERR("Failed to write output: %s", strerror (errno));
            return CAIRO_STATUS_WRITE_ERROR;
        }
    }

    return CAIRO_STATUS_SUCCESS;
}

static
void write_png_stream(cairo_t *cr, int w __UNUSED__, int h __UNUSED__)
{
    cairo_status_t status;

    cairo_surface_t *surface = cairo_get_target(cr);
    if (!surface) ERR("cairo get target failed");

    status = cairo_surface_write_to_png_stream(surface,
            stdio_write_func,
            NULL);
    if (status != CAIRO_STATUS_SUCCESS)
        ERR("error:%s", cairo_status_to_string(status));
}

static
void render_evas(cairo_t *cr, int w, int h)
{
    Ecore_Evas *ee;
    Evas_Object *img;

    RET_IF(!ecore_evas_init())

    cairo_surface_t *surface = cairo_get_target(cr);
    unsigned char *data = cairo_image_surface_get_data(surface);

    ee = ecore_evas_new(NULL, 0, 0, w, h, NULL);
    ecore_evas_show(ee);

    img = evas_object_image_filled_add(ecore_evas_get(ee));
#if 0
    evas_object_image_file_set(img, "/home/bluezery/Downloads/test.jpg", NULL);
    evas_object_image_preload(img, EINA_TRUE);
    Evas_Load_Error err = evas_object_image_load_error_get(img);
    printf("err:%d\n", err);
#endif
    evas_object_image_colorspace_set(img, EVAS_COLORSPACE_ARGB8888);
    evas_object_image_size_set(img, w, h);
    evas_object_image_data_set(img, data);
    evas_object_resize(img, w, h);
    evas_object_show(img);

    ecore_main_loop_begin();

    evas_object_del(img);
    ecore_evas_free(ee);
    ecore_evas_shutdown();
}


typedef void (*render_loop)(cairo_t *cr, int w_in_pt, int h_in_pt);

static cairo_surface_t *
_cairo_img_surface_create(render_loop render_func,
        void *key,
        int w, int h)
{
    cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
    cairo_status_t status = cairo_surface_status(surface);
    if (status != CAIRO_STATUS_SUCCESS) ERR("error:%s", cairo_status_to_string(status));

    if (cairo_surface_set_user_data(surface,
                key,
                render_func,
                NULL))
        ERR("set user data failure");
    return surface;
}

static cairo_surface_t *
_create_cairo_surface(int type, int w, int h, void *closure_key)
{
    cairo_surface_t *surface;

    switch(type)
    {
        default:
        case 0:
            surface = _cairo_img_surface_create(render_evas,
                    closure_key, w, h);
            break;
        case 1:
            surface = _cairo_img_surface_create(write_png_stream,
                    closure_key, w, h);
            break;
        case 2:
            surface = cairo_svg_surface_create_for_stream(stdio_write_func,
                    NULL, w, h);
            break;
        case 3:
            surface = cairo_pdf_surface_create_for_stream(stdio_write_func,
                    NULL, w, h);
            break;
        case 4:
            surface = cairo_ps_surface_create_for_stream(stdio_write_func,
                    NULL, w, h);
            break;
        case 5:
            surface = cairo_ps_surface_create_for_stream(stdio_write_func,
                    NULL, w, h);
            cairo_ps_surface_set_eps(surface, 1);
            break;
    }
    return surface;
}

cairo_t *
_create_cairo(cairo_surface_t *surface)
{
    unsigned int fr, fg, fb, fa, br, bg, bb, ba;
    br = bg = bb = ba = 255;
    fr = fg = fb = 0; fa = 255;

    cairo_t *cr = cairo_create(surface);
    cairo_content_t content = cairo_surface_get_content(surface);

    switch (content) {
        case CAIRO_CONTENT_ALPHA:
            cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
            cairo_set_source_rgba(cr, 1., 1., 1., br / 255.);
            cairo_paint(cr);
            cairo_set_source_rgba(cr, 1., 1., 1.,
                    (fr / 255.) * (fa / 255.) + (br / 255) * (1 - (fa / 255.)));
            break;
        default:
        case CAIRO_CONTENT_COLOR:
        case CAIRO_CONTENT_COLOR_ALPHA:
            cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
            cairo_set_source_rgba(cr, br / 255., bg / 255., bb / 255., ba / 255.);
            cairo_paint(cr);
            cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
            cairo_set_source_rgba(cr, fr / 255., fg / 255., fb / 255., fa / 255.);
            break;
    }

    return cr;
}

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

int main(int argc, char *argv[])
{
    // Use font-config
    //const char *font_file = "/usr/share/fonts/truetype/nanum/NanumGothic.ttf"; // TrueType
    const char *font_file = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"; // Sans Font
    //const char *font_file = "/usr/share/fonts/opentype/cantarell/Cantarell-Regular.otf"; // OpenType
    //const char *font_file = "/usr/share/fonts/truetype/msttcorefonts/Courier_New.ttf"; // fixed witdh
    unsigned int font_idx = 0;

    double font_size = 40;

    char **line_txt;
    int line_len;

    int w, h;

    Font *font;
    Text **text;

    cairo_t *cr;
    cairo_surface_t *surface;

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

    // Create font
    font = _font_new(font_file, font_idx, font_size);

    double line_space = 0;
    // Create Text
    text = (Text **)malloc(sizeof(Text *) * line_len);
    for (int i = 0 ; i < line_len ; i++) {
        text[i] = _text_new(font, line_txt[i], NULL, NULL, NULL, NULL, line_space, 300, 3, true);
        //_create_text2(line_txt[i], NULL, NULL, NULL, NULL, font);
        free(line_txt[i]);
    }
    free(line_txt);

    double margin_left = 0;
    double margin_right = 0;
    double margin_top = 0;
    double margin_bottom = 0;

    // create cairo surface create
    w = 600;
    h = 600;
    cairo_user_data_key_t closure_key;
    int param = 0;
    if (argc == 2 && argv[1]) param = atoi(argv[1]);
    surface = _create_cairo_surface(param, w, h, &closure_key);

    // create cairo context
    cr = _create_cairo(surface);

    // Draw multiple texts
    cairo_save(cr);
    cairo_translate(cr, margin_left, margin_top);
    for (int i = 0 ; i < line_len ; i++) {
        if (!text[i]) continue;
        if (text[i]->vertical) {
            if (i) cairo_translate (cr, line_space, 0);
        }
        else {
            if (i) cairo_translate (cr, 0, line_space);
        }
        // draw cairo
        _text_cairo_draw(cr, font, text[i]);

        cairo_save(cr);
        double ww = text[i]->width;
        double hh = text[i]->height;
        cairo_rectangle(cr, 0, 0, ww, hh);
        cairo_set_line_width(cr, 1);
        cairo_set_source_rgba(cr, 1, 0, 0, 1);
        cairo_stroke(cr);
        cairo_restore(cr);

        if (text[i]->vertical) {
            cairo_translate (cr, text[i]->height, 0);
        }
        else {
            cairo_translate (cr, 0, text[i]->height);
        }
    }
    cairo_restore(cr);

    cairo_surface_t *surf = cairo_get_target(cr);
    RET_IF(!surf, -1);
	render_loop mainloop = cairo_surface_get_user_data (surf, &closure_key);
	RET_IF(!mainloop, -1);
    if (mainloop) mainloop(cr, w, h);


    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    _text_del(text[0]);
    free(text);

    _font_del(font);
    _font_shutdown();

    return 0;
}
