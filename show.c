#include <math.h>
#include <glib.h>
#include <hb-ft.h>
#include <cairo.h>
#include <cairo-ft.h>
#include <cairo-svg.h>
#include <cairo-pdf.h>
#include <cairo-ps.h>
#include <stdlib.h>
#include <errno.h>

#include <Ecore.h>
#include <Evas.h>
#include <Ecore_Evas.h>

#define FREETYPE 1
#define OUT_FILE "test"
#define FONT_FILE "/usr/share/fonts/truetype/nanum/NanumGothic.ttf"
#define FONT_IDX 0
#define FONT_SIZE 256

#ifdef __GNUC__
#define __UNUSED__ __attribute__((__unused__))
#else
#endif

#define LOG(...) fprintf(stderr, __VA_ARGS__);
#define ERR(...) fprintf(stderr, __VA_ARGS__);

static hb_font_t *
create_hb_font(const char *font_file, unsigned int font_idx)
{
    hb_font_t *font = NULL;

    GError *err = NULL;
    GMappedFile *mf = g_mapped_file_new(font_file, FALSE, &err);
    if (!mf) {
        ERR("%s", err->message);
        return NULL;
    }

    char *font_data = g_mapped_file_get_contents(mf);
    unsigned int font_len = g_mapped_file_get_length(mf);
    if (!font_data || !font_len) {
        ERR("font data is NULL or length is zero\n");
        g_mapped_file_unref(mf);
        return NULL;
    }

    hb_blob_t *blob = hb_blob_create (font_data, font_len,
            HB_MEMORY_MODE_READONLY_MAY_MAKE_WRITABLE, mf,
            (hb_destroy_func_t)g_mapped_file_unref);
    if (!blob) {
        ERR("hb blob create failed\n");
        g_mapped_file_unref(mf);
        return NULL;
    }

    hb_face_t *face = hb_face_create (blob, font_idx);
    hb_blob_destroy(blob);
    if (!face) {
        ERR("hb face create failed\n");
        return NULL;
    }

    unsigned int upem = hb_face_get_upem(face);

    font = hb_font_create(face);
    hb_face_destroy(face);
    if (!font) {
        ERR("hb font create failed\n");
        return NULL;
    }
    hb_font_set_scale(font, upem, upem);

#if FREETYPE
    hb_ft_font_set_funcs(font);
#else // Open Type
    hb_ot_font_set_funcs(font);
#endif

#if 0
    // Set functions for
    hb_font_set_funcs(font2, func, NULL, NULL);
    static hb_font_funcs_t *font_funcs = NULL;
    hb_font_funcs_set_glyph_h_advance_func(font_funcs, xxx, NULL, NULL);
    hb_font_funcs_set_glyph_h_kerning_func(font_funcs, xxx, NULL, NULL);
#endif
    return font;
}

hb_buffer_t *
create_hb_buffer(const char *text, const char *before, const char *after, const char *direction,
        const char *script, const char *lang)
{
    hb_buffer_t *buffer = hb_buffer_create ();
    if (!buffer || !hb_buffer_allocation_successful(buffer)) {
        ERR("hb buffer create failed\n");
        return NULL;
    }
    hb_buffer_clear_contents(buffer);

    if (before) {
        unsigned int len = strlen(before);
        hb_buffer_add_utf8(buffer, before, len, len, 0);
    }
    unsigned int len = strlen(text);
    hb_buffer_add_utf8(buffer, text, len, 0, len);
    //hb_buffer_add_utf32(buffer, (const uint32_t *)str, len, 0, len);
    if (after) {
        hb_buffer_add_utf8(buffer, after, -1, 0, 0);
    }
    //LOG("%s %s %s\n", before, text, after);

    if (1) // !utf8_cluster
    {
        unsigned int num_glyphs = hb_buffer_get_length(buffer);
        hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buffer, NULL);
        for (unsigned int i = 0 ; i < num_glyphs; i++) {
            info->cluster = i;
            info++;
        }
    }

    // setup buffer
    hb_direction_t hb_dir = hb_direction_from_string(direction, -1);
    hb_script_t hb_script = hb_script_from_string(script, -1);
    //LANG = getenv("LANG");
    hb_language_t hb_lang = hb_language_from_string(lang, -1);
    hb_buffer_flags_t hb_flags = HB_BUFFER_FLAG_DEFAULT;
    // HB_BUFFER_FLAG_BOT | HB_BUFFER_FLAG_EOT | HB_BUFFER_FLAG_PRESERVE_DEFAULT_IGNORABLES;

    hb_buffer_set_direction(buffer, hb_dir);
    hb_buffer_set_script(buffer, hb_script);
    hb_buffer_set_language(buffer, hb_lang);
    hb_buffer_set_flags(buffer, hb_flags);

    // Default buffer property setup
    hb_buffer_guess_segment_properties(buffer);

    return buffer;
}

