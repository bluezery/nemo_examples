#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <float.h>
#include <limits.h>
#include <math.h>

#include <nemopath.h>
#include <mischelper.h>

struct nemopath *nemopath_create(void)
{
	struct nemopath *path;

	path = (struct nemopath *)malloc(sizeof(struct nemopath));
	if (path == NULL)
		return NULL;
	memset(path, 0, sizeof(struct nemopath));

	path->pathdata = (cairo_path_data_t *)malloc(sizeof(cairo_path_data_t) * 16);
	if (path->pathdata == NULL)
		goto err1;
	path->npathdata = 0;
	path->spathdata = 16;
	path->lastindex = -1;

	path->pathdist = (double *)malloc(sizeof(double) * 16);
	if (path->pathdist == NULL)
		goto err2;
	path->npathdist = 0;
	path->spathdist = 16;

	path->cpath = NULL;
	path->dirty = 0;

	path->index = 0;
	path->offset = 0.0f;

	path->extents[0] = FLT_MAX;
	path->extents[1] = FLT_MAX;
	path->extents[2] = -FLT_MAX;
	path->extents[3] = -FLT_MAX;
	path->length = 0.0f;

	return path;

err2:
	free(path->pathdata);

err1:
	free(path);

	return NULL;
}

void nemopath_destroy(struct nemopath *path)
{
	if (path->cpath != NULL)
		free(path->cpath);

	free(path->pathdist);
	free(path->pathdata);
	free(path);
}

int nemopath_append_path(struct nemopath *path, cairo_path_t *cpath)
{
	cairo_path_data_t cp, *data, *lp;
	int i;

	if (cpath == NULL || cpath->data == NULL)
		return -1;

	for (i = 0; i < cpath->num_data; i += cpath->data[i].header.length) {
		data = &cpath->data[i];

		switch (data->header.type) {
			case CAIRO_PATH_MOVE_TO:
				cp.header.type = CAIRO_PATH_MOVE_TO;
				cp.header.length = 2;
				ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, cp);
				cp.point.x = data[1].point.x;
				cp.point.y = data[1].point.y;
				ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, cp);
				path->lastindex = path->npathdata - 1;

				path->lastdata = cp;
				break;

			case CAIRO_PATH_CLOSE_PATH:
				cp.header.type = CAIRO_PATH_CLOSE_PATH;
				cp.header.length = 1;
				ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, cp);

				if (path->lastindex >= 0) {
					lp = &path->pathdata[path->lastindex];

					cp.header.type = CAIRO_PATH_MOVE_TO;
					cp.header.length = 2;
					ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, cp);
					cp.point.x = lp->point.x;
					cp.point.y = lp->point.y;
					ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, cp);
					path->lastindex = path->npathdata - 1;

					CAIRO_POINT_MINMAX(path->lastdata, path->extents[0], path->extents[1], path->extents[2], path->extents[3]);
					CAIRO_POINT_MINMAX(cp, path->extents[0], path->extents[1], path->extents[2], path->extents[3]);

					path->lastdata = cp;
				}
				break;

			case CAIRO_PATH_LINE_TO:
				cp.header.type = CAIRO_PATH_LINE_TO;
				cp.header.length = 2;
				ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, cp);
				cp.point.x = data[1].point.x;
				cp.point.y = data[1].point.y;
				ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, cp);

				CAIRO_POINT_MINMAX(path->lastdata, path->extents[0], path->extents[1], path->extents[2], path->extents[3]);
				CAIRO_POINT_MINMAX(data[1], path->extents[0], path->extents[1], path->extents[2], path->extents[3]);

				path->lastdata = cp;
				break;

			case CAIRO_PATH_CURVE_TO:
				cp.header.type = CAIRO_PATH_CURVE_TO;
				cp.header.length = 4;
				ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, cp);
				cp.point.x = data[1].point.x;
				cp.point.y = data[1].point.y;
				ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, cp);
				cp.point.x = data[2].point.x;
				cp.point.y = data[2].point.y;
				ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, cp);
				cp.point.x = data[3].point.x;
				cp.point.y = data[3].point.y;
				ARRAY_APPEND(path->pathdata, path->spathdata, path->npathdata, cp);

				CAIRO_POINT_MINMAX(path->lastdata, path->extents[0], path->extents[1], path->extents[2], path->extents[3]);
				CAIRO_POINT_MINMAX(data[1], path->extents[0], path->extents[1], path->extents[2], path->extents[3]);
				CAIRO_POINT_MINMAX(data[2], path->extents[0], path->extents[1], path->extents[2], path->extents[3]);
				CAIRO_POINT_MINMAX(data[3], path->extents[0], path->extents[1], path->extents[2], path->extents[3]);

				path->lastdata = data[3];
				break;

			default:
				break;
		}
	}

	path->dirty = 1;

	return 0;
}

