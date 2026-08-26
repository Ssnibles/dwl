/*
 * DWL - Layout Engine Module
 * Implements window tiling algorithms (tile, dwindle, spiral, monocle),
 * Overview Mode grid layout, and layout adjustment commands.
 */

#include "dwl.h"

/* Main layout orchestrator: recalculates client geometries and visibility for a monitor */
void
arrange(Monitor *m)
{
	Client *c;

	if (!m->wlr_output->enabled)
		return;

	/* Update client visibility nodes according to current tags or overview mode */
	wl_list_for_each(c, &clients, link) {
		if (c->mon == m) {
			wlr_scene_node_set_enabled(&c->scene->node, VISIBLEON(c, m));
			client_set_suspended(c, !VISIBLEON(c, m));
		}
	}

	/* Update fullscreen background node */
	wlr_scene_node_set_enabled(&m->fullscreen_bg->node,
			!m->isoverview && (c = focustop(m)) && c->isfullscreen);

	/* Set status layout symbol */
	if (m->isoverview)
		strncpy(m->ltsymbol, "[O]", LENGTH(m->ltsymbol));
	else
		strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, LENGTH(m->ltsymbol));

	/* Reparent scene nodes (floating vs tiled vs overview) */
	wl_list_for_each(c, &clients, link) {
		if (c->mon != m || c->scene->node.parent == layers[LyrFS])
			continue;

		wlr_scene_node_reparent(&c->scene->node,
				(c->isfloating && !m->isoverview) ? layers[LyrFloat] : layers[LyrTile]);
	}

	/* Execute active layout algorithm */
	if (m->isoverview)
		overview(m);
	else if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);

	motionnotify(0, NULL, 0, 0, 0, 0);
	checkidleinhibitor(NULL);
}

/* Callback helper to set corner radius on scene buffer nodes */
static void
setcorner_radius_cb(struct wlr_scene_buffer *buffer, int sx, int sy, void *data)
{
	int radius = *(int *)data;
	wlr_scene_buffer_set_corner_radius(buffer, radius, CORNER_LOCATION_ALL);
}

/* Resizes and positions a client window surface and border geometry */
void
resize(Client *c, struct wlr_box geo, int interact)
{
	struct wlr_box *bbox;
	struct wlr_box clip;
	int radius, inner_radius;

	if (!c->mon || !client_surface(c)->mapped)
		return;

	bbox = interact ? &sgeom : &c->mon->w;

	client_set_bounds(c, geo.width, geo.height);
	c->geom = geo;
	applybounds(c, bbox);

	client_get_clip(c, &clip);

	/* Update scene-graph, including borders */
	wlr_scene_node_set_position(&c->scene->node, c->geom.x, c->geom.y);
	wlr_scene_node_set_position(&c->scene_surface->node, c->bw, c->bw);

	if (c->border) {
		wlr_scene_rect_set_size(c->border, c->geom.width, c->geom.height);
		wlr_scene_node_set_position(&c->border->node, 0, 0);

		/* Set corner radius on background border */
		radius = (c->isfullscreen || c->bw == 0) ? 0 : (int)corner_radius;
		wlr_scene_rect_set_corner_radius(c->border, radius, CORNER_LOCATION_ALL);
	} else {
		radius = 0;
	}

	inner_radius = MAX(0, radius - (int)c->bw);
	wlr_scene_node_for_each_buffer(&c->scene_surface->node, setcorner_radius_cb, &inner_radius);

	c->resize = client_set_size(c, c->geom.width - 2 * c->bw,
			c->geom.height - 2 * c->bw);
	wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip);
}

/* Master-and-stack tiling layout algorithm */
void
tile(Monitor *m)
{
	int i, n = 0, h, mw, my, ty;
	Client *c;

	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, m) && !c->isfloating && !c->isfullscreen)
			n++;
	}
	if (n == 0)
		return;

	if (n > m->nmaster)
		mw = m->nmaster ? (int)(m->w.width * m->mfact) : 0;
	else
		mw = m->w.width;

	i = my = ty = 0;
	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || c->isfloating || c->isfullscreen)
			continue;

		if (i < m->nmaster) {
			h = (m->w.height - my) / (MIN(n, m->nmaster) - i);
			resize(c, (struct wlr_box){
				.x = m->w.x + gappx,
				.y = m->w.y + my + gappx,
				.width = MAX(1, (int)mw - 2 * (int)gappx),
				.height = MAX(1, (int)h - 2 * (int)gappx),
			}, 0);
			my += h;
		} else {
			h = (m->w.height - ty) / (n - i);
			resize(c, (struct wlr_box){
				.x = m->w.x + mw + gappx,
				.y = m->w.y + ty + gappx,
				.width = MAX(1, (int)(m->w.width - mw) - 2 * (int)gappx),
				.height = MAX(1, (int)h - 2 * (int)gappx),
			}, 0);
			ty += h;
		}
		i++;
	}
}