typedef struct _cairo_glyph_line
{
    int num_glyphs;
    cairo_glyph_t *glyphs;

    int num_clusters;
    cairo_text_cluster_t *clusters;

    cairo_text_cluster_flags_t cluster_flags;

    char *utf8;
    unsigned int utf8_len;
} cairo_glyph_line;

    static cairo_glyph_line *
create_cairo_glyph_line(hb_buffer_t *buffer, double scale, const char *text)
{
    cairo_glyph_line *l = calloc(sizeof(cairo_glyph_line), 1);
    // ********** Consume Glyphs **********************//
    l->num_glyphs = hb_buffer_get_length(buffer);

    unsigned int hb_glyph_len = 0, hb_glyph_pos_len = 0;
    hb_glyph_info_t *hb_glyph = hb_buffer_get_glyph_infos(buffer, &hb_glyph_len);
    hb_glyph_position_t *hb_glyph_pos = hb_buffer_get_glyph_positions(buffer, &hb_glyph_pos_len);

    // Why buffer length is used instead of glyph info length?
    l->glyphs = cairo_glyph_allocate (l->num_glyphs + 1);
    if (!l->glyphs) {
        ERR("error!!\n");
    }

    // cluster
    if (text) {
        l->utf8 = g_strndup(text, strlen(text));
        if (!l->utf8) {
            ERR("error!!\n");
        }
        l->utf8_len = strlen(text);
        unsigned int i = 0;
        l->num_clusters = l->num_glyphs ? 1 : 0;
        for (i = 1 ; i < l->num_glyphs ; i++) {
            if (hb_glyph[i].cluster != hb_glyph[i-1].cluster)
                l->num_clusters++;
        }
        l->clusters = cairo_text_cluster_allocate(l->num_clusters);
        if (!l->clusters) ERR("error!!\n");
    }

    /*
       LOG("utf8:%s, len: %u, num cluster: %u, num glyphs: %d\n",
       l->utf8, l->utf8_len, l->num_clusters, l->num_glyphs);*/

    hb_position_t x = 0, y = 0;
    int i = 0;
    for (i = 0; i < l->num_glyphs ; i++) {
        l->glyphs[i].index = hb_glyph[i].codepoint;
        l->glyphs[i].x = ( hb_glyph_pos[i].x_offset + x) * scale;
        l->glyphs[i].y = (-hb_glyph_pos[i].y_offset + y) * scale;
        x +=  hb_glyph_pos[i].x_advance;
        y += -hb_glyph_pos[i].y_advance;
        /*
           LOG("[%u] index: %lu, x:%lf , y:%lf, x:%d, y:%d\n", i, l->glyphs[i].index,
           l->glyphs[i].x, l->glyphs[i].y, x, y );
           */
    }
    l->glyphs[i].index = -1;
    l->glyphs[i].x = x * scale;
    l->glyphs[i].y = y * scale;

    if (l->num_clusters) {
        memset(l->clusters, 0, l->num_clusters * sizeof(l->clusters[0]));
        hb_direction_t hb_dir = hb_buffer_get_direction(buffer);
        hb_bool_t backward = HB_DIRECTION_IS_BACKWARD(hb_dir);
        l->cluster_flags =
            backward ? CAIRO_TEXT_CLUSTER_FLAG_BACKWARD : (cairo_text_cluster_flags_t) 0;

        unsigned int cluster = 0;
        const char *start = l->utf8, *end;
        l->clusters[cluster].num_glyphs++;

        if (backward) {
            for (i = l->num_glyphs - 2; i >= 0; i--) {
                if (hb_glyph[i].cluster != hb_glyph[i+1].cluster) {
                    g_assert (hb_glyph[i].cluster > hb_glyph[i+1].cluster);
                    if (0) // utf8_cluster?
                        end = start + hb_glyph[i].cluster - hb_glyph[i+1].cluster;
                    else
                        end = g_utf8_offset_to_pointer (start, hb_glyph[i].cluster - hb_glyph[i+1].cluster);
                    l->clusters[cluster].num_bytes = end - start;
                    start = end;
                    cluster++;
                }
                l->clusters[cluster].num_glyphs++;
            }
            l->clusters[cluster].num_bytes = l->utf8 + l->utf8_len - start;
        } else {
            for (i = 1 ; i < l->num_glyphs ; i++) {
                if (hb_glyph[i].cluster != hb_glyph[i-1].cluster) {
                    g_assert(hb_glyph[i].cluster > hb_glyph[i-1].cluster);
                    if (0) // utf8 cluster?
                        end  = start + hb_glyph[i].cluster - hb_glyph[i-1].cluster;
                    else
                        end = g_utf8_offset_to_pointer(start,
                                hb_glyph[i].cluster - hb_glyph[i-1].cluster);

                    l->clusters[cluster].num_bytes = end - start;

                    start = end;
                    /*
                       LOG("i:%u(%u),i-1:%u(%u),start:%p, end:%p,cluster: %d,bytes:%d, num_glyps: %d\n",
                       i, hb_glyph[i].cluster,	i-1, hb_glyph[i-1].cluster,
                       start, end, cluster, l->clusters[cluster].num_bytes,
                       l->clusters[cluster].num_glyphs);
                       */
                    cluster++;
                }
                l->clusters[cluster].num_glyphs++;
            }
            l->clusters[cluster].num_bytes = l->utf8 + l->utf8_len - start;
        }

    }
    return l;
}