struct pathcontext {
	struct nemopath *path;

	cairo_path_data_t cp;
	cairo_path_data_t rp;

	char cmd;
	int param;
	int relative;
	double params[7];
};

static void nemopath_cmd_default_xy(struct pathcontext *context, int nparams)
{
	int i;

	if (context->relative) {
		for (i = context->param; i < nparams; i++) {
			if (i > 2)
				context->params[i] = context->params[i - 2];
			else if (i == 1)
				context->params[i] = context->cp.point.y;
			else if (i == 0)
				context->params[i] = context->cp.point.x;
		}
	} else {
		for (i = context->param; i < nparams; i++)
			context->params[i] = 0.0f;
	}
}

static void nemopath_cmd_flush(struct pathcontext *context, int final)
{
	double x1, y1, x2, y2, x3, y3;

	switch (context->cmd) {
		case 'm':
			if (context->param == 2 || final) {
				nemopath_cmd_default_xy(context, 2);
				nemopath_move_to(context->path, context->params[0], context->params[1]);
				context->cp.point.x = context->rp.point.x = context->params[0];
				context->cp.point.y = context->rp.point.y = context->params[1];
				context->param = 0;
				context->cmd = 'l';
			}
			break;

		case 'l':
			if (context->param == 2 || final) {
				nemopath_cmd_default_xy(context, 2);
				nemopath_line_to(context->path, context->params[0], context->params[1]);
				context->cp.point.x = context->rp.point.x = context->params[0];
				context->cp.point.y = context->rp.point.y = context->params[1];
				context->param = 0;
			}
			break;

		case 'c':
			if (context->param == 6 || final) {
				nemopath_cmd_default_xy(context, 6);
				x1 = context->params[0];
				y1 = context->params[1];
				x2 = context->params[2];
				y2 = context->params[3];
				x3 = context->params[4];
				y3 = context->params[5];
				nemopath_curve_to(context->path, x1, y1, x2, y2, x3, y3);
				context->rp.point.x = x2;
				context->rp.point.y = y2;
				context->cp.point.x = x3;
				context->cp.point.y = y3;
				context->param = 0;
			}
			break;

		case 's':
			if (context->param == 4 || final) {
				nemopath_cmd_default_xy(context, 4);
				x1 = 2 * context->cp.point.x - context->rp.point.x;
				y1 = 2 * context->cp.point.y - context->rp.point.y;
				x2 = context->params[0];
				y2 = context->params[1];
				x3 = context->params[2];
				y3 = context->params[3];
				nemopath_curve_to(context->path, x1, y1, x2, y2, x3, y3);
				context->rp.point.x = x2;
				context->rp.point.y = y2;
				context->cp.point.x = x3;
				context->cp.point.y = y3;
				context->param = 0;
			}
			break;

		case 'h':
			if (context->param == 1) {
				nemopath_line_to(context->path, context->params[0], context->cp.point.y);
				context->cp.point.x = context->rp.point.x = context->params[0];
				context->param = 0;
			}
			break;

		case 'v':
			if (context->param == 1) {
				nemopath_line_to(context->path, context->cp.point.x, context->params[0]);
				context->cp.point.y = context->rp.point.y = context->params[0];
				context->param = 0;
			}
			break;

		case 'q':
			if (context->param == 4 || final) {
				nemopath_cmd_default_xy(context, 4);
				x1 = (context->cp.point.x + 2 * context->params[0]) * (1.0f / 3.0f);
				y1 = (context->cp.point.y + 2 * context->params[1]) * (1.0f / 3.0f);
				x3 = context->params[2];
				y3 = context->params[3];
				x2 = (x3 + 2 * context->params[0]) * (1.0f / 3.0f);
				y2 = (y3 + 2 * context->params[1]) * (1.0f / 3.0f);
				nemopath_curve_to(context->path, x1, y1, x2, y2, x3, y3);
				context->rp.point.x = context->params[0];
				context->rp.point.y = context->params[1];
				context->cp.point.x = x3;
				context->cp.point.y = y3;
				context->param = 0;
			}
			break;

		case 't':
			if (context->param == 2 || final) {
				double xc, yc;

				xc = 2 * context->cp.point.x - context->rp.point.x;
				yc = 2 * context->cp.point.y - context->rp.point.y;
				x1 = (context->cp.point.x + 2 * xc) * (1.0f / 3.0f);
				y1 = (context->cp.point.y + 2 * yc) * (1.0f / 3.0f);
				x3 = context->params[0];
				y3 = context->params[1];
				x2 = (x3 + 2 * xc) * (1.0f / 3.0f);
				y2 = (y3 + 2 * yc) * (1.0f / 3.0f);
				nemopath_curve_to(context->path, x1, y1, x2, y2, x3, y3);
				context->rp.point.x = xc;
				context->rp.point.y = yc;
				context->cp.point.x = x3;
				context->cp.point.y = y3;
				context->param = 0;
			} else if (final) {
				if (context->param > 2) {
					nemopath_cmd_default_xy(context, 4);
					x1 = (context->cp.point.x + 2 * context->params[0]) * (1.0f / 3.0f);
					y1 = (context->cp.point.y + 2 * context->params[1]) * (1.0f / 3.0f);
					x3 = context->params[2];
					y3 = context->params[3];
					x2 = (x3 + 2 * context->params[0]) * (1.0f / 3.0f);
					y2 = (y3 + 2 * context->params[1]) * (1.0f / 3.0f);
					nemopath_curve_to(context->path, x1, y1, x2, y2, x3, y3);
					context->rp.point.x = context->params[0];
					context->rp.point.y = context->params[1];
					context->cp.point.x = x3;
					context->cp.point.y = y3;
				} else {
					nemopath_cmd_default_xy(context, 2);
					nemopath_line_to(context->path, context->params[0], context->params[1]);
					context->cp.point.x = context->rp.point.x = context->params[0];
					context->cp.point.y = context->rp.point.y = context->params[1];
				}
				context->param = 0;
			}
			break;

		case 'a':
			if (context->param == 7 || final) {
				nemopath_arc(context->path,
						context->cp.point.x, context->cp.point.y,
						context->params[0], context->params[1], context->params[2],
						context->params[3], context->params[4], context->params[5], context->params[6]);
				context->cp.point.x = context->params[5];
				context->cp.point.y = context->params[6];
				context->param = 0;
			}
			break;

		default:
			context->param = 0;
	}
}

