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
//#include <debug.h>
#define NEMO_DEBUG(fmt, ...) fprintf(stderr, fmt,##__VA_ARGS__);
//#define NEMO_DEBUG(...)

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

	NEMO_DEBUG("Glyph Name: %s\n", name);
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

	for (i = 0; i < npoints; i++) {
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

	for (i = 0, o = 0; i < ncontours; i++) {
		n = contours[i] - o + 1;

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

		o = contours[i] + 1;
	}

	fprintf(stdout, "'/>\n");

	fprintf(stdout, "</svg>\n");

out:
	FT_Done_Face(face);
	FT_Done_FreeType(library);

	return 0;
}