void shape(hb_font_t *font, hb_buffer_t *buffer)
{
    // ********* Shaping ***************** //
    // what is features?
    hb_feature_t *features = NULL;
    unsigned int num_features = 0;

    // what is shapers?
    const char **shapers = NULL;
    //const char **shapers = {"fallback", NULL};

    if (!hb_shape_full(font, buffer, features, num_features, shapers)) {
        hb_buffer_set_length(buffer, 0);
        ERR("hb shape full failed\n");
    }
    if (0) hb_buffer_normalize_glyphs(buffer);
}

cairo_scaled_font_t *
create_cairo_scaled_font(FT_Face ft_face, double font_size)
{
    cairo_scaled_font_t *scaled_font;
    cairo_font_face_t *cairo_face;
    if (!ft_face) {
        ERR("error\n");
        /** at least show something **/
        cairo_face = cairo_toy_font_face_create("@cairo:sans",
                CAIRO_FONT_SLANT_NORMAL,
                CAIRO_FONT_WEIGHT_NORMAL);

    } else {
        cairo_face = cairo_ft_font_face_create_for_ft_face(ft_face, 0);
    }

    cairo_matrix_t ctm, font_matrix;
    cairo_font_options_t *font_options;

    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&font_matrix, font_size, font_size);

    font_options = cairo_font_options_create();
    cairo_font_options_set_hint_style(font_options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_hint_metrics(font_options, CAIRO_HINT_METRICS_OFF);

    scaled_font = cairo_scaled_font_create (cairo_face,
            &font_matrix, &ctm, font_options);

    cairo_font_options_destroy(font_options);
    cairo_font_face_destroy(cairo_face);

    return scaled_font;
}

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
    LOG("w:%lf h:%lf\n", w, h);
}