static void nemopath_cmd_end_of_number(struct pathcontext *context, double val, int sign, int exp_sign, int exp)
{
	val *= sign * pow(10, exp_sign * exp);

	if (context->relative) {
		switch (context->cmd) {
			case 'l':
			case 'm':
			case 'c':
			case 's':
			case 'q':
			case 't':
				if ((context->param & 1) == 0)
					val += context->cp.point.x;
				else if ((context->param & 1) == 1)
					val += context->cp.point.y;
				break;

			case 'a':
				if (context->param == 5)
					val += context->cp.point.x;
				else if (context->param == 6)
					val += context->cp.point.y;
				break;

			case 'h':
				val += context->cp.point.x;
				break;

			case 'v':
				val += context->cp.point.y;
				break;
		}
	}

	context->params[context->param++] = val;

	nemopath_cmd_flush(context, 0);
}

static void nemopath_cmd_parse(struct pathcontext *context, const char *data)
{
	int i = 0;
	double val = 0.0f;
	char c = 0;
	int in_num = 0;
	int in_frac = 0;
	int in_exp = 0;
	int exp_wait_sign = 0;
	int sign = 0;
	int exp = 0;
	int exp_sign = 0;
	double frac = 0.0f;

	in_num = 0;
	for (i = 0; ; i++) {
		c = data[i];
		if (c >= '0' && c <= '9') {
			if (in_num) {
				if (in_exp) {
					exp = (exp * 10) + c - '0';
					exp_wait_sign = 0;
				} else if (in_frac) {
					val += (frac *= 0.1) * (c - '0');
				} else {
					val = (val * 10) + c - '0';
				}
			} else {
				in_num = 1;
				in_frac = 0;
				in_exp = 0;
				exp = 0;
				exp_sign = 1;
				exp_wait_sign = 0;
				val = c - '0';
				sign = 1;
			}
		} else if (c == '.') {
			if (!in_num) {
				in_frac = 1;
				val = 0;
			} else if (in_frac) {
				nemopath_cmd_end_of_number(context, val, sign, exp_sign, exp);
				in_frac = 0;
				in_exp = 0;
				exp = 0;
				exp_sign = 1;
				exp_wait_sign = 0;
				val = 0;
				sign = 1;
			} else {
				in_frac = 1;
			}
			in_num = 1;
			frac = 1;
		} else if ((c == 'E' || c == 'e') && in_num) {
			in_exp = 1;
			exp_wait_sign = 1;
			exp = 0;
			exp_sign = 1;
		} else if ((c == '+' || c == '-') && in_exp) {
			exp_sign = c == '+' ? 1 : -1;
		} else if (in_num) {
			nemopath_cmd_end_of_number(context, val, sign, exp_sign, exp);
			in_num = 0;
		}

		if (c == '\0') {
			break;
		} else if ((c == '+' || c == '-') && !exp_wait_sign) {
			sign = c == '+' ? 1 : -1;
			val = 0;
			in_num = 1;
			in_frac = 0;
			in_exp = 0;
			exp = 0;
			exp_sign = 1;
			exp_wait_sign = 0;
		} else if (c == 'z' || c == 'Z') {
			if (context->param)
				nemopath_cmd_flush(context, 1);
			nemopath_close_path(context->path);

			context->cp = context->rp = context->path->pathdata[context->path->npathdata - 1];
		} else if (c >= 'A' && c <= 'Z' && c != 'E') {
			if (context->param)
				nemopath_cmd_flush(context, 1);
			context->cmd = c + 'a' - 'A';
			context->relative = 0;
		} else if (c >= 'a' && c <= 'z' && c != 'e') {
			if (context->param)
				nemopath_cmd_flush(context, 1);
			context->cmd = c;
			context->relative = 1;
		}
	}

	if (context->param)
		nemopath_cmd_flush(context, 1);
}

