#ifndef	__NEMO_PATH_H__
#define	__NEMO_PATH_H__

#include <stdint.h>
#include <math.h>
#include <cairo.h>

#include <bezierhelper.h>
#include <mischelper.h>

#define NEMOPATH_CUBIC_BEZIER_FLATTEN_STEPS		(100)
#define NEMOPATH_CUBIC_BEZIER_FLATTEN_GAPS			(3)

struct nemopath {
	cairo_path_data_t *pathdata;
	int npathdata, spathdata;
	int lastindex;
	cairo_path_data_t lastdata;

	double *pathdist;
	int npathdist, spathdist;

	cairo_path_t *cpath;
	int dirty;

	int index;
	double offset;
	cairo_path_data_t cp;
	cairo_path_data_t lp;

	double extents[4];
	double length;

	double cx[4], cy[4];
};

extern struct nemopath *nemopath_create(void);
extern void nemopath_destroy(struct nemopath *path);

extern int nemopath_append_path(struct nemopath *path, cairo_path_t *cpath);
extern int nemopath_append_cmd(struct nemopath *path, const char *cmd);
extern void nemopath_clear_path(struct nemopath *path);

extern cairo_path_t *nemopath_get_cairo_path(struct nemopath *path);

extern int nemopath_draw_all(struct nemopath *path, cairo_t *cr, double *extents);
extern int nemopath_draw_subpath(struct nemopath *path, cairo_t *cr, double *extents);

extern void nemopath_translate(struct nemopath *path, double x, double y);
extern void nemopath_scale(struct nemopath *path, double sx, double sy);
extern void nemopath_transform(struct nemopath *path, cairo_matrix_t *matrix);

extern int nemopath_flatten(struct nemopath *path);
extern double nemopath_get_position(struct nemopath *path, double offset, double *px, double *py);
extern double nemopath_get_progress(struct nemopath *path, double start, double end, double x, double y);

extern void nemopath_dump(struct nemopath *path, FILE *out);

static inline void nemopath_reset(struct nemopath *path)
{
	path->index = 0;
	path->offset = 0.0f;
}

static inline void nemopath_update(struct nemopath *path)
{
	cairo_path_data_t *data, lp, cp;
	double l;
	int i;

#define	CAIRO_PATH_DISTANCE(a, b)	\
	sqrtf((b.point.x - a.point.x) * (b.point.x - a.point.x) + (b.point.y - a.point.y) * (b.point.y - a.point.y))

	for (i = path->npathdist; i < path->npathdata; i += path->pathdata[i].header.length) {
		data = &path->pathdata[i];

		switch (data->header.type) {
			case CAIRO_PATH_MOVE_TO:
				l = 0.0f;
				lp = data[1];
				cp = data[1];
				ARRAY_APPEND(path->pathdist, path->spathdist, path->npathdist, 0.0f);
				ARRAY_APPEND(path->pathdist, path->spathdist, path->npathdist, 0.0f);
				break;

			case CAIRO_PATH_CLOSE_PATH:
				data = (&lp) - 1;
			case CAIRO_PATH_LINE_TO:
				l = CAIRO_PATH_DISTANCE(cp, data[1]);
				cp = data[1];
				ARRAY_APPEND(path->pathdist, path->spathdist, path->npathdist, l);
				ARRAY_APPEND(path->pathdist, path->spathdist, path->npathdist, 0.0f);
				break;

			case CAIRO_PATH_CURVE_TO:
				l = cubicbezier_length(
						cp.point.x, cp.point.y,
						data[1].point.x, data[1].point.y,
						data[2].point.x, data[2].point.y,
						data[3].point.x, data[3].point.y,
						NEMOPATH_CUBIC_BEZIER_FLATTEN_STEPS);
				cp = data[3];
				ARRAY_APPEND(path->pathdist, path->spathdist, path->npathdist, l);
				ARRAY_APPEND(path->pathdist, path->spathdist, path->npathdist, 0.0f);
				ARRAY_APPEND(path->pathdist, path->spathdist, path->npathdist, 0.0f);
				ARRAY_APPEND(path->pathdist, path->spathdist, path->npathdist, 0.0f);
				break;

			default:
				break;
		}

		path->length += l;
	}
}

static inline void nemopath_get_extents(struct nemopath *path, double *extents)
{
	extents[0] = path->extents[0];
	extents[1] = path->extents[1];
	extents[2] = path->extents[2];
	extents[3] = path->extents[3];
}