cairo_t *
create_cairo_ctx(cairo_surface_t *surface)
{
    unsigned int fr, fg, fb, fa, br, bg, bb, ba;
    br = bg = bb = ba = 255;
    fr = fg = fb = 0; fa = 255;

    /*
       if (ba == 255 && br == bg && bg == bb && fr == fg && fg == fb)
       surface = cairo_image_surface_create (CAIRO_FORMAT_A8, w, h);
       else if (ba == 255)
       surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, w, h);
       else
       surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
       */

    cairo_t *cr = cairo_create(surface);
    cairo_content_t content = cairo_surface_get_content(surface);

    switch (content) {
        case CAIRO_CONTENT_ALPHA:
            LOG("alphb\n");
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
draw_cairo(cairo_t *cr, int line_len, cairo_glyph_line *l, double line_space,
        double margin_left, double margin_top,
        unsigned int vertical)

{
    // DRAW
    cairo_save(cr);

    int v = vertical ? 1 : 0;
    int h = vertical ? 0 : 1;
    cairo_font_extents_t font_extents;
    cairo_font_extents(cr, &font_extents);
    cairo_translate(cr, margin_left, margin_top);
    double descent;
    if (vertical)
        descent = font_extents.height * (line_len + .5);
    else
        descent = font_extents.height - font_extents.ascent;
    cairo_translate (cr, v * descent, h * -descent);

    //LOG("descent: %lf\n", descent);
    for (unsigned int i = 0 ; i < line_len ; i++) {
        if (i)
            cairo_translate (cr, v * -line_space, h * line_space);

        cairo_translate (cr, v * -font_extents.height, h * font_extents.height);

        // annotate
        if (0) {
            cairo_save (cr);

            /* Draw actual glyph origins */
            cairo_set_source_rgba (cr, 1., 0., 0., .5);
            cairo_set_line_width (cr, 5);
            cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
            for (unsigned i = 0; i < l->num_glyphs; i++) {
                cairo_move_to (cr, l->glyphs[i].x, l->glyphs[i].y);
                cairo_rel_line_to (cr, 0, 0);
            }
            cairo_stroke (cr);

            cairo_restore (cr);
        }

        if (0 && cairo_surface_get_type (cairo_get_target (cr)) == CAIRO_SURFACE_TYPE_IMAGE) {
            /* cairo_show_glyphs() doesn't support subpixel positioning */
            cairo_glyph_path (cr, l->glyphs, l->num_glyphs);
            cairo_fill (cr);
        } else if (l->num_clusters)
            cairo_show_text_glyphs (cr,
                    l->utf8, l->utf8_len,
                    l->glyphs, l->num_glyphs,
                    l->clusters, l->num_clusters,
                    l->cluster_flags);
        else
            cairo_show_glyphs (cr, l->glyphs, l->num_glyphs);
        l++;
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


int main(int argc, char *argv[])
{
    // Tuning values
    const char *font_file = FONT_FILE;
    unsigned int font_idx = FONT_IDX;
    double font_size = FONT_SIZE;

    int line_len = 1;
    double line_space = 0;
    double margin_left = 16;
    double margin_right = 16;
    double margin_top = 16;
    double margin_bottom = 16;
    double w = 0, h = 0;

    const char *text = "김태환";
    const char *direction = NULL;
    const char *script = NULL;
    const char *lang = NULL;

    // Cairo
    cairo_t *cr;
    cairo_surface_t *surface;
    cairo_scaled_font_t *scaled_font;
    cairo_glyph_line *l;
    unsigned char is_vertical;

    // Harfubuzz
    hb_font_t *hb_font;
    hb_face_t *hb_face;
    hb_buffer_t *hb_buffer;

    // FreeType
    FT_Face ft_face;

    // Start

    hb_font = create_hb_font(font_file, font_idx);

    // Calculate scale
    hb_face = hb_font_get_face(hb_font);
    unsigned int upem = hb_face_get_upem(hb_face);
    double scale = font_size/upem;
    LOG("font face upem: %u, font size: %lf, font scale: %lf\n", upem, font_size, scale);

    hb_buffer = create_hb_buffer(text, NULL, NULL, direction, script, lang);
    is_vertical = HB_DIRECTION_IS_VERTICAL (hb_buffer_get_direction(hb_buffer));

    // font shaping for buffer
    shape(hb_font, hb_buffer);

    // Get freetype face for cairo
    ft_face = hb_ft_font_get_face(hb_font);
    if (!ft_face) {
        FT_Library ft_lib;
        FT_Init_FreeType(&ft_lib);
        if (!ft_lib) ERR("error\n");
        FT_New_Face(ft_lib, font_file, 0, &ft_face);
    }

    // Mapping buffer for cairo glyph
    l = create_cairo_glyph_line(hb_buffer, scale, text);
    hb_buffer_destroy(hb_buffer);

    // create cairo font
    scaled_font = create_cairo_scaled_font(ft_face, font_size);

    // calculate surface size
    calc_surface_size(scaled_font, line_len, l, line_space,
            margin_left, margin_right, margin_top, margin_bottom,
            is_vertical,
            &w, &h);

    cairo_user_data_key_t closure_key;
    // width, height should be points
    int ww = ceil(w);
    int hh = ceil(h);

    //*********************** CAIRO ***********************//
    // create cairo surface create
    int param = 0;
    if (argc == 2 && argv[1]) param = atoi(argv[1]);
    surface = _create_cairo_surface(param, ww, hh, &closure_key);

    // create cairo context
    cr = create_cairo_ctx(surface);
    cairo_set_scaled_font(cr, scaled_font);
    cairo_scaled_font_destroy(scaled_font);

    // draw cairo
    draw_cairo(cr, line_len, l, line_space, margin_left, margin_top, is_vertical);

	closure_func func = cairo_surface_get_user_data (cairo_get_target (cr), &closure_key);
    if (func) func(cr, ww, hh);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    free(l);
    hb_font_destroy(hb_font);

    /*
       for (unsigned int i = 0; i < line_len; i++) {
       if (l->glyphs) cairo_glyph_free(l->glyphs);
       if (l->clusters) cairo_text_cluster_free(l->clusters);
       if (l->utf8) free(l->utf8);
       free(l);
       l++;
       }
       */


    return 0;
}