int nemopath_append_cmd(struct nemopath *path, const char *cmd)
{
	struct pathcontext context;

	context.path = path;

	context.cp.point.x = 0.0f;
	context.cp.point.y = 0.0f;
	context.cmd = 0;
	context.param = 0;

	nemopath_cmd_parse(&context, cmd);

	return 0;
}

void nemopath_clear_path(struct nemopath *path)
{
	path->npathdata = 0;
	path->lastindex = -1;

	path->dirty = 1;

	path->extents[0] = FLT_MAX;
	path->extents[1] = FLT_MAX;
	path->extents[2] = -FLT_MAX;
	path->extents[3] = -FLT_MAX;
}

cairo_path_t *nemopath_get_cairo_path(struct nemopath *path)
{
	if (path->dirty != 0) {
		if (path->cpath != NULL) {
			free(path->cpath);

			path->cpath = NULL;
		}

		path->dirty = 0;
	}

	if (path->cpath == NULL) {
		cairo_path_t *cpath;

		cpath = (cairo_path_t *)malloc(sizeof(cairo_path_t));
		if (cpath == NULL)
			return NULL;

		cpath->status = CAIRO_STATUS_SUCCESS;
		cpath->data = path->pathdata;
		cpath->num_data = path->npathdata;

		path->cpath = cpath;
	}

	return path->cpath;
}

