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

#ifdef __GNUC__
#define __UNUSED__ __attribute__((__unused__))
#else
#define __UNUSED__
#endif

#define LOG_COLOR_BLUE "\x1b[34m"
#define LOG_COLOR_RED "\x1b[31m"
#define LOG_COLOR_GREEN "\x1b[32m"
#define LOG_COLOR_END "\x1b[0m"
#define LOGFMT(COLOR, fmt, ...) do { fprintf(stderr, "[%s:%s:%d] " fmt , __FILE__,  __func__, __LINE__, ##__VA_ARGS__); } while (0);
#define ERR(...) LOGFMT(LOG_COLOR_RED, ##__VA_ARGS__)
#define LOG(...) LOGFMT(LOG_COLOR_GREEN, ##__VA_ARGS__)


typedef struct _File_Map
{
    char *data;
    size_t len;
} File_Map;

typedef struct _Font
{
    hb_font_t *hb_font;
    const char **shapers;
    unsigned int size;
    double scale;
    double height;

    cairo_scaled_font_t *cairo_font; // drawing font
} Font;

typedef struct _Text
{
    char *utf8;
    unsigned int utf8_len;

    bool is_vertical;
    bool is_backward;

    unsigned int num_glyphs;
    cairo_glyph_t *glyphs;

    int num_clusters;
    cairo_text_cluster_t *clusters;
    cairo_text_cluster_flags_t cluster_flags;
    double width, height;
} Text;

#ifdef DEBUG
static void
_dump_cairo_font_extents(cairo_font_extents_t extents)
{
    LOG("ascent:%lf, descent:%lf, height:%lf, max_x_adv:%lf, max_y_adv:%lf\n",
         extents.ascent, extents.descent, extents.height, extents.max_x_advance,
         extents.max_y_advance);
}

static void
_dump_text(Text *text)
{
    LOG("[%d]%s\n", text->utf8_len, text->utf8);
    LOG("vertical:%d backward: %d\n", text->is_vertical, text->is_backward);
    LOG("num glyphs:[%3d]\n", text->num_glyphs);
    for(int i = 0 ; i < text->num_glyphs ; i++) {
        LOG("[%3d]: index:%6lx, x:%3.3lf y:%3.3lf\n", i,
            (text->glyphs)[i].index, (text->glyphs)[i].x, (text->glyphs)[i].y);
    }
    LOG("num clusters:[%3d] cluster flags[%3d]\n", text->num_clusters, text->cluster_flags);
    for(int i = 0 ; i < text->num_clusters ; i++) {
        LOG("[%3d]: num_bytes:%3d, num_glyphs:%3d\n", i,
            (text->clusters)[i].num_bytes, (text->clusters)[i].num_glyphs);
    }
}

