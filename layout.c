/*
 * See LICENSE file for copyright and license details.
 */
#include <stdio.h>
#include <string.h>

#include "dwl.h"
#include "layout.h"
#include "client.h"
#include "config.h"

void
arrange(Monitor *m)
{
	Client *c;

	if (!m->wlr_output->enabled)
		return;

	wl_list_for_each(c, &clients, link) {
		if (c->mon == m) {
			wlr_scene_node_set_enabled(&c->scene->node, VISIBLEON(c, m));
			client_set_suspended(c, !VISIBLEON(c, m));
		}
	}

	wlr_scene_node_set_enabled(&m->fullscreen_bg->node,
			!m->isoverview && (c = focustop(m)) && c->isfullscreen);

	if (m->isoverview)
		strncpy(m->ltsymbol, "[O]", LENGTH(m->ltsymbol));
	else
		strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, LENGTH(m->ltsymbol));

	/* We move all floating clients to LyrFloat so they are always on top of tiled ones */
	wl_list_for_each(c, &clients, link) {
		if (c->mon != m || c->scene->node.parent == layers[LyrFS])
			continue;

		wlr_scene_node_reparent(&c->scene->node,
				(c->isfloating && !m->isoverview) ? layers[LyrFloat] : layers[LyrTile]);
	}

	if (m->isoverview)
		overview(m);
	else if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
	motionnotify(0, NULL, 0, 0, 0, 0);
	checkidleinhibitor(NULL);
}

void
fibonacci(Monitor *m, int s)
{
	unsigned int i, n = 0;
	int nx, ny, nw, nh;
	int g = (int)gappx;
	Client *c;

	wl_list_for_each(c, &clients, link)
		if (VISIBLEON(c, m) && !c->isfloating && !c->isfullscreen)
			n++;
	if (n == 0)
		return;

	nx = m->w.x + g;
	ny = m->w.y + g;
	nw = m->w.width - 2 * g;
	nh = m->w.height - 2 * g;

	i = 0;
	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || c->isfloating || c->isfullscreen)
			continue;

		if (i < n - 1) {
			if (s == 0) { /* dwindle: alternate split 50/50 horizontally and vertically */
				if (i % 2 == 0) { /* horizontal split (50% left, 50% right) */
					int half_w = (nw - g) / 2;
					resize(c, (struct wlr_box){.x = nx, .y = ny, .width = half_w, .height = nh}, 0);
					nx += half_w + g;
					nw -= half_w + g;
				} else { /* vertical split (50% top, 50% bottom) */
					int half_h = (nh - g) / 2;
					resize(c, (struct wlr_box){.x = nx, .y = ny, .width = nw, .height = half_h}, 0);
					ny += half_h + g;
					nh -= half_h + g;
				}
			} else { /* spiral: rotate split 50/50 right -> down -> left -> up */
				if (i % 4 == 0) {
					int half_w = (nw - g) / 2;
					resize(c, (struct wlr_box){.x = nx, .y = ny, .width = half_w, .height = nh}, 0);
					nx += half_w + g;
					nw -= half_w + g;
				} else if (i % 4 == 1) {
					int half_h = (nh - g) / 2;
					resize(c, (struct wlr_box){.x = nx, .y = ny, .width = nw, .height = half_h}, 0);
					ny += half_h + g;
					nh -= half_h + g;
				} else if (i % 4 == 2) {
					int half_w = (nw - g) / 2;
					resize(c, (struct wlr_box){.x = nx + half_w + g, .y = ny, .width = nw - half_w - g, .height = nh}, 0);
					nw = half_w;
				} else {
					int half_h = (nh - g) / 2;
					resize(c, (struct wlr_box){.x = nx, .y = ny + half_h + g, .width = nw, .height = nh - half_h - g}, 0);
					nh = half_h;
				}
			}
		} else {
			resize(c, (struct wlr_box){.x = nx, .y = ny, .width = nw, .height = nh}, 0);
		}
		i++;
	}
}

void
dwindle(Monitor *m)
{
	fibonacci(m, 0);
}

void
incnmaster(const Arg *arg)
{
	if (!arg || !selmon)
		return;
	selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
}

void
monocle(Monitor *m)
{
	Client *c;
	int n = 0;
	int g = (int)gappx;
	struct wlr_box gbox;

	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || c->isfloating || c->isfullscreen)
			continue;
		gbox = (struct wlr_box){
			.x = m->w.x + g,
			.y = m->w.y + g,
			.width = m->w.width - 2 * g,
			.height = m->w.height - 2 * g,
		};
		resize(c, gbox, 0);
		n++;
	}
	if (n)
		snprintf(m->ltsymbol, LENGTH(m->ltsymbol), "[%d]", n);
	if ((c = focustop(m)))
		wlr_scene_node_raise_to_top(&c->scene->node);
}