int nemopath_draw_all(struct nemopath *path, cairo_t *cr, double *extents)
{
	cairo_path_t *cpath = nemopath_get_cairo_path(path);

	cairo_append_path(cr, cpath);

	extents[0] = path->extents[0];
	extents[1] = path->extents[1];
	extents[2] = path->extents[2];
	extents[3] = path->extents[3];

	return 0;
}

int nemopath_draw_subpath(struct nemopath *path, cairo_t *cr, double *extents)
{
	cairo_path_data_t *data, lp, cp;
	int i;

	extents[0] = FLT_MAX;
	extents[1] = FLT_MAX;
	extents[2] = -FLT_MAX;
	extents[3] = -FLT_MAX;

	if (path->index > 0) {
		cp = path->cp;

		cairo_move_to(cr, cp.point.x, cp.point.y);
	}

	for (i = path->index; i < path->npathdata; i += path->pathdata[i].header.length) {
		data = &path->pathdata[i];

		switch (data->header.type) {
			case CAIRO_PATH_MOVE_TO:
				cairo_move_to(cr, data[1].point.x, data[1].point.y);

				lp = data[1];
				cp = data[1];
				break;

			case CAIRO_PATH_CLOSE_PATH:
				data = (&lp) - 1;
			case CAIRO_PATH_LINE_TO:
				cairo_line_to(cr, data[1].point.x, data[1].point.y);

				CAIRO_POINT_MINMAX(cp, extents[0], extents[1], extents[2], extents[3]);
				CAIRO_POINT_MINMAX(data[1], extents[0], extents[1], extents[2], extents[3]);

				cp = data[1];
				break;

			case CAIRO_PATH_CURVE_TO:
				cairo_curve_to(cr,
						data[1].point.x, data[1].point.y,
						data[2].point.x, data[2].point.y,
						data[3].point.x, data[3].point.y);

				CAIRO_POINT_MINMAX(cp, extents[0], extents[1], extents[2], extents[3]);
				CAIRO_POINT_MINMAX(data[1], extents[0], extents[1], extents[2], extents[3]);
				CAIRO_POINT_MINMAX(data[2], extents[0], extents[1], extents[2], extents[3]);
				CAIRO_POINT_MINMAX(data[3], extents[0], extents[1], extents[2], extents[3]);

				cp = data[3];
				break;

			default:
				return -1;
		}
	}

	path->cp = cp;
	path->index = i;

	return 0;
}

void nemopath_translate(struct nemopath *path, double x, double y)
{
	cairo_path_data_t *data;
	int i;

	for (i = 0; i < path->npathdata; i += path->pathdata[i].header.length) {
		data = &path->pathdata[i];

		switch (data->header.type) {
			case CAIRO_PATH_MOVE_TO:
				data[1].point.x += x;
				data[1].point.y += y;
				break;

			case CAIRO_PATH_CLOSE_PATH:
				break;

			case CAIRO_PATH_LINE_TO:
				data[1].point.x += x;
				data[1].point.y += y;
				break;

			case CAIRO_PATH_CURVE_TO:
				data[1].point.x += x;
				data[1].point.y += y;
				data[2].point.x += x;
				data[2].point.y += y;
				data[3].point.x += x;
				data[3].point.y += y;
				break;

			default:
				break;
		}
	}

	path->extents[0] += x;
	path->extents[1] += y;
	path->extents[2] += x;
	path->extents[3] += y;
}

void nemopath_scale(struct nemopath *path, double sx, double sy)
{
	cairo_path_data_t *data;
	int i;

	for (i = 0; i < path->npathdata; i += path->pathdata[i].header.length) {
		data = &path->pathdata[i];

		switch (data->header.type) {
			case CAIRO_PATH_MOVE_TO:
				data[1].point.x *= sx;
				data[1].point.y *= sy;
				break;

			case CAIRO_PATH_CLOSE_PATH:
				break;

			case CAIRO_PATH_LINE_TO:
				data[1].point.x *= sx;
				data[1].point.y *= sy;
				break;

			case CAIRO_PATH_CURVE_TO:
				data[1].point.x *= sx;
				data[1].point.y *= sy;
				data[2].point.x *= sx;
				data[2].point.y *= sy;
				data[3].point.x *= sx;
				data[3].point.y *= sy;
				break;

			default:
				break;
		}
	}

	path->extents[0] *= sx;
	path->extents[1] *= sy;
	path->extents[2] *= sx;
	path->extents[3] *= sy;
}