static inline double nemopath_get_length(struct nemopath *path)
{
	if (path->npathdist != path->npathdata)
		nemopath_update(path);

	return path->length;
}

static inline double nemopath_get_width(struct nemopath *path)
{
	return path->extents[2];
}

static inline double nemopath_get_height(struct nemopath *path)
{
	return path->extents[3];
}

static inline void nemopath_move_to(struct nemopath *path, double x, double y)
{
	cairo_path_data_t data[2];

	data[0].header.type = CAIRO_PATH_MOVE_TO;
	data[0].header.length = 2;
	ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, data[0]);

	data[1].point.x = x;
	data[1].point.y = y;
	ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, data[1]);
	path->lastindex = path->npathdata - 1;

	path->lastdata = data[1];

	path->dirty = 1;
}

static inline void nemopath_line_to(struct nemopath *path, double x, double y)
{
	cairo_path_data_t data[2];

	data[0].header.type = CAIRO_PATH_LINE_TO;
	data[0].header.length = 2;
	ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, data[0]);

	data[1].point.x = x;
	data[1].point.y = y;
	ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, data[1]);

#define	CAIRO_POINT_MINMAX(cp, minx, miny, maxx, maxy) \
	if (cp.point.x < minx) minx = cp.point.x; \
	if (cp.point.y < miny) miny = cp.point.y;	\
	if (cp.point.x > maxx) maxx = cp.point.x;	\
	if (cp.point.y > maxy) maxy = cp.point.y;

	CAIRO_POINT_MINMAX(path->lastdata, path->extents[0], path->extents[1], path->extents[2], path->extents[3]);
	CAIRO_POINT_MINMAX(data[1], path->extents[0], path->extents[1], path->extents[2], path->extents[3]);

	path->lastdata = data[1];

	path->dirty = 1;
}

static inline void nemopath_curve_to(struct nemopath *path, double x1, double y1, double x2, double y2, double x3, double y3)
{
	cairo_path_data_t data[4];

	data[0].header.type = CAIRO_PATH_CURVE_TO;
	data[0].header.length = 4;
	ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, data[0]);

	data[1].point.x = x1;
	data[1].point.y = y1;
	ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, data[1]);

	data[2].point.x = x2;
	data[2].point.y = y2;
	ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, data[2]);

	data[3].point.x = x3;
	data[3].point.y = y3;
	ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, data[3]);

	CAIRO_POINT_MINMAX(path->lastdata, path->extents[0], path->extents[1], path->extents[2], path->extents[3]);
	CAIRO_POINT_MINMAX(data[1], path->extents[0], path->extents[1], path->extents[2], path->extents[3]);
	CAIRO_POINT_MINMAX(data[2], path->extents[0], path->extents[1], path->extents[2], path->extents[3]);
	CAIRO_POINT_MINMAX(data[3], path->extents[0], path->extents[1], path->extents[2], path->extents[3]);

	path->lastdata = data[3];

	path->dirty = 1;
}

static inline void nemopath_quadratic_curve_to(struct nemopath *path, double x1, double y1, double x2, double y2)
{
	double x0 = path->lastdata.point.x;
	double y0 = path->lastdata.point.y;
	double px0 = x0;
	double py0 = y0;
	double px3 = x2;
	double py3 = y2;
	double px1 = x0 + 2.0f / 3.0f * (x1 - x0);
	double py1 = y0 + 2.0f / 3.0f * (y1 - y0);
	double px2 = x2 + 2.0f / 3.0f * (x1 - x2);
	double py2 = y2 + 2.0f / 3.0f * (y1 - y2);

	nemopath_curve_to(path, px1, py1, px2, py2, px3, py3);
}