void
setlayout(const Arg *arg)
{
	if (!selmon)
		return;
	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt ^= 1;
	if (arg && arg->v)
		selmon->lt[selmon->sellt] = (Layout *)arg->v;
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, LENGTH(selmon->ltsymbol));
	arrange(selmon);
	printstatus();
}

/* arg > 1.0 will set mfact absolutely */
void
setmfact(const Arg *arg)
{
	float f;

	if (!arg || !selmon || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0f ? arg->f + selmon->mfact : arg->f - 1.0f;
	if (f < 0.1 || f > 0.9)
		return;
	selmon->mfact = f;
	arrange(selmon);
}

void
spiral(Monitor *m)
{
	fibonacci(m, 1);
}

void
tile(Monitor *m)
{
	unsigned int mw, my, ty;
	int i, n = 0, nm, ns;
	int g = (int)gappx;
	int mx, mw_final, sx, sw_final;
	Client *c;

	wl_list_for_each(c, &clients, link)
		if (VISIBLEON(c, m) && !c->isfloating && !c->isfullscreen)
			n++;
	if (n == 0)
		return;

	nm = MIN(n, m->nmaster);
	ns = n - nm;

	if (ns > 0 && nm > 0) {
		mw = (int)roundf((m->w.width - g) * m->mfact);
		mx = m->w.x + g;
		mw_final = mw - g - g / 2;
		sx = m->w.x + mw + g / 2;
		sw_final = m->w.width - mw - g / 2 - g;
	} else if (nm > 0) {
		mw = m->w.width;
		mx = m->w.x + g;
		mw_final = m->w.width - 2 * g;
		sx = mx;
		sw_final = m->w.width - 2 * g;
	} else {
		mw = 0;
		mx = m->w.x + g;
		mw_final = m->w.width - 2 * g;
		sx = mx;
		sw_final = mw_final;
	}

	i = my = ty = 0;
	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || c->isfloating || c->isfullscreen)
			continue;
		if (i < m->nmaster) {
			int h = (m->w.height - my - g * (nm - i + 1)) / (nm - i);
			resize(c, (struct wlr_box){.x = mx, .y = m->w.y + my + g, .width = mw_final, .height = h}, 0);
			my += h + g;
		} else {
			int k = i - m->nmaster;
			int h = (m->w.height - ty - g * (ns - k + 1)) / (ns - k);
			resize(c, (struct wlr_box){.x = sx, .y = m->w.y + ty + g, .width = sw_final, .height = h}, 0);
			ty += h + g;
		}
		i++;
	}
}

/* Overview Mode Grid Layout Algorithm */
void
overview(Monitor *m)
{
	Client *c;
	unsigned int n = 0, i = 0, cols, rows, row, col;
	int inset_x, inset_y, available_w, available_h;
	int tile_w, tile_h, grid_x, grid_y;
	int last_row_cols, last_row_offset;

	wl_list_for_each(c, &clients, link) {
		if (c->mon == m)
			n++;
	}
	if (n == 0)
		return;

	/* Calculate optimal column and row count for N windows */
	cols = (unsigned int)ceil(sqrt((double)n));
	rows = (n + cols - 1) / cols;

	/* Screen margin insets (4%) */
	inset_x = (int)(m->w.width * 0.04f);
	inset_y = (int)(m->w.height * 0.04f);
	available_w = m->w.width - (inset_x * 2);
	available_h = m->w.height - (inset_y * 2);

	tile_w = available_w / cols;
	tile_h = available_h / rows;

	last_row_cols = n % cols;
	if (last_row_cols == 0)
		last_row_cols = cols;
	last_row_offset = (available_w - (last_row_cols * tile_w)) / 2;

	wl_list_for_each(c, &clients, link) {
		if (c->mon != m)
			continue;

		row = i / cols;
		col = i % cols;

		if (row == rows - 1)
			grid_x = m->w.x + inset_x + last_row_offset + col * tile_w;
		else
			grid_x = m->w.x + inset_x + col * tile_w;

		grid_y = m->w.y + inset_y + row * tile_h;

		resize(c, (struct wlr_box){
			.x = grid_x + gappx,
			.y = grid_y + gappx,
			.width = MAX(1, tile_w - 2 * (int)gappx),
			.height = MAX(1, tile_h - 2 * (int)gappx),
		}, 0);
		i++;
	}
}

