/*
 * See LICENSE file for copyright and license details.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "dwl.h"
#include "layout.h"
#include "client.h"
#include "config.h"
#include "tree.h"
#include "workspace.h"

void
arrange(Monitor *m)
{
	Client *c;

	if (!m || !m->wlr_output->enabled)
		return;

	wl_list_for_each(c, &clients, link) {
		if (c->mon == m) {
			int visible = m->isoverview ? (!c->ws || c->ws->id != SCRATCHPAD_WORKSPACE) : VISIBLEON(c, m);
			wlr_scene_node_set_enabled(&c->scene->node, visible);
			client_set_suspended(c, !visible);
		}
	}

	wlr_scene_node_set_enabled(&m->fullscreen_bg->node,
			!m->isoverview && (c = focustop(m)) && c->isfullscreen);

	if (m->isoverview)
		strncpy(m->ltsymbol, "[O]", LENGTH(m->ltsymbol));
	else
		strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, LENGTH(m->ltsymbol));

	/* We move all floating clients and scratchpad overlay clients to LyrFloat so they are on top */
	wl_list_for_each(c, &clients, link) {
		int on_top;
		if (c->mon != m || c->scene->node.parent == layers[LyrFS])
			continue;

		on_top = (c->isfloating || (c->ws && c->ws->id == SCRATCHPAD_WORKSPACE)) && !m->isoverview;
		wlr_scene_node_reparent(&c->scene->node,
				on_top ? layers[LyrFloat] : layers[LyrTile]);
	}

	if (m->scratchpad_showing && !m->isoverview) {
		wl_list_for_each(c, &clients, link) {
			if (c->mon == m && c->ws && c->ws->id == SCRATCHPAD_WORKSPACE)
				wlr_scene_node_raise_to_top(&c->scene->node);
		}
	}

	if (m->isoverview) {
		overview(m);
	} else {
		const Layout *lt;
		if (m->active_workspace) {
			lt = m->active_workspace->layout ? m->active_workspace->layout : m->lt[m->sellt];
			arrange_workspace(m, m->active_workspace, m->w, lt);
			if (cursor_mode != CurResize && cursor_mode != CurMove)
				tree_export_ipc(m->active_workspace);
		}

		if (m->scratchpad_showing) {
			Workspace *scratch_ws = workspace_get_by_id(m, SCRATCHPAD_WORKSPACE);
			if (scratch_ws && scratch_ws->root) {
				struct wlr_box overlay_box = m->w;
				int margin = 32;
				if (overlay_box.width > 2 * margin && overlay_box.height > 2 * margin) {
					overlay_box.x += margin;
					overlay_box.y += margin;
					overlay_box.width -= 2 * margin;
					overlay_box.height -= 2 * margin;
				}
				lt = scratch_ws->layout ? scratch_ws->layout : m->lt[m->sellt];
				arrange_workspace(m, scratch_ws, overlay_box, lt);
			}
		}
	}

	motionnotify(0, NULL, 0, 0, 0, 0);
	checkidleinhibitor(NULL);
}

static inline int
get_workspace_leaves(Workspace *ws, Client **leaves, int max)
{
	if (!ws || !ws->root)
		return 0;
	return node_collect_leaves(ws->root, leaves, max);
}

static inline float
client_get_ratio(const Client *c, int is_horiz)
{
	float r;
	if (!c || !c->node)
		return 0.5f;
	r = is_horiz ? c->node->ratio_h : c->node->ratio_v;
	return clamp_ratio(r);
}

static float
calculate_ratio_factor(Client **leaves, int count, int is_horiz)
{
	(void)count;
	return client_get_ratio(leaves[0], is_horiz);
}

