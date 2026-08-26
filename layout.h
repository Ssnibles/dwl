/*
 * See LICENSE file for copyright and license details.
 */
#ifndef LAYOUT_H
#define LAYOUT_H

#include "dwl.h"

void arrange(Monitor *m);
void tree_layout(Monitor *m);
void bsp_layout(Monitor *m);
void dwindle(Monitor *m);
void fibonacci(Monitor *m, int s);
void master_stack(Monitor *m);
void columns(Monitor *m);
void tile(Monitor *m);
void monocle(Monitor *m);
void spiral(Monitor *m);
void overview(Monitor *m);
void focusdir(const Arg *arg);
void focusstack(const Arg *arg);
void incnmaster(const Arg *arg);
void setlayout(const Arg *arg);
void setmfact(const Arg *arg);
void clearlabeloverlays(Monitor *m);
void destroylabeloverlay(Client *c);
void toggleoverview(const Arg *arg);
void updatelabeloverlays(Monitor *m);
static inline int
spatial_direction_match(double dx, double dy, int dir, double *dist)
{
	double primary = 0, secondary = 0;
	int in_dir = 0;

	switch (dir) {
	case WLR_DIRECTION_LEFT:
		if (dx < -1.0) { in_dir = 1; primary = -dx; secondary = fabs(dy); }
		break;
	case WLR_DIRECTION_RIGHT:
		if (dx > 1.0) { in_dir = 1; primary = dx; secondary = fabs(dy); }
		break;
	case WLR_DIRECTION_UP:
		if (dy < -1.0) { in_dir = 1; primary = -dy; secondary = fabs(dx); }
		break;
	case WLR_DIRECTION_DOWN:
		if (dy > 1.0) { in_dir = 1; primary = dy; secondary = fabs(dx); }
		break;
	}

	if (in_dir && dist)
		*dist = primary * primary + 3.0 * secondary * secondary;

	return in_dir;
}

#endif /* LAYOUT_H */
