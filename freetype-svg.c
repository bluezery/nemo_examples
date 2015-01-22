#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <ft2build.h>
#include <freetype.h>
#include <ftglyph.h>
#include <ftoutln.h>
#include <fttrigon.h>
#include <Ecore.h>
#include <Evas.h>
#include <Ecore_Evas.h>

#include <cairo-svg.h>
#include <cairo-pdf.h>
#include <cairo-ps.h>
#include <cairo.h>
//#include <debug.h>
#define NEMO_DEBUG(fmt, ...) fprintf(stderr, fmt,##__VA_ARGS__);
#define ERR(fmt, ...) fprintf(stderr, fmt,##__VA_ARGS__);
//#define NEMO_DEBUG(...)

#ifdef __GNUC__
#define __UNUSED__ __attribute__((__unused__))
#else
#define __UNUSED__
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

int main(int argc, char *argv[])
{
	FT_Library library;
	FT_Face face;
	FT_Error error;
	FT_UInt index;
	FT_Outline outline;
	FT_GlyphSlot slot;
	FT_Glyph_Metrics metrics;
	FT_Vector *points;
	char *tags;
	short *contours;
	char name[1024];
	int width, height;
	int npoints, ncontours;
	int i, j, o, n;

    if (argc <= 2) {
        ERR("freetype-svg [font file path] [Unicode char]\n");
        return -1;
    }

	error = FT_Init_FreeType(&library);
	if (error != 0)
		return -1;

	error = FT_New_Face(library, argv[1], 0, &face);
	if (error != 0)
		return -1;

	NEMO_DEBUG("Family Name: %s\n", face->family_name);
	NEMO_DEBUG("Style Name: %s\n", face->style_name);
	NEMO_DEBUG("Number of faces: %ld\n", face->num_faces);
	NEMO_DEBUG("Number of glyphs: %ld\n", face->num_glyphs);

	index = FT_Get_Char_Index(face, strtoul(argv[2], 0, 10));

	error = FT_Load_Glyph(face, index, FT_LOAD_NO_SCALE);
	if (error != 0)
		goto out;

	FT_Get_Glyph_Name(face, index, name, sizeof(name));

	slot = face->glyph;
	outline = slot->outline;
	metrics = slot->metrics;

	NEMO_DEBUG("Glyph Name: %s Index:%d\n", name, index);
	NEMO_DEBUG("Glyph Width: %ld, Height: %ld\n", metrics.width, metrics.height);
	NEMO_DEBUG("HoriAdvance: %ld, VertAdvance: %ld\n", metrics.horiAdvance, metrics.vertAdvance);
	NEMO_DEBUG("Number of points: %d\n", outline.n_points);
	NEMO_DEBUG("Number of contours: %d\n", outline.n_contours);

	points = outline.points;
	tags = outline.tags;
	contours = outline.contours;
	npoints = outline.n_points;
	ncontours = outline.n_contours;
	width = face->bbox.xMax - face->bbox.xMin;
	height = face->bbox.yMax - face->bbox.yMin;

    NEMO_DEBUG("npoints: %d\n", npoints);
	for (i = 0; i < npoints; i++) {
	    NEMO_DEBUG("[%d]%ld\n", i, points[i].y);
		points[i].y = points[i].y * -1 + height;
	}

	fprintf(stdout, "<svg width='%d' height='%d' xmlns='http://www.w3.org/2000/svg' version='1.1'>\n", width, height);

	for (i = 0; i < npoints; i++) {
		fprintf(stdout, "<circle fill='blue' stroke='black' cx='%ld' cy='%ld' r='%d'/>\n",
				points[i].x,
				points[i].y,
				10);
	}

	fprintf(stdout, "<path fill='black' stroke='black' fill-opacity='0.45' stroke-width='2' d='");

    cairo_user_data_key_t key;
    cairo_surface_t *surface = _create_cairo_surface(0, width, height, &key);
    cairo_t *cr = create_cairo(surface);

    cairo_scale(cr, 0.1, 0.1);
    cairo_set_line_width(cr, 5);
    cairo_set_source_rgba(cr, 1, 0, 0, 1);

	for (i = 0, o = 0; i < ncontours; i++) {
		n = contours[i] - o + 1;

        cairo_move_to(cr, points[o].x, points[o].y);
		fprintf(stdout, "M%ld,%ld ",
				points[o].x,
				points[o].y);

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
				fprintf(stdout, "L%ld,%ld ",
						points[p1].x,
						points[p1].y);
			} else if (tags[p2] != 0) {
				fprintf(stdout, "Q%ld,%ld %ld,%ld ",
						points[p1].x,
						points[p1].y,
						points[p2].x,
						points[p2].y);
			} else if (tags[p3] != 0) {
			    cairo_curve_to(cr,
			            points[p1].x,
			            points[p1].y,
			            points[p2].x,
			            points[p2].y,
			            points[p3].x,
			            points[p3].y);
				fprintf(stdout, "C%ld,%ld %ld,%ld %ld,%ld ",
						points[p1].x,
						points[p1].y,
						points[p2].x,
						points[p2].y,
						points[p3].x,
						points[p3].y);
			}
		}

		fprintf(stdout, "Z ");
		cairo_stroke(cr);

		o = contours[i] + 1;
	}

	fprintf(stdout, "'/>\n");

	fprintf(stdout, "</svg>\n");

	render_loop func = cairo_surface_get_user_data (cairo_get_target (cr), &key);
    if (func) func(cr, width, height);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);

out:
	FT_Done_Face(face);
	FT_Done_FreeType(library);

	return 0;
}