static void
fibonacci_recursive(Client **leaves, int count, struct wlr_box box, int depth, int is_dwindle)
{
	int g = (int)gappx;
	int mode, is_horiz;
	float ratio_factor;
	struct wlr_box b1, b2, gbox;

	if (count <= 0)
		return;

	if (count == 1) {
		gbox = (struct wlr_box){
			.x = box.x + g,
			.y = box.y + g,
			.width = MAX(1, box.width - 2 * g),
			.height = MAX(1, box.height - 2 * g)
		};
		resize(leaves[0], gbox, 0);
		return;
	}

	b1 = box;
	b2 = box;

	mode = is_dwindle ? (depth % 2) : (depth % 4);
	is_horiz = (mode % 2 == 0);
	ratio_factor = calculate_ratio_factor(leaves, count, is_horiz);

	switch (mode) {
	case 0: /* Horizontal split, right box receives remainder */
		{
			int min_w = (int)min_width + 2 * g;
			int w = (int)roundf((float)box.width * ratio_factor);
			if (box.width >= 2 * min_w)
				w = MAX(min_w, MIN(box.width - min_w, w));
			else
				w = MAX(1, MIN(box.width - 1, w));
			b1.width = w;
			b2.x = box.x + w;
			b2.width = MAX(1, box.width - w);
		}
		break;
	case 1: /* Vertical split, bottom box receives remainder */
		{
			int min_h = (int)min_height + 2 * g;
			int h = (int)roundf((float)box.height * ratio_factor);
			if (box.height >= 2 * min_h)
				h = MAX(min_h, MIN(box.height - min_h, h));
			else
				h = MAX(1, MIN(box.height - 1, h));
			b1.height = h;
			b2.y = box.y + h;
			b2.height = MAX(1, box.height - h);
		}
		break;
	case 2: /* Horizontal split, left box receives remainder */
		{
			int min_w = (int)min_width + 2 * g;
			int w = (int)roundf((float)box.width * (1.0f - ratio_factor));
			if (box.width >= 2 * min_w)
				w = MAX(min_w, MIN(box.width - min_w, w));
			else
				w = MAX(1, MIN(box.width - 1, w));
			b1.x = box.x + w;
			b1.width = MAX(1, box.width - w);
			b2.width = w;
		}
		break;
	case 3: /* Vertical split, top box receives remainder */
		{
			int min_h = (int)min_height + 2 * g;
			int h = (int)roundf((float)box.height * (1.0f - ratio_factor));
			if (box.height >= 2 * min_h)
				h = MAX(min_h, MIN(box.height - min_h, h));
			else
				h = MAX(1, MIN(box.height - 1, h));
			b1.y = box.y + h;
			b1.height = MAX(1, box.height - h);
			b2.height = h;
		}
		break;
	}

	gbox = (struct wlr_box){
		.x = b1.x + g,
		.y = b1.y + g,
		.width = MAX(1, b1.width - 2 * g),
		.height = MAX(1, b1.height - 2 * g)
	};
	resize(leaves[0], gbox, 0);

	fibonacci_recursive(leaves + 1, count - 1, b2, depth + 1, is_dwindle);
}

static void
dwindle_box(Monitor *m, Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	int n;
	(void)m;
	n = get_workspace_leaves(ws, leaves, 128);
	if (n > 0)
		fibonacci_recursive(leaves, n, box, 0, 1);
}

static void
spiral_box(Monitor *m, Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	int n;
	(void)m;
	n = get_workspace_leaves(ws, leaves, 128);
	if (n > 0)
		fibonacci_recursive(leaves, n, box, 0, 0);
}

static void
columns_box(Monitor *m, Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	int i, n;
	int g = (int)gappx;
	int x, w;
	float total_ratio = 0.0f;
	int avail_w;

	(void)m;
	n = get_workspace_leaves(ws, leaves, 128);
	if (n == 0)
		return;

	for (i = 0; i < n; i++)
		total_ratio += client_get_ratio(leaves[i], 1);
	if (total_ratio <= 0.0f)
		total_ratio = (float)n;

	x = box.x + g;
	avail_w = box.width - g * (n + 1);

	for (i = 0; i < n; i++) {
		float r = client_get_ratio(leaves[i], 1);
		if (i == n - 1)
			w = box.x + box.width - g - x;
		else
			w = (int)roundf((float)avail_w * (r / total_ratio));
		w = MAX(1, w);
		resize(leaves[i], (struct wlr_box){
			.x = x,
			.y = box.y + g,
			.width = w,
			.height = MAX(1, box.height - 2 * g)
		}, 0);
		x += w + g;
	}
}