/* Toggles Overview Mode on/off */
void
toggleoverview(const Arg *arg)
{
	Client *c;
	if (!selmon)
		return;

	if (!selmon->isoverview) {
		/* Save geometries and current tagset when entering overview mode */
		selmon->prevtagset = selmon->tagset[selmon->seltags];
		wl_list_for_each(c, &clients, link) {
			if (c->mon == selmon)
				c->prev = c->geom;
		}
		selmon->isoverview = 1;
		focusclient(focustop(selmon), 1);
	} else {
		/* Exit overview mode and jump tag to selected client */
		Client *sel = focustop(selmon);
		selmon->isoverview = 0;
		if (sel && sel->tags)
			selmon->tagset[selmon->seltags] = sel->tags;
		else
			selmon->tagset[selmon->seltags] = selmon->prevtagset;
		focusclient(sel, 1);
	}

	arrange(selmon);
	printstatus();
}

/* Directional 2D Spatial Focus */
void
focusdir(const Arg *arg)
{
	Client *c = NULL, *tc, *best = NULL;
	double cx, cy, tx, ty, dx, dy;
	double min_dist = 1e18;
	double wrap_dist = 1e18;
	Client *wrap_best = NULL;
	int dir = arg->i;

	if (!selmon || !(c = focustop(selmon)))
		return;

	cx = c->geom.x + c->geom.width / 2.0;
	cy = c->geom.y + c->geom.height / 2.0;

	wl_list_for_each(tc, &clients, link) {
		int in_dir = 0;
		double primary = 0, secondary = 0;
		double wrap_primary = 0, wrap_secondary = 0;
		double dist, wdist;

		if (tc == c || !VISIBLEON(tc, selmon))
			continue;

		tx = tc->geom.x + tc->geom.width / 2.0;
		ty = tc->geom.y + tc->geom.height / 2.0;
		dx = tx - cx;
		dy = ty - cy;

		switch (dir) {
		case WLR_DIRECTION_LEFT:
			if (dx < -1.0) {
				in_dir = 1;
				primary = -dx;
				secondary = fabs(dy);
			}
			break;
		case WLR_DIRECTION_RIGHT:
			if (dx > 1.0) {
				in_dir = 1;
				primary = dx;
				secondary = fabs(dy);
			}
			break;
		case WLR_DIRECTION_UP:
			if (dy < -1.0) {
				in_dir = 1;
				primary = -dy;
				secondary = fabs(dx);
			}
			break;
		case WLR_DIRECTION_DOWN:
			if (dy > 1.0) {
				in_dir = 1;
				primary = dy;
				secondary = fabs(dx);
			}
			break;
		}

		if (in_dir) {
			/* Weight primary distance and penalize orthogonal deviation */
			dist = primary * primary + 3.0 * secondary * secondary;
			if (dist < min_dist) {
				min_dist = dist;
				best = tc;
			}
		} else {
			/* Candidate for wrap-around */
			switch (dir) {
			case WLR_DIRECTION_LEFT:
				wrap_primary = tx; /* rightmost */
				wrap_secondary = fabs(dy);
				break;
			case WLR_DIRECTION_RIGHT:
				wrap_primary = -tx; /* leftmost */
				wrap_secondary = fabs(dy);
				break;
			case WLR_DIRECTION_UP:
				wrap_primary = ty; /* bottommost */
				wrap_secondary = fabs(dx);
				break;
			case WLR_DIRECTION_DOWN:
				wrap_primary = -ty; /* topmost */
				wrap_secondary = fabs(dx);
				break;
			}
			wdist = -wrap_primary * 1000.0 + wrap_secondary;
			if (wdist < wrap_dist) {
				wrap_dist = wdist;
				wrap_best = tc;
			}
		}
	}

	if (!best)
		best = wrap_best;

	if (best)
		focusclient(best, 1);
}

/* Cycles window focus through focus stack */
void
focusstack(const Arg *arg)
{
	Client *c = NULL, *i;
	if (!selmon || !(c = focustop(selmon)))
		return;

	if (selmon->isoverview) {
		focusdir(&(Arg){.i = (arg->i > 0) ? WLR_DIRECTION_RIGHT : WLR_DIRECTION_LEFT});
		return;
	}

	if (arg->i > 0) {
		wl_list_for_each(i, &c->flink, flink) {
			if (&i->flink == &fstack)
				continue;
			if (VISIBLEON(i, selmon)) {
				c = i;
				break;
			}
		}
	} else {
		wl_list_for_each_reverse(i, &c->flink, flink) {
			if (&i->flink == &fstack)
				continue;
			if (VISIBLEON(i, selmon)) {
				c = i;
				break;
			}
		}
	}

	focusclient(c, 1);
}
