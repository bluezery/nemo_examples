#include "cairo_wayland.h"
#include "log.h"

int main()
{
    Window *window;

    int w = 640, h = 480;
    int stride;

    stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, w);

    // Drawing
    cairo_t *cr;
    cairo_surface_t *cairo_surface;
    cairo_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cr = cairo_create(cairo_surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0., 0., 0., 1.);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr, 0., 1., 1., 1.);
    cairo_rectangle(cr, 10, 10, 100, 100);
    cairo_fill(cr);

    window = _window_create(w, h, stride);
    unsigned char *map = cairo_image_surface_get_data(cairo_surface);
    _window_set_buffer(window, map, h * stride);
    _window_loop(window);

#if 0
	disp->xkb_context = xkb_context_new(0);
	if (!disp) {
		fprintf(stderr, "Failed to create XKB context\n");
		free(disp);
		return -1;
	}
#endif
}