static void
tile_box(Monitor *m, Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	unsigned int mw, my, ty;
	int i, n, nm, ns;
	int g = (int)gappx;
	int mx, mw_final, sx, sw_final;
	Client *c;
	float total_m_ratio = 0.0f, total_s_ratio = 0.0f;
	int avail_m_h, avail_s_h;

	n = get_workspace_leaves(ws, leaves, 128);
	if (n == 0)
		return;

	nm = MIN(n, m->nmaster);
	ns = n - nm;

	if (ns > 0 && nm > 0) {
		mw = (int)roundf((box.width - g) * m->mfact);
		mx = box.x + g;
		mw_final = mw - g - g / 2;
		sx = box.x + mw + g / 2;
		sw_final = box.width - mw - g / 2 - g;
	} else if (nm > 0) {
		mw = box.width;
		mx = box.x + g;
		mw_final = box.width - 2 * g;
		sx = mx;
		sw_final = box.width - 2 * g;
	} else {
		mw = 0;
		mx = box.x + g;
		mw_final = box.width - 2 * g;
		sx = mx;
		sw_final = mw_final;
	}

	for (i = 0; i < n; i++) {
		float r = client_get_ratio(leaves[i], 0);
		if (i < nm)
			total_m_ratio += r;
		else
			total_s_ratio += r;
	}
	if (total_m_ratio <= 0.0f) total_m_ratio = (float)nm;
	if (total_s_ratio <= 0.0f) total_s_ratio = (float)ns;

	avail_m_h = box.height - g * (nm + 1);
	avail_s_h = box.height - g * (ns + 1);

	my = ty = 0;
	for (i = 0; i < n; i++) {
		float r;
		c = leaves[i];
		r = client_get_ratio(c, 0);
		if (i < m->nmaster) {
			int h;
			if (i == nm - 1)
				h = box.height - my - g * 2;
			else
				h = (int)roundf((float)avail_m_h * (r / total_m_ratio));
			h = MAX(1, h);
			resize(c, (struct wlr_box){.x = mx, .y = box.y + my + g, .width = mw_final, .height = h}, 0);
			my += h + g;
		} else {
			int h;
			if (i == n - 1)
				h = box.height - ty - g * 2;
			else
				h = (int)roundf((float)avail_s_h * (r / total_s_ratio));
			h = MAX(1, h);
			resize(c, (struct wlr_box){.x = sx, .y = box.y + ty + g, .width = sw_final, .height = h}, 0);
			ty += h + g;
		}
	}
}

static void
monocle_box(Monitor *m, Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	Client *c;
	int i, n;
	int g = (int)gappx;
	struct wlr_box gbox;

	n = get_workspace_leaves(ws, leaves, 128);
	gbox = (struct wlr_box){
		.x = box.x + g,
		.y = box.y + g,
		.width = MAX(1, box.width - 2 * g),
		.height = MAX(1, box.height - 2 * g),
	};

	for (i = 0; i < n; i++)
		resize(leaves[i], gbox, 0);

	if (n && ws == m->active_workspace)
		snprintf(m->ltsymbol, LENGTH(m->ltsymbol), "[%d]", n);
	if ((c = focustop(m)))
		wlr_scene_node_raise_to_top(&c->scene->node);
}

static void
tree_layout_box(Monitor *m, Workspace *ws, struct wlr_box box)
{
	(void)m;
	if (ws && ws->root)
		node_arrange_recursive(ws->root, box);
}