void nemopath_transform(struct nemopath *path, cairo_matrix_t *matrix)
{
	cairo_path_data_t *data;
	int i;

	for (i = 0; i < path->npathdata; i += path->pathdata[i].header.length) {
		data = &path->pathdata[i];

		switch (data->header.type) {
			case CAIRO_PATH_MOVE_TO:
				cairo_matrix_transform_point(matrix,
						&data[1].point.x,
						&data[1].point.y);
				break;

			case CAIRO_PATH_CLOSE_PATH:
				break;

			case CAIRO_PATH_LINE_TO:
				cairo_matrix_transform_point(matrix,
						&data[1].point.x,
						&data[1].point.y);
				break;

			case CAIRO_PATH_CURVE_TO:
				cairo_matrix_transform_point(matrix,
						&data[1].point.x,
						&data[1].point.y);
				cairo_matrix_transform_point(matrix,
						&data[2].point.x,
						&data[2].point.y);
				cairo_matrix_transform_point(matrix,
						&data[3].point.x,
						&data[3].point.y);
				break;

			default:
				break;
		}
	}

	cairo_matrix_transform_point(matrix, &path->extents[0], &path->extents[1]);
	cairo_matrix_transform_point(matrix, &path->extents[2], &path->extents[3]);
}

int nemopath_flatten(struct nemopath *path)
{
	cairo_path_data_t *pathdata;
	cairo_path_data_t *data;
	double length;
	int npathdata;
	int i;

	pathdata = path->pathdata;
	npathdata = path->npathdata;

	path->pathdata = (cairo_path_data_t *)malloc(sizeof(cairo_path_data_t) * 32);
	if (path->pathdata == NULL)
		return -1;
	path->npathdata = 0;
	path->spathdata = 32;
	path->lastindex = -1;
	path->dirty = 1;

	path->extents[0] = FLT_MAX;
	path->extents[1] = FLT_MAX;
	path->extents[2] = -FLT_MAX;
	path->extents[3] = -FLT_MAX;

	for (i = 0; i < npathdata; i += pathdata[i].header.length) {
		data = &pathdata[i];

		switch (data->header.type) {
			case CAIRO_PATH_MOVE_TO:
				nemopath_move_to(path, data[1].point.x, data[1].point.y);
				break;

			case CAIRO_PATH_CLOSE_PATH:
				nemopath_close_path(path);
				break;

			case CAIRO_PATH_LINE_TO:
				nemopath_line_to(path, data[1].point.x, data[1].point.y);
				break;

			case CAIRO_PATH_CURVE_TO:
				length = cubicbezier_length(
						path->lastdata.point.x, path->lastdata.point.y,
						data[1].point.x, data[1].point.y,
						data[2].point.x, data[2].point.y,
						data[3].point.x, data[3].point.y,
						NEMOPATH_CUBIC_BEZIER_FLATTEN_STEPS);

				if (length > 0.0f) {
					double x0 = path->lastdata.point.x;
					double y0 = path->lastdata.point.y;
					double x1 = data[1].point.x;
					double y1 = data[1].point.y;
					double x2 = data[2].point.x;
					double y2 = data[2].point.y;
					double x3 = data[3].point.x;
					double y3 = data[3].point.y;
					double t, cx, cy;
					int steps = ceil(length / NEMOPATH_CUBIC_BEZIER_FLATTEN_GAPS);
					int s;

					for (s = 1; s <= steps; s++) {
						t = (double)s / (double)steps;

						cx = cubicbezier_point(t, x0, x1, x2, x3);
						cy = cubicbezier_point(t, y0, y1, y2, y3);

						nemopath_line_to(path, cx, cy);
					}
				}
				break;

			default:
				break;
		}
	}

	free(pathdata);

	return 0;
}