static inline void nemopath_arc_segment(struct nemopath *path, double xc, double yc, double th0, double th1, double rx, double ry, double x_axis_rotation)
{
	double x1, y1, x2, y2, x3, y3;
	double t;
	double th_half;
	double f, sinf, cosf;

	f = x_axis_rotation * M_PI / 180.0f;
	sinf = sin(f);
	cosf = cos(f);

	th_half = 0.5f * (th1 - th0);
	t = (8.0f / 3.0f) * sin(th_half * 0.5f) * sin(th_half * 0.5f) / sin(th_half);
	x1 = rx * (cos(th0) - t * sin(th0));
	y1 = ry * (sin(th0) + t * cos(th0));
	x3 = rx * cos(th1);
	y3 = ry * sin(th1);
	x2 = x3 + rx * (t * sin(th1));
	y2 = y3 + ry * (-t * cos(th1));

	nemopath_curve_to(path,
			xc + cosf * x1 - sinf * y1,
			yc + sinf * x1 + cosf * y1,
			xc + cosf * x2 - sinf * y2,
			yc + sinf * x2 + cosf * y2,
			xc + cosf * x3 - sinf * y3,
			yc + sinf * x3 + cosf * y3);
}

static inline void nemopath_arc(struct nemopath *path, double lx, double ly, double rx, double ry, double x_axis_rotation, int large_arc_flag, int sweep_flag, double x, double y)
{
	double f, sinf, cosf;
	double x1, y1, x2, y2;
	double x1_, y1_;
	double cx_, cy_, cx, cy;
	double gamma;
	double theta1, delta_theta;
	double k1, k2, k3, k4, k5;
	int i, nsegs;

	x1 = lx;
	y1 = ly;

	x2 = x;
	y2 = y;

	if (x1 == x2 && y1 == y2)
		return;

	f = x_axis_rotation * M_PI / 180.0f;
	sinf = sin(f);
	cosf = cos(f);

	if ((fabs(rx) < 1e-6) || (fabs(ry) < 1e-6)) {
		nemopath_line_to(path, x, y);
		return;
	}

	if (rx < 0)
		rx = -rx;
	if (ry < 0)
		ry = -ry;

	k1 = (x1 - x2) / 2;
	k2 = (y1 - y2) / 2;

	x1_ = cosf * k1 + sinf * k2;
	y1_ = -sinf * k1 + cosf * k2;

	gamma = (x1_ * x1_) / (rx * rx) + (y1_ * y1_) / (ry * ry);
	if (gamma > 1) {
		rx *= sqrt(gamma);
		ry *= sqrt(gamma);
	}

	k1 = rx * rx * y1_ * y1_ + ry * ry * x1_ * x1_;
	if (k1 == 0)
		return;

	k1 = sqrt(fabs((rx * rx * ry * ry) / k1 - 1));
	if (sweep_flag == large_arc_flag)
		k1 = -k1;

	cx_ = k1 * rx * y1_ / ry;
	cy_ = -k1 * ry * x1_ / rx;

	cx = cosf * cx_ - sinf * cy_ + (x1 + x2) / 2;
	cy = sinf * cx_ + cosf * cy_ + (y1 + y2) / 2;

	k1 = (x1_ - cx_) / rx;
	k2 = (y1_ - cy_) / ry;
	k3 = (-x1_ - cx_) / rx;
	k4 = (-y1_ - cy_) / ry;

	k5 = sqrt(fabs(k1 * k1 + k2 * k2));
	if (k5 == 0)
		return;

	k5 = k1 / k5;
	if (k5 < -1)
		k5 = -1;
	else if (k5 > 1)
		k5 = 1;
	theta1 = acos(k5);
	if (k2 < 0)
		theta1 = -theta1;

	k5 = sqrt(fabs((k1 * k1 + k2 * k2) * (k3 * k3 + k4 * k4)));
	if (k5 == 0)
		return;

	k5 = (k1 * k3 + k2 * k4) / k5;
	if (k5 < -1)
		k5 = -1;
	else if (k5 > 1)
		k5 = 1;
	delta_theta = acos(k5);
	if (k1 * k4 - k3 * k2 < 0)
		delta_theta = -delta_theta;

	if (sweep_flag && delta_theta < 0)
		delta_theta += M_PI * 2;
	else if (!sweep_flag && delta_theta > 0)
		delta_theta -= M_PI * 2;

	nsegs = ceil(fabs(delta_theta / (M_PI * 0.5f + 0.001f)));

	for (i = 0; i < nsegs; i++) {
		nemopath_arc_segment(path, cx, cy,
				theta1 + i * delta_theta / nsegs,
				theta1 + (i + 1) * delta_theta / nsegs,
				rx, ry, x_axis_rotation);
	}
}