void
arrange_workspace(Monitor *m, Workspace *ws, struct wlr_box box, const Layout *lt)
{
	if (!m || !ws || !ws->root || !lt || !lt->arrange)
		return;

	if (lt->arrange == tree_layout || lt->arrange == bsp_layout) {
		tree_layout_box(m, ws, box);
	} else if (lt->arrange == tile || lt->arrange == master_stack) {
		tile_box(m, ws, box);
	} else if (lt->arrange == monocle) {
		monocle_box(m, ws, box);
	} else if (lt->arrange == dwindle) {
		dwindle_box(m, ws, box);
	} else if (lt->arrange == spiral || lt->arrange == fibonacci) {
		spiral_box(m, ws, box);
	} else if (lt->arrange == columns) {
		columns_box(m, ws, box);
	} else {
		lt->arrange(m);
	}
}

void
tree_layout(Monitor *m)
{
	if (m && m->active_workspace)
		tree_layout_box(m, m->active_workspace, m->w);
}

void
bsp_layout(Monitor *m)
{
	tree_layout(m);
}

void
dwindle(Monitor *m)
{
	if (m && m->active_workspace)
		dwindle_box(m, m->active_workspace, m->w);
}

void
fibonacci(Monitor *m)
{
	spiral(m);
}

void
spiral(Monitor *m)
{
	if (m && m->active_workspace)
		spiral_box(m, m->active_workspace, m->w);
}

void
columns(Monitor *m)
{
	if (m && m->active_workspace)
		columns_box(m, m->active_workspace, m->w);
}

void
master_stack(Monitor *m)
{
	tile(m);
}

void
tile(Monitor *m)
{
	if (m && m->active_workspace)
		tile_box(m, m->active_workspace, m->w);
}

void
monocle(Monitor *m)
{
	if (m && m->active_workspace)
		monocle_box(m, m->active_workspace, m->w);
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
setlayout(const Arg *arg)
{
	Client *c;
	Workspace *ws;

	if (!selmon)
		return;

	c = focustop(selmon);
	ws = (c && c->ws) ? c->ws : selmon->active_workspace;

	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt ^= 1;
	if (arg && arg->v)
		selmon->lt[selmon->sellt] = (Layout *)arg->v;

	if (ws)
		ws->layout = selmon->lt[selmon->sellt];

	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, LENGTH(selmon->ltsymbol));
	arrange(selmon);
	printstatus();
}

void
setmfact(const Arg *arg)
{
	float f;

	if (!arg || !selmon || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0f ? arg->f + selmon->mfact : arg->f - 1.0f;
	if (f < 0.1f || f > 0.9f)
		return;
	selmon->mfact = f;
	arrange(selmon);
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
		if (c->mon == m && (!c->ws || c->ws->id != SCRATCHPAD_WORKSPACE))
			n++;
	}
	if (n == 0) {
		clearlabeloverlays(m);
		return;
	}

	cols = (unsigned int)ceil(sqrt((double)n));
	rows = (n + cols - 1) / cols;

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
		if (c->mon != m || (c->ws && c->ws->id == SCRATCHPAD_WORKSPACE))
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
	updatelabeloverlays(m);
}

