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
#include "workspace.h"

static void clearlabeloverlays(const Monitor *m);
static void updatelabeloverlays(const Monitor *m);
static void arrange_workspace(Monitor *m, const Workspace *ws, struct wlr_box box, const Layout *lt);
static void overview(Monitor *m);

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

	/* Move clients to appropriate scene graph layers */
	wl_list_for_each(c, &clients, link) {
		int on_top;

		if (c->mon != m)
			continue;

		if (!m->isoverview && client_has_fullscreen_ancestor(c)) {
			wlr_scene_node_reparent(&c->scene->node, layers[LyrFS]);
		} else {
			on_top = (c->isfloating || (c->ws && c->ws->id == SCRATCHPAD_WORKSPACE)) && !m->isoverview;
			wlr_scene_node_reparent(&c->scene->node,
					on_top ? layers[LyrFloat] : layers[LyrTile]);
		}
	}

	if (m->scratchpad_showing && !m->isoverview) {
		wlr_scene_node_set_position(&m->scratchpad_bg->node, m->m.x, m->m.y);
		wlr_scene_rect_set_size(m->scratchpad_bg, m->m.width, m->m.height);
		wlr_scene_node_set_enabled(&m->scratchpad_bg->node, 1);
		wlr_scene_node_raise_to_top(&m->scratchpad_bg->node);

		wl_list_for_each(c, &clients, link) {
			if (c->mon == m && c->ws && c->ws->id == SCRATCHPAD_WORKSPACE)
				wlr_scene_node_raise_to_top(&c->scene->node);
		}
	} else if (m->scratchpad_bg) {
		wlr_scene_node_set_enabled(&m->scratchpad_bg->node, 0);
	}

	if (m->isoverview) {
		overview(m);
	} else {
		wl_list_for_each(c, &clients, link) {
			if (c->mon == m && c->isfullscreen && VISIBLEON(c, m))
				resize(c, m->m, 0);
		}

		const Layout *lt;
		if (m->active_workspace) {
			lt = resolve_layout(NULL, m, NULL);
			arrange_workspace(m, m->active_workspace, m->w, lt);
		}

		if (m->scratchpad_showing) {
			Workspace *scratch_ws = workspace_get_by_id(m, SCRATCHPAD_WORKSPACE);
			if (scratch_ws) {
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

	if (cursor_mode != CurResize && cursor_mode != CurMove)
		motionnotify(0, NULL, 0, 0, 0, 0);
	checkidleinhibitor(NULL);
}

static inline int
get_workspace_leaves(const Workspace *ws, Client **leaves, int max)
{
	Client *c;
	int count = 0;
	if (!ws)
		return 0;
	wl_list_for_each(c, &clients, link) {
		if (c->ws == ws && !c->isfloating && !c->isfullscreen && count < max)
			leaves[count++] = c;
	}
	return count;
}

static void
dwindle_recursive(Client **leaves, int count, struct wlr_box box, int depth, const Workspace *ws)
{
	int g = (int)gappx;
	struct wlr_box b1, b2;
	float fact;
	int dir = ws ? ws->dir : 0; /* 0: Left, 1: Top, 2: Right, 3: Bottom */

	if (count <= 0)
		return;

	if (count == 1) {
		resize(leaves[0], (struct wlr_box){
			.x = box.x + g,
			.y = box.y + g,
			.width = MAX(1, box.width - 2 * g),
			.height = MAX(1, box.height - 2 * g)
		}, 0);
		return;
	}

	int n1 = count / 2;
	int n2 = count - n1;

	int split_dir;
	if (depth == 0) {
		split_dir = dir;
	} else if ((dir % 2 == 0 && depth % 2 == 0) || (dir % 2 == 1 && depth % 2 != 0)) {
		split_dir = (dir % 2 == 0) ? 0 : 1;
	} else {
		split_dir = (dir % 2 == 0) ? 1 : 0;
	}

	if (depth == 0) {
		fact = (ws && ws->mfact > 0) ? ws->mfact : (selmon ? selmon->mfact : 0.5f);
	} else {
		float cfirst = 0.0f, crest = 0.0f;
		for (int i = 0; i < n1; i++)
			cfirst += leaves[i]->cfact > 0 ? leaves[i]->cfact : 1.0f;
		for (int i = n1; i < count; i++)
			crest += leaves[i]->cfact > 0 ? leaves[i]->cfact : 1.0f;
		fact = (cfirst > 0 && crest > 0) ? (cfirst / (cfirst + crest)) : 0.5f;
		if (fact < 0.1f) fact = 0.1f;
		if (fact > 0.9f) fact = 0.9f;
	}

	if (split_dir == 0) { /* Left: b1 on left, b2 on right */
		int w = (int)roundf((float)(box.width + g) * fact);
		w = MAX(1, MIN(box.width - 1, w));
		b1 = (struct wlr_box){ .x = box.x, .y = box.y, .width = w, .height = box.height };
		b2 = (struct wlr_box){ .x = box.x + w - g, .y = box.y, .width = MAX(1, box.width - w + g), .height = box.height };
	} else if (split_dir == 1) { /* Top: b1 on top, b2 on bottom */
		int h = (int)roundf((float)(box.height + g) * fact);
		h = MAX(1, MIN(box.height - 1, h));
		b1 = (struct wlr_box){ .x = box.x, .y = box.y, .width = box.width, .height = h };
		b2 = (struct wlr_box){ .x = box.x, .y = box.y + h - g, .width = box.width, .height = MAX(1, box.height - h + g) };
	} else if (split_dir == 2) { /* Right: b1 on right, b2 on left */
		int w = (int)roundf((float)(box.width + g) * fact);
		w = MAX(1, MIN(box.width - 1, w));
		b1 = (struct wlr_box){ .x = box.x + box.width - w, .y = box.y, .width = w, .height = box.height };
		b2 = (struct wlr_box){ .x = box.x, .y = box.y, .width = MAX(1, box.width - w + g), .height = box.height };
	} else { /* Bottom: b1 on bottom, b2 on top */
		int h = (int)roundf((float)(box.height + g) * fact);
		h = MAX(1, MIN(box.height - 1, h));
		b1 = (struct wlr_box){ .x = box.x, .y = box.y + box.height - h, .width = box.width, .height = h };
		b2 = (struct wlr_box){ .x = box.x, .y = box.y, .width = box.width, .height = MAX(1, box.height - h + g) };
	}

	dwindle_recursive(leaves, n1, b1, depth + 1, ws);
	dwindle_recursive(leaves + n1, n2, b2, depth + 1, ws);
}


static void
dwindle_box(Monitor *m, const Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	int n;
	(void)m;
	n = get_workspace_leaves(ws, leaves, 128);
	if (n > 0)
		dwindle_recursive(leaves, n, box, 0, ws);
}

static void
right_tile_box(Monitor *m, const Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	int g = (int)gappx;
	int n = get_workspace_leaves(ws, leaves, 128);
	if (n == 0) return;

	int ws_nmaster = ws ? ws->nmaster : m->nmaster;
	float ws_mfact = ws ? ws->mfact : m->mfact;
	int nm = MIN(n, ws_nmaster);
	int ns = n - nm;

	float mfact_sum = 0.0f, sfact_sum = 0.0f;
	for (int i = 0; i < n; i++) {
		if (i < nm) mfact_sum += leaves[i]->cfact > 0 ? leaves[i]->cfact : 1.0f;
		else sfact_sum += leaves[i]->cfact > 0 ? leaves[i]->cfact : 1.0f;
	}
	if (mfact_sum <= 0.0f) mfact_sum = 1.0f;
	if (sfact_sum <= 0.0f) sfact_sum = 1.0f;

	int mx, mw_final, sx, sw_final;
	if (ns > 0 && nm > 0) {
		int mw = (int)roundf((box.width - g) * ws_mfact);
		sx = box.x + g;
		sw_final = box.width - mw - g / 2 - g;
		mx = box.x + box.width - mw + g / 2;
		mw_final = mw - g - g / 2;
	} else {
		mx = sx = box.x + g;
		mw_final = sw_final = box.width - 2 * g;
	}

	int my = 0, ty = 0;
	for (int i = 0; i < n; i++) {
		Client *c = leaves[i];
		float cf = c->cfact > 0 ? c->cfact : 1.0f;
		if (i < nm) {
			int avail_h = box.height - g * (nm + 1);
			int h = (i == nm - 1) ? (box.height - my - g * 2) : (int)roundf(avail_h * (cf / mfact_sum));
			h = MAX(1, h);
			resize(c, (struct wlr_box){.x = mx, .y = box.y + my + g, .width = mw_final, .height = h}, 0);
			my += h + g;
		} else {
			int avail_h = box.height - g * (ns + 1);
			int h = (i == n - 1) ? (box.height - ty - g * 2) : (int)roundf(avail_h * (cf / sfact_sum));
			h = MAX(1, h);
			resize(c, (struct wlr_box){.x = sx, .y = box.y + ty + g, .width = sw_final, .height = h}, 0);
			ty += h + g;
		}
	}
}

static void
center_tile_box(Monitor *m, const Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	int g = (int)gappx;
	int n = get_workspace_leaves(ws, leaves, 128);
	if (n == 0) return;

	int ws_nmaster = ws ? ws->nmaster : m->nmaster;
	float ws_mfact = ws ? ws->mfact : m->mfact;
	int nm = MIN(n, ws_nmaster);
	int ns = n - nm;

	if (ns <= 0) {
		int mw = (int)roundf((box.width - 2 * g) * ws_mfact);
		int mx = box.x + (box.width - mw) / 2;
		int my = 0;
		float mfact_sum = 0.0f;
		for (int i = 0; i < nm; i++)
			mfact_sum += leaves[i]->cfact > 0 ? leaves[i]->cfact : 1.0f;
		if (mfact_sum <= 0.0f) mfact_sum = 1.0f;

		for (int i = 0; i < nm; i++) {
			Client *c = leaves[i];
			float cf = c->cfact > 0 ? c->cfact : 1.0f;
			int avail_h = box.height - g * (nm + 1);
			int h = (i == nm - 1) ? (box.height - my - g * 2) : (int)roundf(avail_h * (cf / mfact_sum));
			h = MAX(1, h);
			resize(c, (struct wlr_box){.x = mx, .y = box.y + my + g, .width = mw, .height = h}, 0);
			my += h + g;
		}
		return;
	}

	int mw = (int)roundf((box.width - 2 * g) * ws_mfact);
	mw = MAX(100, MIN(box.width - 4 * g - 100, mw));

	int left_count = (ns + 1) / 2;
	int right_count = ns - left_count;

	int tw = (right_count > 0) ? (box.width - mw - 4 * g) / 2 : (box.width - mw - 3 * g);
	tw = MAX(1, tw);
	int mx = (right_count > 0) ? (box.x + g + tw + g) : (box.x + box.width - mw - g);

	float mfact_sum = 0.0f, lfact_sum = 0.0f, rfact_sum = 0.0f;
	int l_idx = 0, r_idx = 0;
	for (int i = 0; i < n; i++) {
		float cf = leaves[i]->cfact > 0 ? leaves[i]->cfact : 1.0f;
		if (i < nm) {
			mfact_sum += cf;
		} else {
			int s_i = i - nm;
			if (s_i % 2 == 0) lfact_sum += cf;
			else rfact_sum += cf;
		}
	}
	if (mfact_sum <= 0.0f) mfact_sum = 1.0f;
	if (lfact_sum <= 0.0f) lfact_sum = 1.0f;
	if (rfact_sum <= 0.0f) rfact_sum = 1.0f;

	int my = 0, ly = 0, ry = 0;
	for (int i = 0; i < n; i++) {
		Client *c = leaves[i];
		float cf = c->cfact > 0 ? c->cfact : 1.0f;
		if (i < nm) {
			int avail_h = box.height - g * (nm + 1);
			int h = (i == nm - 1) ? (box.height - my - g * 2) : (int)roundf(avail_h * (cf / mfact_sum));
			h = MAX(1, h);
			resize(c, (struct wlr_box){.x = mx, .y = box.y + my + g, .width = mw, .height = h}, 0);
			my += h + g;
		} else {
			int s_i = i - nm;
			if (s_i % 2 == 0) {
				int avail_h = box.height - g * (left_count + 1);
				int h = (l_idx == left_count - 1) ? (box.height - ly - g * 2) : (int)roundf(avail_h * (cf / lfact_sum));
				h = MAX(1, h);
				resize(c, (struct wlr_box){.x = box.x + g, .y = box.y + ly + g, .width = tw, .height = h}, 0);
				ly += h + g;
				l_idx++;
			} else {
				int avail_h = box.height - g * (right_count + 1);
				int h = (r_idx == right_count - 1) ? (box.height - ry - g * 2) : (int)roundf(avail_h * (cf / rfact_sum));
				h = MAX(1, h);
				resize(c, (struct wlr_box){.x = box.x + box.width - tw - g, .y = box.y + ry + g, .width = tw, .height = h}, 0);
				ry += h + g;
				r_idx++;
			}
		}
	}
}

static void
vertical_tile_box(Monitor *m, const Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	int g = (int)gappx;
	int n = get_workspace_leaves(ws, leaves, 128);
	if (n == 0) return;

	int ws_nmaster = ws ? ws->nmaster : m->nmaster;
	float ws_mfact = ws ? ws->mfact : m->mfact;
	int nm = MIN(n, ws_nmaster);
	int ns = n - nm;

	float mfact_sum = 0.0f, sfact_sum = 0.0f;
	for (int i = 0; i < n; i++) {
		if (i < nm) mfact_sum += leaves[i]->cfact > 0 ? leaves[i]->cfact : 1.0f;
		else sfact_sum += leaves[i]->cfact > 0 ? leaves[i]->cfact : 1.0f;
	}
	if (mfact_sum <= 0.0f) mfact_sum = 1.0f;
	if (sfact_sum <= 0.0f) sfact_sum = 1.0f;

	int my, mh_final, sy, sh_final;
	if (ns > 0 && nm > 0) {
		int mh = (int)roundf((box.height - g) * ws_mfact);
		my = box.y + g;
		mh_final = mh - g - g / 2;
		sy = box.y + mh + g / 2;
		sh_final = box.height - mh - g / 2 - g;
	} else {
		my = sy = box.y + g;
		mh_final = sh_final = box.height - 2 * g;
	}

	int mx = 0, tx = 0;
	for (int i = 0; i < n; i++) {
		Client *c = leaves[i];
		float cf = c->cfact > 0 ? c->cfact : 1.0f;
		if (i < nm) {
			int avail_w = box.width - g * (nm + 1);
			int w = (i == nm - 1) ? (box.width - mx - g * 2) : (int)roundf(avail_w * (cf / mfact_sum));
			w = MAX(1, w);
			resize(c, (struct wlr_box){.x = box.x + mx + g, .y = my, .width = w, .height = mh_final}, 0);
			mx += w + g;
		} else {
			int avail_w = box.width - g * (ns + 1);
			int w = (i == n - 1) ? (box.width - tx - g * 2) : (int)roundf(avail_w * (cf / sfact_sum));
			w = MAX(1, w);
			resize(c, (struct wlr_box){.x = box.x + tx + g, .y = sy, .width = w, .height = sh_final}, 0);
			tx += w + g;
		}
	}
}

static void
deck_box(Monitor *m, const Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	Client *c;
	int g = (int)gappx;
	int n = get_workspace_leaves(ws, leaves, 128);
	if (n == 0) return;

	int ws_nmaster = ws ? ws->nmaster : m->nmaster;
	float ws_mfact = ws ? ws->mfact : m->mfact;
	int nm = MIN(n, ws_nmaster);
	int ns = n - nm;

	int mw = (ns > 0 && nm > 0) ? (int)roundf((box.width - g) * ws_mfact) : box.width - g;

	float mfact_sum = 0.0f;
	for (int i = 0; i < nm; i++)
		mfact_sum += leaves[i]->cfact > 0 ? leaves[i]->cfact : 1.0f;
	if (mfact_sum <= 0.0f) mfact_sum = 1.0f;

	int my = 0;
	for (int i = 0; i < n; i++) {
		c = leaves[i];
		if (i < nm) {
			int avail_h = box.height - g * (nm + 1);
			int h = (i == nm - 1) ? (box.height - my - g * 2) : (int)roundf(avail_h * (c->cfact > 0 ? c->cfact : 1.0f) / mfact_sum);
			h = MAX(1, h);
			resize(c, (struct wlr_box){.x = box.x + g, .y = box.y + my + g, .width = MAX(1, mw - g - g / 2), .height = h}, 0);
			my += h + g;
		} else {
			struct wlr_box sbox = {
				.x = box.x + mw + g / 2,
				.y = box.y + g,
				.width = MAX(1, box.width - mw - g / 2 - g),
				.height = MAX(1, box.height - 2 * g)
			};
			resize(c, sbox, 0);
		}
	}
	if ((c = focustop(m)))
		wlr_scene_node_raise_to_top(&c->scene->node);
}

static void
vertical_deck_box(Monitor *m, const Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	Client *c;
	int g = (int)gappx;
	int n = get_workspace_leaves(ws, leaves, 128);
	if (n == 0) return;

	int ws_nmaster = ws ? ws->nmaster : m->nmaster;
	float ws_mfact = ws ? ws->mfact : m->mfact;
	int nm = MIN(n, ws_nmaster);
	int ns = n - nm;

	int mh = (ns > 0 && nm > 0) ? (int)roundf((box.height - g) * ws_mfact) : box.height - g;

	float mfact_sum = 0.0f;
	for (int i = 0; i < nm; i++)
		mfact_sum += leaves[i]->cfact > 0 ? leaves[i]->cfact : 1.0f;
	if (mfact_sum <= 0.0f) mfact_sum = 1.0f;

	int mx = 0;
	for (int i = 0; i < n; i++) {
		c = leaves[i];
		if (i < nm) {
			int avail_w = box.width - g * (nm + 1);
			int w = (i == nm - 1) ? (box.width - mx - g * 2) : (int)roundf(avail_w * (c->cfact > 0 ? c->cfact : 1.0f) / mfact_sum);
			w = MAX(1, w);
			resize(c, (struct wlr_box){.x = box.x + mx + g, .y = box.y + g, .width = w, .height = MAX(1, mh - g - g / 2)}, 0);
			mx += w + g;
		} else {
			struct wlr_box sbox = {
				.x = box.x + g,
				.y = box.y + mh + g / 2,
				.width = MAX(1, box.width - 2 * g),
				.height = MAX(1, box.height - mh - g / 2 - g)
			};
			resize(c, sbox, 0);
		}
	}
	if ((c = focustop(m)))
		wlr_scene_node_raise_to_top(&c->scene->node);
}

static void
grid_box(Monitor *m, const Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	int g = (int)gappx;
	(void)m;

	int n = get_workspace_leaves(ws, leaves, 128);
	if (n == 0)
		return;

	if (n == 1) {
		int cw = (int)((box.width - 2 * g) * 0.90f);
		int ch = (int)((box.height - 2 * g) * 0.90f);
		resize(leaves[0], (struct wlr_box){
			.x = box.x + (box.width - cw) / 2,
			.y = box.y + (box.height - ch) / 2,
			.width = MAX(1, cw),
			.height = MAX(1, ch)
		}, 0);
		return;
	}

	if (n == 2) {
		int cw = (box.width - 3 * g) / 2;
		int ch = (int)((box.height - 2 * g) * 0.65f);
		int cy = box.y + (box.height - ch) / 2;
		resize(leaves[0], (struct wlr_box){.x = box.x + g, .y = cy, .width = MAX(1, cw), .height = MAX(1, ch)}, 0);
		resize(leaves[1], (struct wlr_box){.x = box.x + g + cw + g, .y = cy, .width = MAX(1, cw), .height = MAX(1, ch)}, 0);
		return;
	}

	int cols = (int)ceil(sqrt((double)n));
	int rows = (n + cols - 1) / cols;

	int cell_w = (box.width - g * (cols + 1)) / cols;
	int cell_h = (box.height - g * (rows + 1)) / rows;
	cell_w = MAX(1, cell_w);
	cell_h = MAX(1, cell_h);

	for (int i = 0; i < n; i++) {
		int r = i / cols;
		int c = i % cols;
		int x, y;

		if (r == rows - 1 && n % cols != 0) {
			int overcols = n % cols;
			int last_row_w = overcols * cell_w + (overcols - 1) * g;
			int offset_x = (box.width - last_row_w) / 2;
			x = box.x + offset_x + c * (cell_w + g);
		} else {
			x = box.x + g + c * (cell_w + g);
		}
		y = box.y + g + r * (cell_h + g);

		resize(leaves[i], (struct wlr_box){
			.x = x,
			.y = y,
			.width = cell_w,
			.height = cell_h
		}, 0);
	}
}

static void
vertical_grid_box(Monitor *m, const Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	int g = (int)gappx;
	(void)m;

	int n = get_workspace_leaves(ws, leaves, 128);
	if (n == 0)
		return;

	int rows = (int)ceil(sqrt((double)n));
	int cols = (n + rows - 1) / rows;

	int cell_w = (box.width - g * (cols + 1)) / cols;
	int cell_h = (box.height - g * (rows + 1)) / rows;
	cell_w = MAX(1, cell_w);
	cell_h = MAX(1, cell_h);

	for (int i = 0; i < n; i++) {
		int c = i / rows;
		int r = i % rows;
		int x, y;

		if (c == cols - 1 && n % rows != 0) {
			int overrows = n % rows;
			int last_col_h = overrows * cell_h + (overrows - 1) * g;
			int offset_y = (box.height - last_col_h) / 2;
			y = box.y + offset_y + r * (cell_h + g);
		} else {
			y = box.y + g + r * (cell_h + g);
		}
		x = box.x + g + c * (cell_w + g);

		resize(leaves[i], (struct wlr_box){
			.x = x,
			.y = y,
			.width = cell_w,
			.height = cell_h
		}, 0);
	}
}

static void
columns_box(Monitor *m, const Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	int g = (int)gappx;

	(void)m;
	int n = get_workspace_leaves(ws, leaves, 128);
	if (n == 0)
		return;

	int x = box.x + g;
	int avail_w = box.width - g * (n + 1);

	for (int i = 0; i < n; i++) {
		int w = (i == n - 1) ? (box.x + box.width - g - x) : (avail_w / n);
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
fair_box(Monitor *m, const Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	int g = (int)gappx;
	(void)m;

	int n = get_workspace_leaves(ws, leaves, 128);
	if (n == 0) return;

	int cols = (int)ceil(sqrt((double)n));
	int rows = (n + cols - 1) / cols;

	int avail_w = box.width - g * (cols + 1);
	int avail_h = box.height - g * (rows + 1);

	for (int i = 0; i < n; i++) {
		int r = i / cols;
		int c = i % cols;

		int w = avail_w / cols;
		int h = avail_h / rows;

		int x = box.x + g + c * (w + g);
		int y = box.y + g + r * (h + g);

		resize(leaves[i], (struct wlr_box){.x = x, .y = y, .width = MAX(1, w), .height = MAX(1, h)}, 0);
	}
}

static void
vertical_fair_box(Monitor *m, const Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	int g = (int)gappx;
	(void)m;

	int n = get_workspace_leaves(ws, leaves, 128);
	if (n == 0) return;

	int rows = (int)ceil(sqrt((double)n));
	int cols = (n + rows - 1) / rows;

	int avail_w = box.width - g * (cols + 1);
	int avail_h = box.height - g * (rows + 1);

	for (int i = 0; i < n; i++) {
		int c = i / rows;
		int r = i % rows;

		int w = avail_w / cols;
		int h = avail_h / rows;

		int x = box.x + g + c * (w + g);
		int y = box.y + g + r * (h + g);

		resize(leaves[i], (struct wlr_box){.x = x, .y = y, .width = MAX(1, w), .height = MAX(1, h)}, 0);
	}
}

static void
scroller_box(Monitor *m, const Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	int g = (int)gappx;
	int n = get_workspace_leaves(ws, leaves, 128);
	if (n == 0) return;

	float ws_mfact = ws ? ws->mfact : m->mfact;
	Client *sel = focustop(m);
	int sel_idx = -1;
	for (int i = 0; i < n; i++) {
		if (leaves[i] == sel) {
			sel_idx = i;
			break;
		}
	}
	if (sel_idx < 0) sel_idx = 0;

	if (n == 1) {
		int w = (int)((box.width - 2 * g) * ws_mfact);
		resize(leaves[0], (struct wlr_box){
			.x = box.x + (box.width - w) / 2,
			.y = box.y + g,
			.width = MAX(1, w),
			.height = MAX(1, box.height - 2 * g)
		}, 0);
		return;
	}

	int main_w = (int)((box.width - 2 * g) * ws_mfact);
	int side_w = MAX(50, (box.width - main_w - 3 * g) / 2);
	int main_x = box.x + (box.width - main_w) / 2;

	resize(leaves[sel_idx], (struct wlr_box){
		.x = main_x,
		.y = box.y + g,
		.width = MAX(1, main_w),
		.height = MAX(1, box.height - 2 * g)
	}, 0);

	int left_count = sel_idx;
	if (left_count > 0) {
		int lx = main_x - g - side_w;
		for (int i = sel_idx - 1; i >= 0; i--) {
			resize(leaves[i], (struct wlr_box){
				.x = lx,
				.y = box.y + g,
				.width = MAX(1, side_w),
				.height = MAX(1, box.height - 2 * g)
			}, 0);
			lx -= (side_w + g);
		}
	}

	int right_count = n - sel_idx - 1;
	if (right_count > 0) {
		int rx = main_x + main_w + g;
		for (int i = sel_idx + 1; i < n; i++) {
			resize(leaves[i], (struct wlr_box){
				.x = rx,
				.y = box.y + g,
				.width = MAX(1, side_w),
				.height = MAX(1, box.height - 2 * g)
			}, 0);
			rx += (side_w + g);
		}
	}
	if (sel)
		wlr_scene_node_raise_to_top(&sel->scene->node);
}

static void
vertical_scroller_box(Monitor *m, const Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	int g = (int)gappx;
	int n = get_workspace_leaves(ws, leaves, 128);
	if (n == 0) return;

	float ws_mfact = ws ? ws->mfact : m->mfact;
	Client *sel = focustop(m);
	int sel_idx = -1;
	for (int i = 0; i < n; i++) {
		if (leaves[i] == sel) {
			sel_idx = i;
			break;
		}
	}
	if (sel_idx < 0) sel_idx = 0;

	if (n == 1) {
		int h = (int)((box.height - 2 * g) * ws_mfact);
		resize(leaves[0], (struct wlr_box){
			.x = box.x + g,
			.y = box.y + (box.height - h) / 2,
			.width = MAX(1, box.width - 2 * g),
			.height = MAX(1, h)
		}, 0);
		return;
	}

	int main_h = (int)((box.height - 2 * g) * ws_mfact);
	int side_h = MAX(50, (box.height - main_h - 3 * g) / 2);
	int main_y = box.y + (box.height - main_h) / 2;

	resize(leaves[sel_idx], (struct wlr_box){
		.x = box.x + g,
		.y = main_y,
		.width = MAX(1, box.width - 2 * g),
		.height = MAX(1, main_h)
	}, 0);

	int top_count = sel_idx;
	if (top_count > 0) {
		int ty = main_y - g - side_h;
		for (int i = sel_idx - 1; i >= 0; i--) {
			resize(leaves[i], (struct wlr_box){
				.x = box.x + g,
				.y = ty,
				.width = MAX(1, box.width - 2 * g),
				.height = MAX(1, side_h)
			}, 0);
			ty -= (side_h + g);
		}
	}

	int bottom_count = n - sel_idx - 1;
	if (bottom_count > 0) {
		int by = main_y + main_h + g;
		for (int i = sel_idx + 1; i < n; i++) {
			resize(leaves[i], (struct wlr_box){
				.x = box.x + g,
				.y = by,
				.width = MAX(1, box.width - 2 * g),
				.height = MAX(1, side_h)
			}, 0);
			by += (side_h + g);
		}
	}
	if (sel)
		wlr_scene_node_raise_to_top(&sel->scene->node);
}

static void
tile_box(Monitor *m, const Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	int g = (int)gappx;
	int dir = ws ? ws->dir : 0;

	int n = get_workspace_leaves(ws, leaves, 128);
	if (n == 0)
		return;

	int ws_nmaster = ws ? ws->nmaster : m->nmaster;
	float ws_mfact = ws ? ws->mfact : m->mfact;

	int nm = MIN(n, ws_nmaster);
	int ns = n - nm;

	float mfact_sum = 0.0f, sfact_sum = 0.0f;
	for (int i = 0; i < n; i++) {
		if (i < nm)
			mfact_sum += leaves[i]->cfact > 0 ? leaves[i]->cfact : 1.0f;
		else
			sfact_sum += leaves[i]->cfact > 0 ? leaves[i]->cfact : 1.0f;
	}
	if (mfact_sum <= 0.0f) mfact_sum = 1.0f;
	if (sfact_sum <= 0.0f) sfact_sum = 1.0f;

	if (dir == 0 || dir == 2) {
		int mx, mw_final, sx, sw_final;
		if (ns > 0 && nm > 0) {
			int mw = (int)roundf((box.width - g) * ws_mfact);
			if (dir == 0) {
				mx = box.x + g;
				mw_final = mw - g - g / 2;
				sx = box.x + mw + g / 2;
				sw_final = box.width - mw - g / 2 - g;
			} else {
				sx = box.x + g;
				sw_final = box.width - mw - g / 2 - g;
				mx = box.x + box.width - mw + g / 2;
				mw_final = mw - g - g / 2;
			}
		} else {
			mx = sx = box.x + g;
			mw_final = sw_final = box.width - 2 * g;
		}

		int my = 0, ty = 0;
		for (int i = 0; i < n; i++) {
			Client *c = leaves[i];
			float cf = c->cfact > 0 ? c->cfact : 1.0f;
			if (i < nm) {
				int avail_h = box.height - g * (nm + 1);
				int h = (i == nm - 1) ? (box.height - my - g * 2) : (int)roundf(avail_h * (cf / mfact_sum));
				h = MAX(1, h);
				resize(c, (struct wlr_box){.x = mx, .y = box.y + my + g, .width = mw_final, .height = h}, 0);
				my += h + g;
			} else {
				int avail_h = box.height - g * (ns + 1);
				int h = (i == n - 1) ? (box.height - ty - g * 2) : (int)roundf(avail_h * (cf / sfact_sum));
				h = MAX(1, h);
				resize(c, (struct wlr_box){.x = sx, .y = box.y + ty + g, .width = sw_final, .height = h}, 0);
				ty += h + g;
			}
		}
	} else {
		int my, mh_final, sy, sh_final;
		if (ns > 0 && nm > 0) {
			int mh = (int)roundf((box.height - g) * ws_mfact);
			if (dir == 1) {
				my = box.y + g;
				mh_final = mh - g - g / 2;
				sy = box.y + mh + g / 2;
				sh_final = box.height - mh - g / 2 - g;
			} else {
				sy = box.y + g;
				sh_final = box.height - mh - g / 2 - g;
				my = box.y + box.height - mh + g / 2;
				mh_final = mh - g - g / 2;
			}
		} else {
			my = sy = box.y + g;
			mh_final = sh_final = box.height - 2 * g;
		}

		int mx = 0, tx = 0;
		for (int i = 0; i < n; i++) {
			Client *c = leaves[i];
			float cf = c->cfact > 0 ? c->cfact : 1.0f;
			if (i < nm) {
				int avail_w = box.width - g * (nm + 1);
				int w = (i == nm - 1) ? (box.width - mx - g * 2) : (int)roundf(avail_w * (cf / mfact_sum));
				w = MAX(1, w);
				resize(c, (struct wlr_box){.x = box.x + mx + g, .y = my, .width = w, .height = mh_final}, 0);
				mx += w + g;
			} else {
				int avail_w = box.width - g * (ns + 1);
				int w = (i == n - 1) ? (box.width - tx - g * 2) : (int)roundf(avail_w * (cf / sfact_sum));
				w = MAX(1, w);
				resize(c, (struct wlr_box){.x = box.x + tx + g, .y = sy, .width = w, .height = sh_final}, 0);
				tx += w + g;
			}
		}
	}
}

static void
monocle_box(Monitor *m, const Workspace *ws, struct wlr_box box)
{
	Client *leaves[128];
	Client *c;
	int g = (int)gappx;

	int n = get_workspace_leaves(ws, leaves, 128);
	struct wlr_box gbox = {
		.x = box.x + g,
		.y = box.y + g,
		.width = MAX(1, box.width - 2 * g),
		.height = MAX(1, box.height - 2 * g),
	};

	for (int i = 0; i < n; i++)
		resize(leaves[i], gbox, 0);

	if (n && ws == m->active_workspace)
		snprintf(m->ltsymbol, LENGTH(m->ltsymbol), "[%d]", n);
	if ((c = focustop(m)))
		wlr_scene_node_raise_to_top(&c->scene->node);
}

static void
arrange_workspace(Monitor *m, const Workspace *ws, struct wlr_box box, const Layout *lt)
{
	if (!m || !ws || !lt || !lt->arrange)
		return;

	if (lt->arrange == tile)
		tile_box(m, ws, box);
	else if (lt->arrange == right_tile)
		right_tile_box(m, ws, box);
	else if (lt->arrange == center_tile)
		center_tile_box(m, ws, box);
	else if (lt->arrange == vertical_tile)
		vertical_tile_box(m, ws, box);
	else if (lt->arrange == deck)
		deck_box(m, ws, box);
	else if (lt->arrange == vertical_deck)
		vertical_deck_box(m, ws, box);
	else if (lt->arrange == monocle)
		monocle_box(m, ws, box);
	else if (lt->arrange == grid)
		grid_box(m, ws, box);
	else if (lt->arrange == vertical_grid)
		vertical_grid_box(m, ws, box);
	else if (lt->arrange == dwindle)
		dwindle_box(m, ws, box);
	else if (lt->arrange == columns)
		columns_box(m, ws, box);
	else if (lt->arrange == fair)
		fair_box(m, ws, box);
	else if (lt->arrange == vertical_fair)
		vertical_fair_box(m, ws, box);
	else if (lt->arrange == scroller)
		scroller_box(m, ws, box);
	else if (lt->arrange == vertical_scroller)
		vertical_scroller_box(m, ws, box);
	else
		lt->arrange(m);
}

void
right_tile(Monitor *m)
{
	if (m && m->active_workspace)
		right_tile_box(m, m->active_workspace, m->w);
}

void
center_tile(Monitor *m)
{
	if (m && m->active_workspace)
		center_tile_box(m, m->active_workspace, m->w);
}

void
vertical_tile(Monitor *m)
{
	if (m && m->active_workspace)
		vertical_tile_box(m, m->active_workspace, m->w);
}

void
deck(Monitor *m)
{
	if (m && m->active_workspace)
		deck_box(m, m->active_workspace, m->w);
}

void
vertical_deck(Monitor *m)
{
	if (m && m->active_workspace)
		vertical_deck_box(m, m->active_workspace, m->w);
}

void
vertical_grid(Monitor *m)
{
	if (m && m->active_workspace)
		vertical_grid_box(m, m->active_workspace, m->w);
}

void
fair(Monitor *m)
{
	if (m && m->active_workspace)
		fair_box(m, m->active_workspace, m->w);
}

void
vertical_fair(Monitor *m)
{
	if (m && m->active_workspace)
		vertical_fair_box(m, m->active_workspace, m->w);
}

void
scroller(Monitor *m)
{
	if (m && m->active_workspace)
		scroller_box(m, m->active_workspace, m->w);
}

void
vertical_scroller(Monitor *m)
{
	if (m && m->active_workspace)
		vertical_scroller_box(m, m->active_workspace, m->w);
}

void
dwindle(Monitor *m)
{
	if (m && m->active_workspace)
		dwindle_box(m, m->active_workspace, m->w);
}

void
grid(Monitor *m)
{
	if (m && m->active_workspace)
		grid_box(m, m->active_workspace, m->w);
}

void
columns(Monitor *m)
{
	if (m && m->active_workspace)
		columns_box(m, m->active_workspace, m->w);
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
setlayoutdir(const Arg *arg)
{
	Workspace *ws;
	if (!selmon || !arg)
		return;
	ws = selmon->active_workspace;
	if (ws) {
		ws->dir = (arg->i % 4 + 4) % 4;
		arrange(selmon);
	}
}

void
rotatelayout(const Arg *arg)
{
	Workspace *ws;
	if (!selmon)
		return;
	ws = selmon->active_workspace;
	if (ws) {
		int delta = arg ? arg->i : 1;
		ws->dir = (ws->dir + delta + 4) % 4;
		arrange(selmon);
	}
}

void
incnmaster(const Arg *arg)
{
	Workspace *ws;
	if (!arg || !selmon)
		return;
	ws = selmon->active_workspace;
	if (ws)
		ws->nmaster = MAX(ws->nmaster + arg->i, 0);
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
	if (selmon->scratchpad_showing)
		ws = workspace_get_by_id(selmon, SCRATCHPAD_WORKSPACE);
	else
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
	Workspace *ws;

	if (!arg || !selmon || !selmon->lt[selmon->sellt]->arrange)
		return;

	ws = selmon->active_workspace;
	float cur_mfact = (ws && ws->mfact > 0) ? ws->mfact : selmon->mfact;

	if (arg->f == 0.0f) {
		if (ws) ws->mfact = 0.55f;
		selmon->mfact = 0.55f;
		arrange(selmon);
		return;
	}
	f = arg->f < 1.0f ? arg->f + cur_mfact : arg->f - 1.0f;
	if (f < 0.1f || f > 0.9f)
		return;
	if (ws)
		ws->mfact = f;
	selmon->mfact = f;
	arrange(selmon);
}

void
setcfact(const Arg *arg)
{
	Client *c;

	if (!arg || !selmon || !selmon->lt[selmon->sellt]->arrange)
		return;
	c = focustop(selmon);
	if (!c || c->isfloating)
		return;

	if (arg->f == 0.0f) {
		c->cfact = 1.0f;
	} else {
		float f = c->cfact + arg->f;
		if (f < 0.25f)
			f = 0.25f;
		else if (f > 4.0f)
			f = 4.0f;
		c->cfact = f;
	}
	arrange(selmon);
}

/* Overview Mode Grid Layout Algorithm */
static int
get_overview_clients(const Monitor *m, Client **leaves, int max)
{
	Client *c;
	int count;
	if (!m)
		return 0;

	count = m->active_workspace ? get_workspace_leaves(m->active_workspace, leaves, max) : 0;
	wl_list_for_each(c, &clients, link) {
		if (c->mon == m && (!c->ws || c->ws->id != SCRATCHPAD_WORKSPACE)) {
			int j;
			for (j = 0; j < count && leaves[j] != c; j++);
			if (j == count && count < max)
				leaves[count++] = c;
		}
	}
	return count;
}

/* Overview Mode Grid Layout Algorithm */
static void
overview(Monitor *m)
{
	Client *leaves[128];

	if (!m)
		return;

	int n = get_overview_clients(m, leaves, 128);
	if (n == 0) {
		clearlabeloverlays(m);
		return;
	}

	int cols = (int)ceil(sqrt((double)n));
	int rows = (n + cols - 1) / cols;

	int inset_x = (int)(m->w.width * 0.04f);
	int inset_y = (int)(m->w.height * 0.04f);
	int available_w = m->w.width - (inset_x * 2);
	int available_h = m->w.height - (inset_y * 2);

	int tile_w = available_w / cols;
	int tile_h = available_h / rows;

	int last_row_cols = n % cols;
	if (last_row_cols == 0)
		last_row_cols = cols;
	int last_row_offset = (available_w - (last_row_cols * tile_w)) / 2;

	for (int i = 0; i < n; i++) {
		Client *c = leaves[i];
		int row = i / cols;
		int col = i % cols;
		int grid_x, grid_y;

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

static void
clearlabeloverlays(const Monitor *m)
{
	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (c->mon == m)
			destroylabeloverlay(c);
	}
}

static void
updatelabeloverlays(const Monitor *m)
{
	Client *leaves[128];
	int num_labels = (int)strlen(overview_labels);

	clearlabeloverlays(m);
	if (!m || !m->isoverview)
		return;

	int n = get_overview_clients(m, leaves, 128);

	for (int i = 0; i < MIN(n, num_labels); i++) {
		char lbl = overview_labels[i];
		char upper_lbl = (lbl >= 'a' && lbl <= 'z') ? ('A' + (lbl - 'a')) : lbl;

		Client *c = leaves[i];
		c->label = lbl;
		c->label_tree = wlr_scene_tree_create(layers[LyrFloat]);
		if (!c->label_tree)
			continue;

		wlr_scene_rect_create(c->label_tree, 36, 36, (float[]){0.1f, 0.1f, 0.15f, 0.95f});
		struct wlr_scene_rect *bg_inner = wlr_scene_rect_create(c->label_tree, 32, 32, (float[]){0.2f, 0.5f, 0.9f, 1.0f});
		wlr_scene_node_set_position(&bg_inner->node, 2, 2);

		int font_idx = (upper_lbl >= 'A' && upper_lbl <= 'Z') ? (upper_lbl - 'A') : 0;
		int start_x = 10;
		int start_y = 7;
		for (int row = 0; row < 7; row++) {
			for (int col = 0; col < 5; col++) {
				if ((font5x7[font_idx][row] >> (4 - col)) & 1) {
					struct wlr_scene_rect *pixel = wlr_scene_rect_create(c->label_tree, 3, 3, (float[]){1.0f, 1.0f, 1.0f, 1.0f});
					wlr_scene_node_set_position(&pixel->node, start_x + col * 3, start_y + row * 3);
				}
			}
		}

		int badge_x = c->geom.x + (c->geom.width - 36) / 2;
		int badge_y = c->geom.y + (c->geom.height - 36) / 2;
		wlr_scene_node_set_position(&c->label_tree->node, badge_x, badge_y);
		wlr_scene_node_raise_to_top(&c->label_tree->node);
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
			if (c->mon == selmon && !c->isfullscreen)
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
			if (c->mon == selmon && c->isfloating && !c->isfullscreen)
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
		if (tc == c || (selmon->isoverview ? (tc->mon != selmon || (tc->ws && tc->ws->id == SCRATCHPAD_WORKSPACE)) : !VISIBLEON(tc, selmon)))
			continue;

		if ((tc->ws && tc->ws->id == SCRATCHPAD_WORKSPACE) != is_scratch)
			continue;

		tx = tc->geom.x + tc->geom.width / 2.0;
		ty = tc->geom.y + tc->geom.height / 2.0;
		dx = tx - cx;
		dy = ty - cy;

		double dist;
		if (spatial_direction_match(dx, dy, dir, &dist)) {
			if (dist < min_dist) {
				min_dist = dist;
				best = tc;
			}
		} else {
			double wrap_primary = 0, wrap_secondary = 0;
			switch (dir) {
			case WLR_DIRECTION_LEFT: wrap_primary = tx; wrap_secondary = fabs(dy); break;
			case WLR_DIRECTION_RIGHT: wrap_primary = -tx; wrap_secondary = fabs(dy); break;
			case WLR_DIRECTION_UP: wrap_primary = ty; wrap_secondary = fabs(dx); break;
			case WLR_DIRECTION_DOWN: wrap_primary = -ty; wrap_secondary = fabs(dx); break;
			}
			double wdist = -wrap_primary * 1000.0 + wrap_secondary;
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

void
movestack(const Arg *arg)
{
	Client *c = focustop(selmon);
	Client *target = NULL;
	if (!c || c->isfloating || !selmon)
		return;

	if (arg->i > 0) {
		struct wl_list *p;
		for (p = c->link.next; p != &clients; p = p->next) {
			Client *tmp = wl_container_of(p, tmp, link);
			if (tmp->mon == selmon && tmp->ws == c->ws && !tmp->isfloating) {
				target = tmp;
				break;
			}
		}
		if (target) {
			wl_list_remove(&c->link);
			wl_list_insert(&target->link, &c->link);
		}
	} else {
		struct wl_list *p;
		for (p = c->link.prev; p != &clients; p = p->prev) {
			Client *tmp = wl_container_of(p, tmp, link);
			if (tmp->mon == selmon && tmp->ws == c->ws && !tmp->isfloating) {
				target = tmp;
				break;
			}
		}
		if (target) {
			wl_list_remove(&c->link);
			wl_list_insert(target->link.prev, &c->link);
		}
	}
	arrange(selmon);
}

void
movestack_dir(const Arg *arg)
{
	Client *c = focustop(selmon);
	Client *best = NULL, *tc;
	double cx, cy, tx, ty, dx, dy;
	double min_dist = 1e18;
	int dir = arg->i;

	if (!selmon || !c || c->isfloating)
		return;

	cx = c->geom.x + c->geom.width / 2.0;
	cy = c->geom.y + c->geom.height / 2.0;

	wl_list_for_each(tc, &clients, link) {
		if (tc == c || !tc->ws || tc->ws != c->ws || !VISIBLEON(tc, selmon) || tc->isfloating)
			continue;

		tx = tc->geom.x + tc->geom.width / 2.0;
		ty = tc->geom.y + tc->geom.height / 2.0;
		dx = tx - cx;
		dy = ty - cy;

		double dist;
		if (spatial_direction_match(dx, dy, dir, &dist)) {
			if (dist < min_dist) {
				min_dist = dist;
				best = tc;
			}
		}
	}

	if (best) {
		struct wl_list *c_prev = c->link.prev;
		struct wl_list *b_prev = best->link.prev;

		if (c_prev == &best->link) {
			wl_list_remove(&c->link);
			wl_list_insert(&best->link, &c->link);
		} else if (b_prev == &c->link) {
			wl_list_remove(&best->link);
			wl_list_insert(&c->link, &best->link);
		} else {
			wl_list_remove(&c->link);
			wl_list_remove(&best->link);
			wl_list_insert(b_prev, &c->link);
			wl_list_insert(c_prev, &best->link);
		}
		arrange(selmon);
	} else {
		movestack(arg);
	}
}