static void
_dump_harfbuzz(hb_buffer_t *buffer, hb_font_t *font)
{
    unsigned int x_ppem, y_ppem;
    int x_scale, y_scale;
    unsigned int num_glyphs;
    hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer, &num_glyphs);
    hb_glyph_position_t *poses = hb_buffer_get_glyph_positions(buffer, NULL);

    hb_font_get_ppem(font, &x_ppem, &y_ppem);
    hb_font_get_scale(font, &x_scale, &y_scale);
    LOG("========================== Font info ===============================\n");
    LOG("x_ppem: %u, y_ppem: %u\n", x_ppem, y_ppem);
    LOG("x_scale: %d, y_scale: %d\n", x_scale, y_scale);
    LOG("========================== Buffer info ===============================\n");
    LOG("\tcontent_type: %d [unicode = 1, glyphs]\n", hb_buffer_get_content_type(buffer));
    LOG("\tdir: %d [LTR = 4, RTL, TTB, BTT]\n", hb_buffer_get_direction(buffer));
    LOG("\tcode point: %d\n",  hb_buffer_get_replacement_codepoint(buffer));
    LOG("\tflag: %u [DEFAULT = 0, BOT = 1u, EOT = 2u, IGNORE = 4u\n",  hb_buffer_get_flags(buffer));

    hb_codepoint_t pcode = 0;
    for (unsigned int i = 0 ; i < num_glyphs ; i++) {
        LOG("\t**************** Glyph Info. start *******************\n");
        LOG("\t\t[%2d] cluster:%d codepoint:%6X, x_offset:%3d y_offset:%3d, x_advance:%3d, y_advance:%3d\n",
            i, infos[i].cluster, infos[i].codepoint,
            poses[i].x_offset, poses[i].y_offset,
            poses[i].x_advance, poses[i].y_advance);

        if (font) {
            hb_codepoint_t code;
            hb_direction_t dir = hb_buffer_get_direction(buffer);

            // Harfbuzz codepoint
            if (hb_buffer_get_content_type(buffer) == 1) { // if unicode, convert it as glyph codepoint.
                hb_font_get_glyph(font, infos[i].codepoint, 0, &code);
            } else {  // itself already codepint.
                code = infos[i].codepoint;
            }
            LOG("\t\thb code: %6X\n", code);

            // Origin
            hb_position_t org_x, org_y, orgh_x, orgh_y, orgv_x, orgv_y;
            hb_font_get_glyph_origin_for_direction(font, code, dir, &org_x, &org_y);
            hb_font_get_glyph_h_origin(font, code, &orgh_x, &orgh_y);
            hb_font_get_glyph_v_origin(font, code, &orgv_x, &orgv_y);
            LOG("\t\tOrigin[x:%d, y:%d] / hori(x:%d, y:%d)  vert(x:%d, y:%d)\n",
                org_x, org_y, orgh_x, orgh_y, orgv_x, orgv_y);

            // Advance
            hb_position_t h_adv, v_adv;
            hb_font_get_glyph_advance_for_direction(font, code, dir, &h_adv, &v_adv);
            LOG("\t\tAdvance[hori:%d vert:%d] / hori(%d), vert(%d)\n",
                h_adv, v_adv,
                hb_font_get_glyph_h_advance(font, code), hb_font_get_glyph_v_advance(font, code));

            // Extents
            hb_glyph_extents_t extd, ext;
            hb_font_get_glyph_extents_for_origin(font, code, dir, &extd);
            hb_font_get_glyph_extents(font, code, &ext);
            LOG("\t\tx_bearing: %d, y_bearing: %d, width: %d, height: %d  x_bearing: %d, y_bearing: %d, width: %d, height: %d\n",
                extd.x_bearing, extd.y_bearing, extd.width, extd.height,
                ext.x_bearing, ext.y_bearing, ext.width, ext.height);

            // Kerning
            if (pcode != 0) {
                hb_position_t x, y;
                hb_position_t xx = 0, yy = 0;
                if (HB_DIRECTION_IS_HORIZONTAL(hb_buffer_get_direction(buffer)))
                    xx = hb_font_get_glyph_h_kerning(font, code, pcode);
                else
                    yy = hb_font_get_glyph_v_kerning(font, code, pcode);
                hb_font_get_glyph_kerning_for_direction(font, code, pcode, dir, &x, &y);
                LOG("\t\tkerning x:%d(%d), y:%d(%d)\n", x, xx, y, yy);
            }
            pcode = code;

#if 0
            // Glyph name, Almost all fonts does not support it?
            const char *name = "Ampersand";
            hb_codepoint_t cc;
            hb_font_get_glyph_from_name(font, name, -1, &cc);
            LOG("\t\tcode: %6X\n", cc);

            LOG("\t\t============= contour =============\n");
            unsigned int idx = 0;
            hb_position_t cx, cy, cxx, cyy;
            while (hb_font_get_glyph_contour_point_for_origin(font, code, idx, dir, &cx, &cy)) {
                if (!hb_font_get_glyph_contour_point(font, code, idx, &cxx, &cyy)) {
                    cxx = 0;
                    cyy = 0;
                }
                LOG("[%2d] cx: %d(%d), cy: %d(%d)", idx, cx, cxx, cy, cyy);
                idx++;
            }
            LOG("\n\t\t==================================\n");
#endif
        }
        LOG("\t**************** Glyph Info. End *********************\n");
    }
}