double nemopath_get_position(struct nemopath *path, double offset, double *px, double *py)
{
	cairo_path_data_t *data, lp, cp;
	double p, l;
	double ox, oy, pr = 0.0f;
	int i;

	if (path->npathdist != path->npathdata)
		nemopath_update(path);

	if (path->offset <= offset) {
		offset = offset - path->offset;
		lp = path->lp;
		cp = path->cp;
	} else {
		path->index = 0;
		path->offset = 0.0f;
	}

	for (i = path->index; i < path->npathdata && offset >= 0.0f; i += path->pathdata[i].header.length) {
		data = &path->pathdata[i];

		switch (data->header.type) {
			case CAIRO_PATH_MOVE_TO:
				l = 0.0f;
				lp = data[1];
				cp = data[1];
				path->lp = data[1];
				path->cp = data[1];
				break;

			case CAIRO_PATH_CLOSE_PATH:
				data = (&lp) - 1;
			case CAIRO_PATH_LINE_TO:
				l = path->pathdist[i];
				if (offset > l) {
					path->index = i + path->pathdata[i].header.length;
					path->offset += l;
					path->cp = data[1];
					p = 1.0f;
				} else {
					p = offset / l;
				}
				*px = (data[1].point.x - cp.point.x) * p + cp.point.x;
				*py = (data[1].point.y - cp.point.y) * p + cp.point.y;
				pr = atan2(cp.point.x - data[1].point.x, -(cp.point.y - data[1].point.y));
				cp = data[1];
				break;

			case CAIRO_PATH_CURVE_TO:
				l = path->pathdist[i];
				if (offset > l) {
					path->index = i + path->pathdata[i].header.length;
					path->offset += l;
					path->cp = data[3];
					p = 1.0f;
				} else {
					p = offset / l;
				}
				*px = cubicbezier_point(p,
						cp.point.x,
						data[1].point.x,
						data[2].point.x,
						data[3].point.x);
				*py = cubicbezier_point(p,
						cp.point.y,
						data[1].point.y,
						data[2].point.y,
						data[3].point.y);
				ox = cubicbezier_point(p - (double)NEMOPATH_CUBIC_BEZIER_FLATTEN_GAPS / l,
						cp.point.x,
						data[1].point.x,
						data[2].point.x,
						data[3].point.x);
				oy = cubicbezier_point(p - (double)NEMOPATH_CUBIC_BEZIER_FLATTEN_GAPS / l,
						cp.point.y,
						data[1].point.y,
						data[2].point.y,
						data[3].point.y);
				pr = atan2(ox - *px, -(oy - *py));
				cp = data[3];
				break;

			default:
				break;
		}

		offset -= l;
	}

	return pr;
}

double nemopath_get_progress(struct nemopath *path, double start, double end, double x, double y)
{
	double px, py;
	double dx, dy;
	double dist;
	double mindist = FLT_MAX;
	double min = -1.0f;
	double offset;

	if (path->npathdist != path->npathdata)
		nemopath_update(path);

	for (offset = start; nemopath_get_position(path, offset, &px, &py) == 0 && offset <= end; offset = offset + 1.0f) {
		dx = x - px;
		dy = y - py;

		dist = sqrtf(dx * dx + dy * dy);
		if (dist < mindist) {
			mindist = dist;
			min = offset;
		}
	}

	return min;
}

void nemopath_dump(struct nemopath *path, FILE *out)
{
	cairo_path_data_t *data;
	int i;

	for (i = 0; i < path->npathdata; i += path->pathdata[i].header.length) {
		data = &path->pathdata[i];

		switch (data->header.type) {
			case CAIRO_PATH_MOVE_TO:
				fprintf(out, "[MOVE] (%f, %f)\n",
						data[1].point.x, data[1].point.y);
				break;

			case CAIRO_PATH_CLOSE_PATH:
				fprintf(out, "[CLOSE]\n");
				break;

			case CAIRO_PATH_LINE_TO:
				fprintf(out, "[LINE] (%f, %f)\n",
						data[1].point.x, data[1].point.y);
				break;

			case CAIRO_PATH_CURVE_TO:
				fprintf(out, "[CURVE] (%f, %f) (%f, %f) (%f, %f)\n",
						data[1].point.x, data[1].point.y,
						data[2].point.x, data[2].point.y,
						data[3].point.x, data[3].point.y);
				break;

			default:
				break;
		}
	}
}
