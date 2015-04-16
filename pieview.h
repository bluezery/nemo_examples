#ifndef __PIEVIEW_H__
#define __PIEVUEW_H__

#include "nemohelper.h"
#include <taletransition.h>

typedef struct _Pie Pie;
typedef struct _PieView PieView;

void _pieview_set_data(PieView *pieview, void *data);
PieView *_pieview_create(struct pathone *group, int r, int ir);
void _pieview_destroy(PieView *pieview);
int _pieview_count(PieView *pieview);
int _pieview_get_power(PieView *pieview);
Pie *_pieview_get_ap(PieView *pieview, int idx);
void _pieview_resize(PieView *pieview, int r, int ir);
void _pieview_move(PieView *pieview, int x, int y);
void _pieview_change_power(PieView *pieview, int idx, int pow);
void _pieview_set_color(PieView *pieview, int idx, double r, double g, double b, double a);
Pie *_pieview_add_ap(PieView *pieview, int power);
void _pieview_del_ap(PieView *pieview, Pie *pie);
void _pieview_update(PieView *pieview, _Win *win, struct taletransition *trans);

#endif