static void
_dump_cairo_glyph(cairo_glyph_t *glyph, unsigned int num_glyph)
{
    LOG("========================== cairo glyphs info ===============================\n");
    for (unsigned int i = 0 ; i < num_glyph ; i++) {
        LOG("[%2d] index: %6lx, x:%7.2lf y:%7.2lf\n",
            i, glyph[i].index, glyph[i].x, glyph[i].y);
    }
}

static void
_dump_cairo_text_cluster(cairo_text_cluster_t *cluster, unsigned int num_cluster)
{
    LOG("========================== cairo clusters info ===============================\n");
    for (unsigned int i = 0 ; i < num_cluster ; i++) {
        LOG("[%2d] num_bytes: %d, num_glyphs: %d\n",
            i, cluster[i].num_bytes, cluster[i].num_glyphs);
    }
}

static void
_dump_ft_face(FT_Face ft_face)
{
    LOG("===================== Font Face Info ==================\n");
    LOG("has vertical: %ld\n", FT_HAS_VERTICAL(ft_face));
    LOG("has horizontal: %ld", FT_HAS_HORIZONTAL(ft_face));
    LOG("has kerning: %ld", FT_HAS_KERNING(ft_face));
    LOG("sfnt: %ld\n", FT_IS_SFNT(ft_face));
    LOG("trick: %ld\n", FT_IS_TRICKY(ft_face));
    LOG("scalable: %ld\n", FT_IS_SCALABLE(ft_face));
    LOG("fixed_width: %ld\n", FT_IS_FIXED_WIDTH(ft_face));
    LOG("CID: %ld\n", FT_IS_CID_KEYED(ft_face)); // CID(Character Identifier Font),
    LOG("bitmap: %ld\n", FT_HAS_FIXED_SIZES(ft_face));
    LOG("color: %ld\n", FT_HAS_COLOR(ft_face));
    LOG("multiple master: %ld\n", FT_HAS_MULTIPLE_MASTERS(ft_face));
    LOG("w:%ld h:%ld hbx:x %ld, hby:%ld, hd:%ld, vbx:%ld, vby:%ld, vd:%ld\n",
            ft_face->glyph->metrics.width, ft_face->glyph->metrics.height,
            ft_face->glyph->metrics.horiBearingX, ft_face->glyph->metrics.horiBearingY,
            ft_face->glyph->metrics.horiAdvance,
            ft_face->glyph->metrics.vertBearingX, ft_face->glyph->metrics.vertBearingY,
            ft_face->glyph->metrics.vertAdvance);
    LOG("========================================================\n");
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

static File_Map *
create_file_map(const char *file)
{
    char *data;
    struct stat st;
    int fd;
    size_t len;

    if (!file) {
        ERR("file is NULL\n");
        return NULL;
    }
    if ((fd = open(file, O_RDONLY)) <= 0) {
        ERR("open failed\n");
        return NULL;
    }

    if (fstat(fd, &st) == -1) {
        ERR("fstat failed\n");
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
        ERR("mmap failed: %s, len:%lu, fd:%d\n", strerror(errno), len, fd);
        close(fd);
        return NULL;
    }
    close(fd);

    File_Map *fmap = (File_Map *)malloc(sizeof(File_Map));
    fmap->data = data;
    fmap->len = len;
    return fmap;
}

void destroy_file_map(File_Map *fmap)
{
    if (!fmap) return;
    if (munmap(fmap->data, fmap->len) < 0)
        ERR("munmap failed\n");
}

cairo_scaled_font_t *
create_cairo_scaled_font(FT_Face ft_face, const char *font_file, double font_size)
{
    cairo_font_face_t *cairo_face;
    cairo_matrix_t ctm, font_matrix;
    cairo_font_options_t *font_options;
    cairo_scaled_font_t *scaled_font;

    if (!ft_face) {
        LOG("hb ft font get face failed. Instead create ft face\n");
        FT_Library ft_lib;
        if (FT_Init_FreeType(&ft_lib) ||
            FT_New_Face(ft_lib, font_file, 0, &ft_face)) {
            ERR("FT Init FreeType or FT_New_Face failed\n");
        }
    }
    if (ft_face) {
        cairo_face = cairo_ft_font_face_create_for_ft_face(ft_face, 0);
        if (cairo_font_face_set_user_data(cairo_face, NULL, ft_face,
                    (cairo_destroy_func_t)FT_Done_Face)
            || cairo_font_face_status(cairo_face) != CAIRO_STATUS_SUCCESS) {
            ERR("cairo ft font face create for ft face failed\n");
            cairo_font_face_destroy(cairo_face);
            FT_Done_Face(ft_face);
            return NULL;
        }
    } else {
        ERR("Something wrong. Testing font is just created");
        // FIXME: Just for testing
        cairo_face = cairo_toy_font_face_create("@cairo:sans",
                CAIRO_FONT_SLANT_NORMAL,
                CAIRO_FONT_WEIGHT_NORMAL);
    }

    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&font_matrix, font_size, font_size);

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

static void
_destroy_font(Font *myfont)
{
    if (!myfont) return;
    hb_font_destroy(myfont->hb_font);
    cairo_scaled_font_destroy(myfont->cairo_font);
    free(myfont);
}

static Font *
_create_font(const char *file, unsigned int idx, int size)
{
    File_Map *fmap;

    hb_blob_t *blob;
    hb_face_t *face;
    hb_font_t *hb_font;
    cairo_scaled_font_t *cairo_font;

    unsigned int upem;
    double scale;

    Font *myfont;

    if (!file) {
        ERR("font file is NULL\n");
        return NULL;
    }

    fmap = create_file_map(file);
    if (!fmap->data || !fmap->len) {
        ERR("font data is NULL or length is zero\n");
        return NULL;
    }

    blob = hb_blob_create(fmap->data, fmap->len,
            HB_MEMORY_MODE_READONLY_MAY_MAKE_WRITABLE, fmap, (hb_destroy_func_t)destroy_file_map);
    if (!blob) {
        ERR("hb blob create failed\n");
        destroy_file_map(fmap);
        return NULL;
    }

    face = hb_face_create (blob, idx);
    hb_blob_destroy(blob);
    if (!face) {
        ERR("hb face create failed\n");
        return NULL;
    }
    // Usally, upem is 1000 for OpenType Shape font, 2048 for TrueType Shape font.
    // upem(Unit per em) is for master (or Em) space
    upem = hb_face_get_upem(face);
    if (upem <= 0) {
        ERR("impossible upem!!\n");
        hb_face_destroy(face);
        return NULL;
    }

    hb_font = hb_font_create(face);
    hb_face_destroy(face);
    if (!hb_font) {
        ERR("hb font create failed\n");
        return NULL;
    }
    hb_font_set_scale(hb_font, upem, upem);

    // You can choose backend free type or open type (harfbuzz internal) or your custom
    //hb_ft_font_set_funcs(hb_font);   // free type backend
    hb_ot_font_set_funcs(hb_font);   // open type backend
#if 0 // Custom backend
    hb_font_set_funcs(font2, func, NULL, NULL);
    static hb_font_funcs_t *font_funcs = NULL;
    hb_font_funcs_set_glyph_h_kerning_func
    hb_font_funcs_set_glyph_h_advance_func(font_funcs, xxx, NULL, NULL);
    hb_font_funcs_set_glyph_h_kerning_func(font_funcs, xxx, NULL, NULL);
    //...
#endif

    // Create cairo drawing font
    cairo_font = create_cairo_scaled_font(hb_ft_font_get_face(hb_font), file, upem);
    if (!cairo_font) {
        ERR("create cairo scaled font failed\n");
        hb_font_destroy(hb_font);
        return NULL;
    }
    cairo_font_extents_t extents;
    cairo_scaled_font_extents(cairo_font, &extents);
    scale = (double)size/extents.height;

    myfont = (Font *)malloc(sizeof(Font));
    myfont->hb_font = hb_font;
    // Scale will be used to multiply master outline coordinates to produce
    // pixel distances on a device.
    // i.e., Conversion from mater (Em) space into device (or pixel) space.
    myfont->shapers = NULL;  //e.g. {"ot", "fallback", "graphite2", "coretext_aat"}
    myfont->cairo_font = cairo_font;
    myfont->size = size;
    myfont->scale = scale;
    myfont->height = extents.height * scale;

    return myfont;
}

static hb_buffer_t *
_create_hb_buffer(const char *utf8, unsigned int utf8_len, const char *dir, const char *script, const char *lang,
        const char *features, const char **hb_shapers, hb_font_t *hb_font)
{
    hb_buffer_t *hb_buffer;
    hb_feature_t *hb_features = NULL;
    unsigned int num_features = 0;

    if (!utf8 || !hb_font) {
        ERR("utf8 is NULL or hb_font is NULL");
        return NULL;
    }


    // *********** Buffer creation ***********//
    hb_buffer = hb_buffer_create ();
    if (!hb_buffer_allocation_successful(hb_buffer)) {
        ERR("hb buffer create failed\n");
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
                ERR("hb feature from string failed\n");
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
        ERR("hb shape full failed\n");
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
_destroy_text(Text *mytext)
{
    if (!mytext) return;
    cairo_text_cluster_free(mytext->clusters);
    cairo_glyph_free(mytext->glyphs);
    free(mytext->utf8);
    free(mytext);
}

static Text *
_create_text(const char *utf8, const char *dir, const char *script, const char *lang,
        const char *features, Font *font)
{
    Text *mytext;

    unsigned int utf8_len;
    bool is_backward;

    // harfbuzz
    hb_buffer_t *hb_buffer;
    hb_glyph_info_t *hb_glyphs;
    hb_glyph_position_t *hb_glyph_poses;

    // cairo
    unsigned int num_glyphs;
    cairo_glyph_t *glyphs;
    unsigned int num_clusters;
    cairo_text_cluster_t *clusters;
    cairo_text_cluster_flags_t cluster_flags;
    double width, height;

    if (!utf8 || !font) {
        ERR("utf8 is NULL or font is NULL\n");
        return NULL;
    }

    utf8_len = strlen(utf8);
    if (utf8_len == 0) {
        ERR("utf8 length is 0\n");
        return NULL;
    }

    hb_buffer = _create_hb_buffer(utf8, utf8_len, dir, script, lang, features, font->shapers, font->hb_font);

    // Convert from harfbuzz glyphs to cairo glyphs
    hb_glyphs = hb_buffer_get_glyph_infos(hb_buffer, &num_glyphs);
    if (!num_glyphs) {
        ERR("num of glyphs is zero");
        hb_buffer_destroy(hb_buffer);
        return NULL;
    }
    hb_glyph_poses = hb_buffer_get_glyph_positions(hb_buffer, NULL);

    num_clusters = 1;
    for (unsigned int i = 1 ; i < num_glyphs ; i++) {
        if (hb_glyphs[i].cluster != hb_glyphs[i-1].cluster)
            num_clusters++;
    }
    clusters = cairo_text_cluster_allocate(num_clusters);
    if (!clusters) {
        ERR("cairo text cluster allocate failed!!\n");
        hb_buffer_destroy(hb_buffer);
        return NULL;
    }
    memset(clusters, 0, num_clusters * sizeof(clusters[0]));

    glyphs = cairo_glyph_allocate (num_glyphs + 1);
    if (!glyphs) {
        ERR("cairo glyph allocate failed\n");
        hb_buffer_destroy(hb_buffer);
        return NULL;
    }

    hb_position_t x = 0, y = 0;
    unsigned int i = 0;
    for (i = 0; i < num_glyphs ; i++) {
        glyphs[i].index = hb_glyphs[i].codepoint;
        glyphs[i].x =  (hb_glyph_poses[i].x_offset + x);
        glyphs[i].y = -(hb_glyph_poses[i].y_offset + y);
        x += hb_glyph_poses[i].x_advance;
        y += hb_glyph_poses[i].y_advance;
        LOG("%d %d\n", i, x);
    }
    glyphs[i].index = -1;
    glyphs[i].x = x;
    glyphs[i].y = y;

    width = (double) x * font->scale;
    height = font->height;

    is_backward = HB_DIRECTION_IS_BACKWARD(hb_buffer_get_direction(hb_buffer));
    cluster_flags =
        is_backward ? CAIRO_TEXT_CLUSTER_FLAG_BACKWARD : (cairo_text_cluster_flags_t) 0;

    unsigned int cluster = 0;
    const char *start = utf8;
    const char *end;
    clusters[cluster].num_glyphs++;

    if (is_backward) {
        for (int i = num_glyphs - 2; i >= 0; i--) {
            if (hb_glyphs[i].cluster != hb_glyphs[i+1].cluster) {
                if (hb_glyphs[i].cluster >= hb_glyphs[i+1].cluster) {
                    ERR("cluster index is not correct");
                    cairo_glyph_free(glyphs);
                    cairo_text_cluster_free(clusters);
                    hb_buffer_destroy(hb_buffer);
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
        for (unsigned int i = 1 ; i < num_glyphs ; i++) {
            if (hb_glyphs[i].cluster != hb_glyphs[i-1].cluster) {
                if (hb_glyphs[i].cluster <= hb_glyphs[i-1].cluster) {
                    ERR("cluster index is not correct");
                    cairo_glyph_free(glyphs);
                    cairo_text_cluster_free(clusters);
                    hb_buffer_destroy(hb_buffer);
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

    mytext = (Text *)malloc(sizeof(Text));
    mytext->utf8 = strdup(utf8);
    mytext->utf8_len = utf8_len;
    mytext->is_vertical = HB_DIRECTION_IS_VERTICAL (hb_buffer_get_direction(hb_buffer));
    mytext->is_backward = is_backward;

    mytext->num_glyphs = num_glyphs;
    mytext->glyphs = glyphs;

    mytext->num_clusters = num_clusters;
    mytext->clusters = clusters;
    mytext->cluster_flags = cluster_flags;
    mytext->width = width;
    mytext->height = height;

    hb_buffer_destroy(hb_buffer);

    return mytext;
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
    //LOG("w:%lf h:%lf\n", w, h);
}
*/

cairo_t *
create_cairo(cairo_surface_t *surface)
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

static void
draw_cairo(cairo_t *cr, Font *font, int line_len, Text *l[], double line_space,
        double margin_left, double margin_top,
        unsigned int vertical)

{
    double scale;

    cairo_font_extents_t font_extents;

    cairo_save(cr);

    cairo_font_extents(cr, &font_extents);

    scale = font->size/font_extents.height;
    cairo_scale(cr, scale, scale);

    cairo_translate(cr, margin_left, margin_top);

    double descent;
    if (vertical) {
        descent = font_extents.height * (line_len + .5);
        cairo_translate (cr, descent, 0);
    } else {
        descent = font_extents.descent;
        cairo_translate (cr, 0, -descent);
    }

    for (unsigned int i = 0 ; i < line_len ; i++) {
        if (vertical) {
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
            for (unsigned i = 0; i < l[i]->num_glyphs; i++) {
                cairo_move_to (cr, l[i]->glyphs[i].x, l[i]->glyphs[i].y);
                cairo_rel_line_to (cr, 0, 0);
            }
            cairo_stroke (cr);

            cairo_restore (cr);
        }
#if 1
        if (0 && cairo_surface_get_type (cairo_get_target (cr)) == CAIRO_SURFACE_TYPE_IMAGE) {
            /* cairo_show_glyphs() doesn't support subpixel positioning */
            cairo_glyph_path (cr, l[i]->glyphs, l[i]->num_glyphs);
            cairo_fill (cr);
        } else if (l[i]->num_clusters) {
            cairo_show_text_glyphs (cr,
                    l[i]->utf8, l[i]->utf8_len,
                    l[i]->glyphs, l[i]->num_glyphs,
                    l[i]->clusters, l[i]->num_clusters,
                    l[i]->cluster_flags);
        } else {
            cairo_show_glyphs (cr, l[i]->glyphs, l[i]->num_glyphs);
        }
#endif
    }

    cairo_restore (cr);
}

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

void write_png_stream(cairo_t *cr, int w __UNUSED__, int h __UNUSED__)
{
    cairo_status_t status;

    cairo_surface_t *surface = cairo_get_target(cr);
    if (!surface) ERR("cairo get target failed\n");

    status = cairo_surface_write_to_png_stream(surface,
            stdio_write_func,
            NULL);
    if (status != CAIRO_STATUS_SUCCESS)
        ERR("error:%s\n", cairo_status_to_string(status));
}

void render_evas(cairo_t *cr, int w, int h)
{
    Ecore_Evas *ee;
    Evas_Object *img;

    if (!ecore_evas_init()) return;

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


typedef void (*closure_func)(cairo_t *cr, int w_in_pt, int h_in_pt);

static cairo_surface_t *
_cairo_img_surface_create(closure_func func,
        void *key,
        int w, int h)
{
    cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
    cairo_status_t status = cairo_surface_status(surface);
    if (status != CAIRO_STATUS_SUCCESS) ERR("error:%s\n", cairo_status_to_string(status));

    if (cairo_surface_set_user_data(surface,
                key,
                func,
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


char **_read_file(const char *file, int *line_len)
{
    FILE *fp;
    char **line = NULL;
    int idx = 0;
    char *buffer = NULL;
    size_t buffer_len;

    if (!file || !line_len) {
        ERR("file name is NULL or line_len is NULL");
        return NULL;
    }

    fp = fopen(file, "r");
    if (!fp) {
        ERR("%s\n", strerror(errno));
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
    // Use font-config
    const char *font_file = "/usr/share/fonts/truetype/nanum/NanumGothic.ttf"; // TrueType
    //const char *font_file = "/usr/share/fonts/opentype/cantarell/Cantarell-Regular.otf"; // OpenType
    //const char *font_file = "/usr/share/fonts/truetype/msttcorefonts/Courier_New.ttf"; // fixed witdh
    unsigned int font_idx = 0;

    double font_size = 100;

    char **line_txt;
    int line_len;

    int w, h;

    Font *myfont;
    Text **mytext;

    // Cairo
    cairo_t *cr;
    cairo_surface_t *surface;

    if (argc != 2 || !argv[1]) {
        ERR("Usage: show [file name]\n");
        return 0;
    }

    myfont = _create_font(font_file, font_idx, font_size);

    line_txt = _read_file(argv[1], &line_len);
    if (!line_txt || !line_txt[0] || line_len <= 0) {
        ERR("Err: line_txt is NULL or no string or length is 0\n");
        return -1;
    }
    mytext = (Text **)malloc(sizeof(Text *) * line_len);
    for (int i = 0 ; i < line_len ; i++) {
        mytext[i] = _create_text(line_txt[i], NULL, NULL, NULL, NULL, myfont);
        free(line_txt[i]);
    }
    free(line_txt);

    double line_space = 0;
    double margin_left = 0;
    double margin_right = 0;
    double margin_top = 0;
    double margin_bottom = 0;

    // create cairo surface create
    w = 480;
    h = 480;
    cairo_user_data_key_t closure_key;
    int param = 0;
    if (argc == 2 && argv[1]) param = atoi(argv[1]);
    surface = _create_cairo_surface(param, w, h, &closure_key);

    // create cairo context
    cr = create_cairo(surface);
    cairo_set_scaled_font(cr, myfont->cairo_font);

    // draw cairo
    draw_cairo(cr, myfont, line_len, mytext, line_space, margin_left, margin_top, false);

    for (int i = 0 ; i < line_len ; i++) {
        double w = mytext[i]->width;
        double h = mytext[i]->height;
        LOG("%lf %lf\n", w, h);
        cairo_rectangle(cr, 0, h * i , w, h);
    }
    cairo_set_line_width(cr, 1);
    cairo_set_source_rgba(cr, 1, 0, 0, 1);
    cairo_stroke(cr);

	closure_func func = cairo_surface_get_user_data (cairo_get_target (cr), &closure_key);
    if (func) func(cr, w, h);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    for (int i = 0 ; i < line_len ; i++) _destroy_text(mytext[i]);
    free(mytext);

    _destroy_font(myfont);

    return 0;
}