/* Monocle layout: maximizes each window to occupy full monitor usable area */
void
monocle(Monitor *m)
{
	Client *c;

	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || c->isfloating || c->isfullscreen)
			continue;

		resize(c, (struct wlr_box){
			.x = m->w.x + gappx,
			.y = m->w.y + gappx,
			.width = MAX(1, m->w.width - 2 * (int)gappx),
			.height = MAX(1, m->w.height - 2 * (int)gappx),
		}, 0);
	}
}

/* Dwindle layout algorithm helper */
void
fibonacci(Monitor *m, int s)
{
	unsigned int i, n = 0, nx, ny, nw, nh;
	Client *c;

	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, m) && !c->isfloating && !c->isfullscreen)
			n++;
	}
	if (n == 0)
		return;

	nx = m->w.x;
	ny = m->w.y;
	nw = m->w.width;
	nh = m->w.height;

	i = 0;
	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || c->isfloating || c->isfullscreen)
			continue;

		if (i < n - 1) {
			if (i % 2 == (s ? 0 : 1)) {
				nh /= 2;
				if (i % 4 == (s ? 0 : 1))
					ny += nh;
			} else {
				nw /= 2;
				if (i % 4 == (s ? 2 : 3))
					nx += nw;
			}
		}

		resize(c, (struct wlr_box){
			.x = nx + gappx,
			.y = ny + gappx,
			.width = MAX(1, (int)nw - 2 * (int)gappx),
			.height = MAX(1, (int)nh - 2 * (int)gappx),
		}, 0);
		i++;
	}
}

/* Dwindle tiling layout */
void
dwindle(Monitor *m)
{
	fibonacci(m, 1);
}

/* Spiral tiling layout */
void
spiral(Monitor *m)
{
	fibonacci(m, 0);
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
	} else {
		/* Exit overview mode and jump tag to selected client */
		selmon->isoverview = 0;
		c = focustop(selmon);
		if (c && c->tags)
			selmon->tagset[selmon->seltags] = c->tags;
		else
			selmon->tagset[selmon->seltags] = selmon->prevtagset;
	}

	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}

/* Adjusts master area factor mfact */
void
setmfact(const Arg *arg)
{
	float f;
	if (!arg || !selmon || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0f ? arg->f + selmon->mfact : arg->f - 1.0f;
	if (f < 0.05f || f > 0.95f)
		return;
	selmon->mfact = f;
	arrange(selmon);
}

/* Increments or decrements number of master windows */
void
incnmaster(const Arg *arg)
{
	if (!selmon)
		return;
	selmon->nmaster = MAX(0, selmon->nmaster + arg->i);
	arrange(selmon);
}

/* Switches layout scheme */
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

/* Promotes focused client window to master position */
void
zoom(const Arg *arg)
{
	Client *c = focustop(selmon);
	if (!c || c->isfloating)
		return;

	if (c == wl_container_of(clients.next, c, link)) {
		Client *next = NULL, *tmp;
		wl_list_for_each(tmp, &clients, link) {
			if (tmp != c && VISIBLEON(tmp, selmon) && !tmp->isfloating) {
				next = tmp;
				break;
			}
		}
		if (next)
			c = next;
	}

	wl_list_remove(&c->link);
	wl_list_insert(&clients, &c->link);
	focusclient(c, 1);
	arrange(selmon);
}

/* Toggles window floating state */
void
togglefloating(const Arg *arg)
{
	Client *c = focustop(selmon);
	if (c && !c->isfullscreen)
		setfloating(c, !c->isfloating);
}

/* Toggles window fullscreen state */
void
togglefullscreen(const Arg *arg)
{
	Client *c = focustop(selmon);
	if (c)
		setfullscreen(c, !c->isfullscreen);
}

/* Cycles window focus through focus stack */
void
focusstack(const Arg *arg)
{
	Client *c = NULL, *i;
	if (!selmon || !(c = focustop(selmon)))
		return;

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
