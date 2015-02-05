// errno type
#include "cairo_view.h"
#include <errno.h>

#include "text.h"
#include "log.h"

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
    font = _font_create(font_file, font_idx, font_size);

    double line_space = 0;
    // Create Text
    text = (Text **)malloc(sizeof(Text *) * line_len);
    for (int i = 0 ; i < line_len ; i++) {
        text[i] = _text_create(font, line_txt[i], NULL, NULL, NULL, NULL, line_space, 0, 3, true);
        //_create_text2(line_txt[i], NULL, NULL, NULL, NULL, font);
        free(line_txt[i]);
    }
    free(line_txt);

    double margin_left = 0;
    //double margin_right = 0;
    double margin_top = 0;
    //double margin_bottom = 0;

    // create cairo surface create
    cairo_t *cr;
    cairo_surface_t *surf;

    w = 600;
    h = 600;
    cairo_user_data_key_t key;
    int param = 0;
    if (argc == 2 && argv[1]) param = atoi(argv[1]);
    surf = _cairo_surface_create(param, w, h, &key);

    // create cairo context
    cr = _cairo_create(surf, 255, 255, 255, 255);

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

	_Cairo_Render func = cairo_surface_get_user_data (surf, &key);
	RET_IF(!func, -1);
	if (func) func(cr, w, h);

    cairo_surface_destroy(surf);
    cairo_destroy(cr);

    _text_destroy(text[0]);
    free(text);

    _font_destroy(font);
    _font_shutdown();

    return 0;
}