static const uint8_t font5x7[26][7] = {
	{0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* A */
	{0x1E, 0x11, 0x1E, 0x11, 0x11, 0x1E, 0x00}, /* B */
	{0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, /* C */
	{0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C}, /* D */
	{0x1F, 0x10, 0x1E, 0x10, 0x10, 0x10, 0x1F}, /* E */
	{0x1F, 0x10, 0x1E, 0x10, 0x10, 0x10, 0x10}, /* F */
	{0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, /* G */
	{0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* H */
	{0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* I */
	{0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}, /* J */
	{0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, /* K */
	{0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, /* L */
	{0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11}, /* M */
	{0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, /* N */
	{0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* O */
	{0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, /* P */
	{0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, /* Q */
	{0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, /* R */
	{0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E}, /* S */
	{0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* T */
	{0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* U */
	{0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, /* V */
	{0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}, /* W */
	{0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, /* X */
	{0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, /* Y */
	{0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, /* Z */
};

void
destroylabeloverlay(Client *c)
{
	if (!c || !c->label_tree)
		return;
	wlr_scene_node_destroy(&c->label_tree->node);
	c->label_tree = NULL;
	c->label = '\0';
}

void
clearlabeloverlays(Monitor *m)
{
	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (c->mon == m)
			destroylabeloverlay(c);
	}
}

void
updatelabeloverlays(Monitor *m)
{
	Client *c;
	int idx = 0;
	if (!m || !m->isoverview) {
		clearlabeloverlays(m);
		return;
	}

	wl_list_for_each(c, &clients, link) {
		int font_idx, start_x, start_y, row, col;
		int num_labels = (int)strlen(overview_labels);
		char lbl, upper_lbl;
		struct wlr_scene_rect *bg_inner, *pixel;

		if (c->mon != m || (c->ws && c->ws->id == SCRATCHPAD_WORKSPACE)) {
			destroylabeloverlay(c);
			continue;
		}

		if (idx >= num_labels) {
			destroylabeloverlay(c);
			continue;
		}

		lbl = overview_labels[idx];
		idx++;

		if (c->label != lbl || !c->label_tree) {
			destroylabeloverlay(c);
			c->label = lbl;
			c->label_tree = wlr_scene_tree_create(layers[LyrFloat]);
			if (c->label_tree) {
				wlr_scene_rect_create(c->label_tree, 36, 36, (float[]){0.1f, 0.1f, 0.15f, 0.95f});
				bg_inner = wlr_scene_rect_create(c->label_tree, 32, 32, (float[]){0.2f, 0.5f, 0.9f, 1.0f});
				wlr_scene_node_set_position(&bg_inner->node, 2, 2);

				upper_lbl = (lbl >= 'a' && lbl <= 'z') ? ('A' + (lbl - 'a')) : lbl;
				font_idx = (upper_lbl >= 'A' && upper_lbl <= 'Z') ? (upper_lbl - 'A') : 0;
				start_x = 10;
				start_y = 7;
				for (row = 0; row < 7; row++) {
					for (col = 0; col < 5; col++) {
						if ((font5x7[font_idx][row] >> (4 - col)) & 1) {
							pixel = wlr_scene_rect_create(c->label_tree, 3, 3, (float[]){1.0f, 1.0f, 1.0f, 1.0f});
							wlr_scene_node_set_position(&pixel->node, start_x + col * 3, start_y + row * 3);
						}
					}
				}
			}
		}

		if (c->label_tree) {
			int badge_x = c->geom.x + (c->geom.width - 36) / 2;
			int badge_y = c->geom.y + (c->geom.height - 36) / 2;
			wlr_scene_node_set_position(&c->label_tree->node, badge_x, badge_y);
			wlr_scene_node_raise_to_top(&c->label_tree->node);
		}
	}
}

void
toggleoverview(const Arg *arg)
{
	Client *c;
	if (!selmon)
		return;

	if (!selmon->isoverview) {
		wl_list_for_each(c, &clients, link) {
			if (c->mon == selmon)
				c->prev = c->geom;
		}
		selmon->overview_prev_client = focustop(selmon);
		selmon->overview_prev_ws = selmon->active_workspace;
		selmon->isoverview = 1;
		focusclient(focustop(selmon), 1);
	} else {
		int confirm = (arg && arg->i == 1);
		Client *sel = confirm ? focustop(selmon) : selmon->overview_prev_client;
		clearlabeloverlays(selmon);
		selmon->isoverview = 0;

		wl_list_for_each(c, &clients, link) {
			if (c->mon == selmon && c->isfloating)
				resize(c, c->prev, 0);
		}

		if (!confirm && selmon->overview_prev_ws) {
			selmon->active_workspace = selmon->overview_prev_ws;
		}

		if (confirm && sel && sel->ws && sel->mon && sel->mon->active_workspace != sel->ws) {
			selmon->active_workspace = sel->ws;
		}

		if (sel && sel->mon == selmon) {
			wlr_cursor_warp_closest(cursor, NULL,
					sel->geom.x + sel->geom.width / 2,
					sel->geom.y + sel->geom.height / 2);
		}
		focusclient(NULL, 0);
		if (sel && sel->mon == selmon)
			focusclient(sel, 1);
		else
			focusclient(focustop(selmon), 1);

		selmon->overview_prev_client = NULL;
		selmon->overview_prev_ws = NULL;
	}

	arrange(selmon);
	printstatus();
}

void
focusdir(const Arg *arg)
{
	Client *c = NULL, *tc, *best = NULL;
	double cx, cy, tx, ty, dx, dy;
	double min_dist = 1e18;
	double wrap_dist = 1e18;
	Client *wrap_best = NULL;
	int dir = arg->i;
	int is_scratch;

	if (!selmon || !(c = focustop(selmon)))
		return;

	is_scratch = (selmon->scratchpad_showing && c->ws && c->ws->id == SCRATCHPAD_WORKSPACE);

	cx = c->geom.x + c->geom.width / 2.0;
	cy = c->geom.y + c->geom.height / 2.0;

	wl_list_for_each(tc, &clients, link) {
		double wrap_primary = 0, wrap_secondary = 0;
		double dist, wdist;

		if (tc == c || (selmon->isoverview ? (tc->mon != selmon || (tc->ws && tc->ws->id == SCRATCHPAD_WORKSPACE)) : !VISIBLEON(tc, selmon)))
			continue;

		if (is_scratch && (!tc->ws || tc->ws->id != SCRATCHPAD_WORKSPACE))
			continue;
		if (!is_scratch && tc->ws && tc->ws->id == SCRATCHPAD_WORKSPACE)
			continue;

		tx = tc->geom.x + tc->geom.width / 2.0;
		ty = tc->geom.y + tc->geom.height / 2.0;
		dx = tx - cx;
		dy = ty - cy;

		if (spatial_direction_match(dx, dy, dir, &dist)) {
			if (dist < min_dist) {
				min_dist = dist;
				best = tc;
			}
		} else {
			switch (dir) {
			case WLR_DIRECTION_LEFT: wrap_primary = tx; wrap_secondary = fabs(dy); break;
			case WLR_DIRECTION_RIGHT: wrap_primary = -tx; wrap_secondary = fabs(dy); break;
			case WLR_DIRECTION_UP: wrap_primary = ty; wrap_secondary = fabs(dx); break;
			case WLR_DIRECTION_DOWN: wrap_primary = -ty; wrap_secondary = fabs(dx); break;
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

void
focusstack(const Arg *arg)
{
	Client *c = NULL, *i;
	int is_scratch;

	if (!selmon || !(c = focustop(selmon)))
		return;

	if (selmon->isoverview) {
		focusdir(&(Arg){.i = (arg->i > 0) ? WLR_DIRECTION_RIGHT : WLR_DIRECTION_LEFT});
		return;
	}

	is_scratch = (selmon->scratchpad_showing && c->ws && c->ws->id == SCRATCHPAD_WORKSPACE);

	if (arg->i > 0) {
		wl_list_for_each(i, &c->flink, flink) {
			if (&i->flink == &fstack)
				continue;
			if (VISIBLEON(i, selmon)) {
				if (is_scratch && (!i->ws || i->ws->id != SCRATCHPAD_WORKSPACE))
					continue;
				if (!is_scratch && i->ws && i->ws->id == SCRATCHPAD_WORKSPACE)
					continue;
				c = i;
				break;
			}
		}
	} else {
		wl_list_for_each_reverse(i, &c->flink, flink) {
			if (&i->flink == &fstack)
				continue;
			if (VISIBLEON(i, selmon)) {
				if (is_scratch && (!i->ws || i->ws->id != SCRATCHPAD_WORKSPACE))
					continue;
				if (!is_scratch && i->ws && i->ws->id == SCRATCHPAD_WORKSPACE)
					continue;
				c = i;
				break;
			}
		}
	}

	focusclient(c, 1);
}