static inline void nemopath_arc_to(struct nemopath *path, double cx, double cy, double rx, double ry)
{
	nemopath_arc(path,
			path->lastdata.point.x, path->lastdata.point.y,
			rx, ry,
			0.0f,
			0, 0,
			cx, cy);

	path->lastdata.point.x = cx;
	path->lastdata.point.y = cy;

	path->dirty = 1;
}

static inline void nemopath_close_path(struct nemopath *path)
{
	cairo_path_data_t data;

	data.header.type = CAIRO_PATH_CLOSE_PATH;
	data.header.length = 1;
	ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, data);

	if (path->lastindex >= 0) {
		cairo_path_data_t *moveto = &path->pathdata[path->lastindex];

		nemopath_move_to(path, moveto[0].point.x, moveto[0].point.y);

		CAIRO_POINT_MINMAX(path->lastdata, path->extents[0], path->extents[1], path->extents[2], path->extents[3]);
		CAIRO_POINT_MINMAX(moveto[0], path->extents[0], path->extents[1], path->extents[2], path->extents[3]);

		path->lastdata = moveto[0];
	}

	path->dirty = 1;
}

static inline void nemopath_curve_move_to(struct nemopath *path, double x, double y)
{
	path->cx[2] = path->cx[1] = path->cx[0] = x;
	path->cy[2] = path->cy[1] = path->cy[0] = y;

	nemopath_move_to(path, x, y);
}

static inline void nemopath_curve_line_to(struct nemopath *path, double x, double y)
{
	double sx, sy, ex, ey;

	path->cx[2] = path->cx[1];
	path->cy[2] = path->cy[1];
	path->cx[1] = path->cx[0];
	path->cy[1] = path->cy[0];
	path->cx[0] = x;
	path->cy[0] = y;

	sx = (path->cx[2] + path->cx[1]) * 0.5f;
	sy = (path->cy[2] + path->cy[1]) * 0.5f;
	ex = (path->cx[1] + path->cx[0]) * 0.5f;
	ey = (path->cy[1] + path->cy[0]) * 0.5f;

	nemopath_curve_to(path,
			(sx + 2.0f * path->cx[1]) / 3.0f,
			(sy + 2.0f * path->cy[1]) / 3.0f,
			(ex + 2.0f * path->cx[1]) / 3.0f,
			(ey + 2.0f * path->cy[1]) / 3.0f,
			ex, ey);
}

static inline int nemopath_spiral_curve_to(struct nemopath *path, double x, double y, double startradius, double spaceperloop, double starttheta, double endtheta, double thetastep)
{
	double a = startradius;
	double b = spaceperloop;
	double oldtheta = starttheta;
	double newtheta = starttheta;
	double oldr = a + b * oldtheta;
	double newr = a + b * newtheta;
	double ox = 0.0f, oy = 0.0f, nx = 0.0f, ny = 0.0f;
	double oldslope = 0.0f, newslope = 0.0f;
	double ab, cx, cy;
	double oldintercept, newintercept;
	int firstslope = 1;

	nx = x + oldr * cosf(oldtheta);
	ny = y + oldr * sinf(oldtheta);

	nemopath_move_to(path, nx, ny);

	while (oldtheta < endtheta - thetastep) {
		oldtheta = newtheta;
		newtheta += thetastep;

		oldr = newr;
		newr = a + b * newtheta;

		ox = nx;
		oy = ny;
		nx = x + newr * cosf(newtheta);
		ny = y + newr * sinf(newtheta);

		ab = a + b * newtheta;
		if (firstslope != 0) {
			oldslope = ((b * sinf(oldtheta) + ab * cosf(oldtheta)) / (b * cosf(oldtheta) - ab * sinf(oldtheta)));
			firstslope = 0;
		} else {
			oldslope = newslope;
		}

		newslope = (b * sinf(newtheta) + ab * cosf(newtheta)) / (b * cosf(newtheta) - ab * sinf(newtheta));

		cx = 0.0f;
		cy = 0.0f;

		oldintercept = -(oldslope * oldr * cosf(oldtheta) - oldr * sinf(oldtheta));
		newintercept = -(newslope * newr * cosf(newtheta) - newr * sinf(newtheta));

		if (oldslope == newslope)
			return -1;

		cx = (newintercept - oldintercept) / (oldslope - newslope);
		cy = oldslope * cx + oldintercept;

		cx += x;
		cy += y;

		nemopath_quadratic_curve_to(path, nx, ny, cx, cy);
	}

	return 0;
}

#endif
