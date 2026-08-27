/*
 * See LICENSE file for copyright and license details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "dwl.h"
#include "client.h"
#include "layout.h"
#include "config.h"
#include "util.h"
#include "tree.h"
#include "workspace.h"

Node *
node_create(NodeType type, Workspace *ws)
{
	Node *node = ecalloc(1, sizeof(Node));
	node->type = type;
	node->split_type = SPLIT_HORIZONTAL;
	node->ratio_h = 0.5f;
	node->ratio_v = 0.5f;
	node->ws = ws;
	wl_list_init(&node->children);
	wl_list_init(&node->link);
	return node;
}

void
node_insert_child(Node *parent, Node *child)
{
	if (!parent || !child)
		return;
	child->parent = parent;
	child->ws = parent->ws;
	wl_list_remove(&child->link);
	wl_list_insert(parent->children.prev, &child->link);
	if (parent->ws)
		parent->ws->tree_gen++;
}

void
node_insert_after(Node *sibling, Node *child)
{
	if (!sibling || !child)
		return;
	child->parent = sibling->parent;
	child->ws = sibling->ws;
	wl_list_remove(&child->link);
	wl_list_insert(&sibling->link, &child->link);
	if (sibling->ws)
		sibling->ws->tree_gen++;
}

int
node_is_ancestor(Node *ancestor, Node *node)
{
	while (node) {
		if (node == ancestor)
			return 1;
		node = node->parent;
	}
	return 0;
}

Node *
node_insert_client(Workspace *ws, Client *c)
{
	Node *target_parent, *leaf, *focus_ref;
	SplitType desired_split;

	if (!ws || !ws->root || !c)
		return NULL;

	if (c->node && c->ws == ws)
		return c->node;

	leaf = node_create(NODE_LEAF, ws);
	leaf->client = c;
	c->node = leaf;

	/* Case 1: Empty workspace root */
	if (wl_list_empty(&ws->root->children)) {
		node_insert_child(ws->root, leaf);
		ws->focused_node = leaf;
		return leaf;
	}

	/* Case 2: Identify focus reference node (O(1) access) */
	focus_ref = (ws->focused_node && ws->focused_node != ws->root) ? ws->focused_node : NULL;
	if (!focus_ref) {
		focus_ref = wl_container_of(ws->root->children.prev, focus_ref, link);
	}
	if (!focus_ref)
		focus_ref = ws->root;

	target_parent = focus_ref->parent ? focus_ref->parent : ws->root;

	if (ws->next_split != 0) {
		desired_split = ws->next_split;
		ws->next_split = 0;
	} else if (focus_ref->type == NODE_LEAF && focus_ref->geom.width > 0 && focus_ref->geom.height > 0) {
		desired_split = (focus_ref->geom.width >= focus_ref->geom.height) ? SPLIT_HORIZONTAL : SPLIT_VERTICAL;
	} else if (target_parent->split_type == SPLIT_HORIZONTAL) {
		desired_split = SPLIT_VERTICAL;
	} else {
		desired_split = SPLIT_HORIZONTAL;
	}

	if (focus_ref->type == NODE_LEAF) {
		Node *container = node_create(NODE_CONTAINER, ws);
		container->split_type = desired_split;

		node_insert_after(focus_ref, container);
		node_insert_child(container, focus_ref);
		node_insert_child(container, leaf);
	} else {
		node_insert_after(focus_ref, leaf);
	}

	ws->focused_node = leaf;
	return leaf;
}

Node *
node_insert_client_at(Workspace *ws, Client *c, Client *at, int dir)
{
	Node *target_parent, *leaf, *target_node;
	SplitType desired_split;
	int insert_before;

	if (!ws || !ws->root || !c)
		return NULL;

	if (c->node && c->ws == ws)
		return c->node;

	if (!at || !at->node || at->ws != ws)
		return node_insert_client(ws, c);

	target_node = at->node;
	target_parent = target_node->parent ? target_node->parent : ws->root;

	leaf = node_create(NODE_LEAF, ws);
	leaf->client = c;
	c->node = leaf;

	switch (dir) {
	case WLR_DIRECTION_LEFT:
		desired_split = SPLIT_HORIZONTAL;
		insert_before = 1;
		break;
	case WLR_DIRECTION_RIGHT:
		desired_split = SPLIT_HORIZONTAL;
		insert_before = 0;
		break;
	case WLR_DIRECTION_UP:
		desired_split = SPLIT_VERTICAL;
		insert_before = 1;
		break;
	case WLR_DIRECTION_DOWN:
		desired_split = SPLIT_VERTICAL;
		insert_before = 0;
		break;
	default:
		desired_split = SPLIT_HORIZONTAL;
		insert_before = 0;
		break;
	}

	/* Automatically adapt split orientation if container has only one child */
	if (node_has_single_child(target_parent)) {
		target_parent->split_type = desired_split;
	}

	if (target_parent->split_type == desired_split) {
		if (insert_before) {
			leaf->parent = target_parent;
			leaf->ws = ws;
			wl_list_remove(&leaf->link);
			wl_list_insert(target_node->link.prev, &leaf->link);
		} else {
			node_insert_after(target_node, leaf);
		}
	} else {
		Node *container = node_create(NODE_CONTAINER, ws);
		container->split_type = desired_split;

		node_insert_after(target_node, container);
		if (insert_before) {
			node_insert_child(container, leaf);
			node_insert_child(container, target_node);
		} else {
			node_insert_child(container, target_node);
			node_insert_child(container, leaf);
		}
	}

	ws->focused_node = leaf;
	return leaf;
}

static void
node_collapse_container(Node *container)
{
	Node *only_child, *gparent;

	/* Collapse container if it contains only 1 child, hoisting child to grandparent */
	if (container->type != NODE_CONTAINER || container->type == NODE_ROOT)
		return;

	if (!node_has_single_child(container))
		return;

	only_child = wl_container_of(container->children.next, only_child, link);
	gparent = container->parent;
	if (!gparent)
		return;

	wl_list_insert(&container->link, &only_child->link);
	wl_list_remove(&container->link);
	wl_list_init(&container->link);

	only_child->parent = gparent;

	if (container->ws && container->ws->focused_node == container)
		container->ws->focused_node = only_child;

	free(container);

	if (gparent->type == NODE_CONTAINER)
		node_collapse_container(gparent);
}

void
node_remove(Node *node)
{
	Node *parent;
	Workspace *ws;

	if (!node || node->type == NODE_ROOT)
		return;

	ws = node->ws;
	if (ws)
		ws->tree_gen++;

	if (node->client)
		node->client->node = NULL;

	parent = node->parent;

	/* Safely update workspace focused_node if it points to node or inside node's subtree */
	if (ws && node_is_ancestor(node, ws->focused_node)) {
		Node *sibling = NULL;
		if (parent && node->link.next != &parent->children)
			sibling = wl_container_of(node->link.next, sibling, link);
		else if (parent && node->link.prev != &parent->children)
			sibling = wl_container_of(node->link.prev, sibling, link);

		ws->focused_node = sibling ? sibling : (parent ? parent : ws->root);
	}

	wl_list_remove(&node->link);
	wl_list_init(&node->link);

	if (node->type != NODE_LEAF) {
		Node *child, *tmp;
		wl_list_for_each_safe(child, tmp, &node->children, link) {
			child->parent = NULL;
			node_remove(child);
		}
	}

	free(node);

	if (parent && parent->type == NODE_CONTAINER) {
		if (wl_list_empty(&parent->children))
			node_remove(parent);
		else
			node_collapse_container(parent);
	}
}

static inline float
node_get_weight(const Node *child, SplitType split)
{
	float r = (split == SPLIT_HORIZONTAL) ? child->ratio_h : child->ratio_v;
	return (r > 0.001f) ? r : 0.05f;
}

Node *
node_find_client(Node *root, Client *c)
{
	if (!root || !c || !c->node)
		return NULL;

	/* O(1): client caches its node, and node caches its workspace */
	return (c->node->ws == root->ws) ? c->node : NULL;
}

int
node_count_leaves(Node *node)
{
	Node *child;
	int count = 0;

	if (!node)
		return 0;

	if (node->type == NODE_LEAF)
		return client_is_tileable(node->client) ? 1 : 0;

	wl_list_for_each(child, &node->children, link) {
		count += node_count_leaves(child);
	}
	return count;
}

int
node_collect_leaves(Node *node, Client **array, int max)
{
	Node *child;
	int count = 0;

	if (!node || max <= 0)
		return 0;

	if (node->type == NODE_LEAF) {
		Client *c = node->client;
		if (client_is_tileable(c)) {
			array[0] = c;
			return 1;
		}
		return 0;
	}

	wl_list_for_each(child, &node->children, link) {
		int sub = node_collect_leaves(child, array + count, max - count);
		count += sub;
		if (count >= max)
			break;
	}
	return count;
}

void
node_arrange_recursive(Node *node, struct wlr_box box)
{
	Node *child;
	float total_ratio = 0.0f;
	int child_count = 0;
	int offset = 0;
	int g = (int)gappx;

	if (!node)
		return;

	node->geom = box;

	if (node->type == NODE_LEAF) {
		Client *c = node->client;
		if (client_is_tileable(c)) {
			int min_w = (int)min_width;
			int min_h = (int)min_height;
			struct wlr_box gbox = {
				.x = box.x + g,
				.y = box.y + g,
				.width = MAX(min_w, box.width - 2 * g),
				.height = MAX(min_h, box.height - 2 * g)
			};
			resize(c, gbox, 0);
		}
		return;
	}

	wl_list_for_each(child, &node->children, link) {
		child_count++;
		total_ratio += node_get_weight(child, node->split_type);
	}

	if (child_count == 0)
		return;

	if (total_ratio <= 0.0f)
		total_ratio = (float)child_count;

	wl_list_for_each(child, &node->children, link) {
		struct wlr_box child_box = box;
		float child_weight = node_get_weight(child, node->split_type);

		if (node->split_type == SPLIT_HORIZONTAL) {
			int w = (int)roundf((float)box.width * (child_weight / total_ratio));
			if (child->link.next == &node->children) {
				w = box.width - offset;
			}
			w = MAX(1, w);
			child_box.x = box.x + offset;
			child_box.width = w;
			offset += w;
		} else if (node->split_type == SPLIT_VERTICAL) {
			int h = (int)roundf((float)box.height * (child_weight / total_ratio));
			if (child->link.next == &node->children) {
				h = box.height - offset;
			}
			h = MAX(1, h);
			child_box.y = box.y + offset;
			child_box.height = h;
			offset += h;
		}

		node_arrange_recursive(child, child_box);
	}
}

void
node_free_tree(Node *node)
{
	Node *child, *tmp;

	if (!node)
		return;

	wl_list_for_each_safe(child, tmp, &node->children, link) {
		node_free_tree(child);
	}

	if (node->client)
		node->client->node = NULL;
	free(node);
}

void
tree_resize_node(Node *node, float delta)
{
	if (!node)
		return;
	node->ratio_h = clamp_ratio(node->ratio_h + delta);
	node->ratio_v = clamp_ratio(node->ratio_v + delta);
}

void
tree_swap_nodes(Node *a, Node *b)
{
	Client *tmp;
	if (!a || !b || !a->client || !b->client)
		return;

	tmp = a->client;
	a->client = b->client;
	b->client = tmp;

	if (a->client)
		a->client->node = a;
	if (b->client)
		b->client->node = b;
}

void
tree_equalize_node(Node *node)
{
	Node *child;

	if (!node)
		return;

	node->ratio_h = 0.5f;
	node->ratio_v = 0.5f;
	wl_list_for_each(child, &node->children, link) {
		tree_equalize_node(child);
	}
}

void
tree_equalize_active(const Arg *arg)
{
	Client *c;
	Workspace *ws;

	if (!selmon)
		return;

	c = focustop(selmon);
	ws = (c && c->ws) ? c->ws : selmon->active_workspace;
	if (ws && ws->root) {
		tree_equalize_node(ws->root);
		arrange(selmon);
	}
}

static void
adjust_sibling_ratios(Node *node_first, Node *node_second, int is_horiz, float delta)
{
	float min_weight = 0.05f;
	float act_d;

	if (!node_first || !node_second)
		return;

	if (is_horiz) {
		if (delta > 0.0f) {
			act_d = MIN(delta, MAX(0.0f, node_second->ratio_h - min_weight));
			node_first->ratio_h += act_d;
			node_second->ratio_h -= act_d;
		} else if (delta < 0.0f) {
			float neg_d = -delta;
			act_d = MIN(neg_d, MAX(0.0f, node_first->ratio_h - min_weight));
			node_first->ratio_h -= act_d;
			node_second->ratio_h += act_d;
		}
		node_first->ratio_h = clamp_ratio(node_first->ratio_h);
		node_second->ratio_h = clamp_ratio(node_second->ratio_h);
	} else {
		if (delta > 0.0f) {
			act_d = MIN(delta, MAX(0.0f, node_second->ratio_v - min_weight));
			node_first->ratio_v += act_d;
			node_second->ratio_v -= act_d;
		} else if (delta < 0.0f) {
			float neg_d = -delta;
			act_d = MIN(neg_d, MAX(0.0f, node_first->ratio_v - min_weight));
			node_first->ratio_v -= act_d;
			node_second->ratio_v += act_d;
		}
		node_first->ratio_v = clamp_ratio(node_first->ratio_v);
		node_second->ratio_v = clamp_ratio(node_second->ratio_v);
	}
}

static void
adjust_node_ratio(Node *target, Node *prev, Node *next, int dir, int is_horiz, float delta)
{
	if (!target)
		return;

	if (is_horiz) {
		if (dir == WLR_DIRECTION_RIGHT) {
			if (next)
				adjust_sibling_ratios(target, next, 1, delta);
			else if (prev)
				adjust_sibling_ratios(prev, target, 1, delta);
		} else if (dir == WLR_DIRECTION_LEFT) {
			if (prev)
				adjust_sibling_ratios(prev, target, 1, -delta);
			else if (next)
				adjust_sibling_ratios(target, next, 1, -delta);
		}
	} else {
		if (dir == WLR_DIRECTION_DOWN) {
			if (next)
				adjust_sibling_ratios(target, next, 0, delta);
			else if (prev)
				adjust_sibling_ratios(prev, target, 0, delta);
		} else if (dir == WLR_DIRECTION_UP) {
			if (prev)
				adjust_sibling_ratios(prev, target, 0, -delta);
			else if (next)
				adjust_sibling_ratios(target, next, 0, -delta);
		}
	}
}

void
tree_resize_active(const Arg *arg)
{
	Client *sel;
	Workspace *ws;
	const Layout *lt;

	if (!selmon || !arg)
		return;

	sel = focustop(selmon);
	if (!sel)
		return;

	lt = resolve_layout(sel, selmon, &ws);

	if (sel->isfloating) {
		struct wlr_box g = sel->geom;
		int step = (int)(arg->f * 100.0f);
		if (step == 0)
			step = (arg->f > 0) ? 50 : -50;
		g.width = MAX((int)min_width, g.width + step);
		g.height = MAX((int)min_height, g.height + step);
		resize(sel, g, 1);
		return;
	}

	if (sel->node) {
		tree_resize_node(sel->node, arg->f);
		if (lt && (lt->arrange == tile || lt->arrange == master_stack)) {
			float f = selmon->mfact + arg->f;
			if (f >= 0.1f && f <= 0.9f)
				selmon->mfact = f;
		}
		arrange(selmon);
	}
}

void
tree_resize_dir(const Arg *arg)
{
	Client *sel;
	float delta = 0.05f;
	int dir, is_horiz;
	const Layout *lt;
	Workspace *ws;
	Client *leaves[128];
	int n, idx = -1, i;
	Node *target_node, *prev_node, *next_node;

	if (!selmon || !arg)
		return;

	sel = focustop(selmon);
	if (!sel)
		return;

	if (sel->isfloating) {
		int step = 100;
		struct wlr_box g = sel->geom;
		switch (arg->i) {
		case WLR_DIRECTION_RIGHT:
			g.width += step;
			break;
		case WLR_DIRECTION_LEFT:
			g.width = MAX((int)min_width, g.width - step);
			break;
		case WLR_DIRECTION_DOWN:
			g.height += step;
			break;
		case WLR_DIRECTION_UP:
			g.height = MAX((int)min_height, g.height - step);
			break;
		}
		resize(sel, g, 1);
		return;
	}

	if (!sel->node)
		return;

	dir = arg->i;
	is_horiz = (dir == WLR_DIRECTION_LEFT || dir == WLR_DIRECTION_RIGHT);
	lt = resolve_layout(sel, selmon, &ws);

	if (lt && lt->arrange == monocle)
		return;

	/* 1. Handling tree_layout, bsp_layout & dwindle */
	if (lt && (lt->arrange == tree_layout || lt->arrange == bsp_layout || lt->arrange == dwindle)) {
		Node *curr, *parent = NULL;
		Node *prev_sub = NULL, *next_sub = NULL;
		target_node = NULL;

		for (curr = sel->node; curr; curr = curr->parent) {
			if (curr->parent && curr->parent->split_type != SPLIT_NONE) {
				if ((is_horiz && curr->parent->split_type == SPLIT_HORIZONTAL) ||
				    (!is_horiz && curr->parent->split_type == SPLIT_VERTICAL)) {
					target_node = curr;
					break;
				}
			}
		}
		if (!target_node)
			target_node = sel->node;

		parent = target_node->parent;
		if (parent) {
			if (target_node->link.prev != &parent->children)
				prev_sub = wl_container_of(target_node->link.prev, prev_sub, link);
			if (target_node->link.next != &parent->children)
				next_sub = wl_container_of(target_node->link.next, next_sub, link);

			adjust_node_ratio(target_node, prev_sub, next_sub, dir, is_horiz, delta);
			arrange(selmon);
		}
		return;
	}

	/* 2. Handling flat workspace leaf layouts (tile, master_stack, columns, dwindle, spiral) */
	n = node_collect_leaves(ws ? ws->root : NULL, leaves, 128);
	if (n <= 1)
		return;

	for (i = 0; i < n; i++) {
		if (leaves[i] == sel) {
			idx = i;
			break;
		}
	}
	if (idx < 0)
		return;

	if (lt && (lt->arrange == tile || lt->arrange == master_stack)) {
		if (is_horiz) {
			if (dir == WLR_DIRECTION_RIGHT)
				selmon->mfact = MIN(0.9f, selmon->mfact + delta);
			else
				selmon->mfact = MAX(0.1f, selmon->mfact - delta);
		} else {
			int nm = MIN(n, selmon->nmaster);
			target_node = sel->node;
			prev_node = NULL;
			next_node = NULL;

			if (idx < nm) { /* In Master Column */
				if (idx > 0 && leaves[idx - 1]->node)
					prev_node = leaves[idx - 1]->node;
				if (idx < nm - 1 && leaves[idx + 1]->node)
					next_node = leaves[idx + 1]->node;
			} else { /* In Stack Column */
				if (idx > nm && leaves[idx - 1]->node)
					prev_node = leaves[idx - 1]->node;
				if (idx < n - 1 && leaves[idx + 1]->node)
					next_node = leaves[idx + 1]->node;
			}
			adjust_node_ratio(target_node, prev_node, next_node, dir, is_horiz, delta);
		}
		arrange(selmon);
		return;
	}

	/* 3. Handling dwindle, spiral, fibonacci layouts */
	if (lt && (lt->arrange == dwindle || lt->arrange == spiral || lt->arrange == fibonacci)) {
		int is_dwindle = (lt->arrange == dwindle);
		int start_d = (idx < n - 1) ? idx : (n - 2);
		int target_d = -1;
		int d;

		for (d = start_d; d >= 0; d--) {
			int mode = is_dwindle ? (d % 2) : (d % 4);
			int split_is_horiz = (mode % 2 == 0);
			if (split_is_horiz == is_horiz) {
				target_d = d;
				break;
			}
		}

		if (target_d >= 0 && leaves[target_d] && leaves[target_d]->node) {
			Node *split_node = leaves[target_d]->node;
			int mode = is_dwindle ? (target_d % 2) : (target_d % 4);
			int increase = 0;

			switch (mode) {
			case 0: /* Horizontal: b1 = LEFT, b2 = RIGHT */
				increase = (dir == WLR_DIRECTION_RIGHT);
				break;
			case 1: /* Vertical: b1 = TOP, b2 = BOTTOM */
				increase = (dir == WLR_DIRECTION_DOWN);
				break;
			case 2: /* Horizontal: b1 = RIGHT, b2 = LEFT */
				increase = (dir == WLR_DIRECTION_LEFT);
				break;
			case 3: /* Vertical: b1 = BOTTOM, b2 = TOP */
				increase = (dir == WLR_DIRECTION_UP);
				break;
			}

			if (is_horiz) {
				if (increase)
					split_node->ratio_h = clamp_ratio(split_node->ratio_h + delta);
				else
					split_node->ratio_h = clamp_ratio(split_node->ratio_h - delta);
			} else {
				if (increase)
					split_node->ratio_v = clamp_ratio(split_node->ratio_v + delta);
				else
					split_node->ratio_v = clamp_ratio(split_node->ratio_v - delta);
			}
			arrange(selmon);
		}
		return;
	}

	/* 4. Handling columns layout */
	target_node = sel->node;
	prev_node = (idx > 0 && leaves[idx - 1]->node) ? leaves[idx - 1]->node : NULL;
	next_node = (idx < n - 1 && leaves[idx + 1]->node) ? leaves[idx + 1]->node : NULL;

	adjust_node_ratio(target_node, prev_node, next_node, dir, is_horiz, delta);
	arrange(selmon);
}

void
tree_swap_dir(const Arg *arg)
{
	Client *c, *tc, *best = NULL;
	double cx, cy, tx, ty, dx, dy;
	double min_dist = 1e18;
	int dir;

	if (!selmon || !arg)
		return;

	if (!(c = focustop(selmon)))
		return;

	dir = arg->i;
	cx = c->geom.x + c->geom.width / 2.0;
	cy = c->geom.y + c->geom.height / 2.0;

	wl_list_for_each(tc, &clients, link) {
		double dist;

		if (tc == c || !VISIBLEON(tc, selmon) || tc->isfloating || tc->ws != c->ws)
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
		}
	}

	if (best && c->node && best->node) {
		tree_swap_nodes(c->node, best->node);
		arrange(selmon);
		focusclient(c, 1);
	}
}

void
tree_set_split_type(const Arg *arg)
{
	Client *c;
	Workspace *ws;

	if (!selmon || !arg)
		return;

	c = focustop(selmon);
	ws = (c && c->ws) ? c->ws : selmon->active_workspace;
	if (ws)
		ws->next_split = (SplitType)arg->i;
}

static void
tree_export_node_json(Node *node, FILE *f, int is_focused)
{
	Node *child;
	int first = 1;

	if (!node || !f)
		return;

	fprintf(f, "{\"type\":%d,\"split\":%d,\"rh\":%.3f,\"rv\":%.3f,\"focused\":%d",
		(int)node->type, (int)node->split_type, node->ratio_h, node->ratio_v, is_focused);

	fprintf(f, ",\"geom\":[%d,%d,%d,%d]",
		node->geom.x, node->geom.y, node->geom.width, node->geom.height);

	if (node->type == NODE_LEAF && node->client) {
		const char *title = client_get_title(node->client);
		const char *p;
		if (!title)
			title = "Window";
		fprintf(f, ",\"client\":{\"title\":\"");
		for (p = title; *p; p++) {
			if (*p == '"' || *p == '\\')
				fputc('\\', f);
			if ((unsigned char)*p >= 0x20)
				fputc(*p, f);
		}
		fprintf(f, "\",\"floating\":%d}", node->client->isfloating ? 1 : 0);
	}

	fprintf(f, ",\"children\":[");
	wl_list_for_each(child, &node->children, link) {
		if (!first) fprintf(f, ",");
		tree_export_node_json(child, f, (child == (node->ws ? node->ws->focused_node : NULL)));
		first = 0;
	}
	fprintf(f, "]}");
}

void
tree_export_ipc(Workspace *ws)
{
	FILE *f;
	const char *tmp_path = "/tmp/dwl-tree.state.tmp";
	const char *ipc_path = "/tmp/dwl-tree.state";

	if (!ws || !ws->root)
		return;

	f = fopen(tmp_path, "w");
	if (!f)
		return;

	fprintf(f, "{\"workspace\":%d,\"root\":", ws->id);
	tree_export_node_json(ws->root, f, (ws->root == ws->focused_node));
	fprintf(f, "}\n");

	fclose(f);
	rename(tmp_path, ipc_path);
}



typedef struct {
	Client *client;
	uint32_t grabc_edges;
	double start_x;
	double start_y;
	unsigned int start_gen; /* tree_gen snapshot to detect stale node pointers */

	/* For tile / master_stack layout */
	float start_mfact;
	float mfact_scale;

	/* Horizontal split node pair */
	Node *node_h_left;
	Node *node_h_right;
	float start_ratio_h_left;
	float start_ratio_h_right;
	float parent_w;
	int invert_h;

	/* Vertical split node pair */
	Node *node_v_top;
	Node *node_v_bottom;
	float start_ratio_v_top;
	float start_ratio_v_bottom;
	float parent_h;
	int invert_v;

	/* Single-node ratio scaling (dwindle, spiral, fibonacci) */
	Node *single_node_h;
	float start_single_ratio_h;
	float scale_h;

	Node *single_node_v;
	float start_single_ratio_v;
	float scale_v;
} TiledResizeState;

static TiledResizeState resize_state;

void
tree_mouse_resize_start(Client *c, uint32_t grabc_edges, double cursor_x, double cursor_y)
{
	Workspace *ws;
	const Layout *lt;

	memset(&resize_state, 0, sizeof(resize_state));
	if (!selmon || !c || !c->node)
		return;

	lt = resolve_layout(c, selmon, &ws);

	if (!lt || lt->arrange == monocle)
		return;

	int resize_h = (grabc_edges & (WLR_EDGE_LEFT | WLR_EDGE_RIGHT)) != 0;
	int resize_v = (grabc_edges & (WLR_EDGE_TOP | WLR_EDGE_BOTTOM)) != 0;
	if (!resize_h && !resize_v) {
		resize_h = 1;
		resize_v = 1;
	}

	int is_edge_right = (grabc_edges & WLR_EDGE_RIGHT) != 0;
	int is_edge_left = (grabc_edges & WLR_EDGE_LEFT) != 0;
	int is_edge_bottom = (grabc_edges & WLR_EDGE_BOTTOM) != 0;
	int is_edge_top = (grabc_edges & WLR_EDGE_TOP) != 0;

	if (!is_edge_right && !is_edge_left && resize_h) {
		is_edge_right = (c->geom.width > 0) ? (cursor_x >= c->geom.x + c->geom.width / 2.0) : 1;
		is_edge_left = !is_edge_right;
	}
	if (!is_edge_bottom && !is_edge_top && resize_v) {
		is_edge_bottom = (c->geom.height > 0) ? (cursor_y >= c->geom.y + c->geom.height / 2.0) : 1;
		is_edge_top = !is_edge_bottom;
	}

	resize_state.client = c;
	resize_state.grabc_edges = grabc_edges;
	resize_state.start_x = cursor_x;
	resize_state.start_y = cursor_y;
	resize_state.start_mfact = selmon->mfact;
	resize_state.start_gen = ws ? ws->tree_gen : 0;

	Client *leaves[128];
	int n = node_collect_leaves(ws ? ws->root : NULL, leaves, 128);
	int idx = -1, i;
	for (i = 0; i < n; i++) {
		client_set_resizing(leaves[i], 1);
		if (leaves[i] == c) {
			idx = i;
		}
	}

	/* A. Tile / Master-Stack Layout */
	if (lt->arrange == tile || lt->arrange == master_stack) {
		int nm = MIN(n, selmon->nmaster);
		int is_master = (idx >= 0 && idx < nm);

		if (idx >= 0 && n > 1) {
			if (resize_h) {
				if (is_master) {
					resize_state.mfact_scale = is_edge_right ? 1.0f : -1.0f;
				} else {
					resize_state.mfact_scale = is_edge_left ? -1.0f : 1.0f;
				}
			}

			if (resize_v) {
				Node *prev_node = NULL, *next_node = NULL;
				if (is_master) {
					if (idx > 0 && leaves[idx - 1]->node)
						prev_node = leaves[idx - 1]->node;
					if (idx < nm - 1 && leaves[idx + 1]->node)
						next_node = leaves[idx + 1]->node;
				} else {
					if (idx > nm && leaves[idx - 1]->node)
						prev_node = leaves[idx - 1]->node;
					if (idx < n - 1 && leaves[idx + 1]->node)
						next_node = leaves[idx + 1]->node;
				}

				if (is_edge_bottom) {
					if (next_node) {
						resize_state.node_v_top = c->node;
						resize_state.node_v_bottom = next_node;
						resize_state.invert_v = 0;
					} else if (prev_node) {
						resize_state.node_v_top = prev_node;
						resize_state.node_v_bottom = c->node;
						resize_state.invert_v = 1;
					}
				} else { /* is_edge_top */
					if (prev_node) {
						resize_state.node_v_top = prev_node;
						resize_state.node_v_bottom = c->node;
						resize_state.invert_v = 1;
					} else if (next_node) {
						resize_state.node_v_top = c->node;
						resize_state.node_v_bottom = next_node;
						resize_state.invert_v = 0;
					}
				}

				if (resize_state.node_v_top && resize_state.node_v_bottom) {
					resize_state.start_ratio_v_top = resize_state.node_v_top->ratio_v;
					resize_state.start_ratio_v_bottom = resize_state.node_v_bottom->ratio_v;
					resize_state.parent_h = (selmon->w.height > 0) ? (float)selmon->w.height : 1080.0f;
				}
			}
		}
		return;
	}

	/* B. Columns Layout */
	if (lt->arrange == columns) {
		if (idx >= 0 && n > 1 && resize_h) {
			Node *prev_node = (idx > 0 && leaves[idx - 1]->node) ? leaves[idx - 1]->node : NULL;
			Node *next_node = (idx < n - 1 && leaves[idx + 1]->node) ? leaves[idx + 1]->node : NULL;

			if (is_edge_right) {
				if (next_node) {
					resize_state.node_h_left = c->node;
					resize_state.node_h_right = next_node;
					resize_state.invert_h = 0;
				} else if (prev_node) {
					resize_state.node_h_left = prev_node;
					resize_state.node_h_right = c->node;
					resize_state.invert_h = 1;
				}
			} else { /* is_edge_left */
				if (prev_node) {
					resize_state.node_h_left = prev_node;
					resize_state.node_h_right = c->node;
					resize_state.invert_h = 1;
				} else if (next_node) {
					resize_state.node_h_left = c->node;
					resize_state.node_h_right = next_node;
					resize_state.invert_h = 0;
				}
			}

			if (resize_state.node_h_left && resize_state.node_h_right) {
				resize_state.start_ratio_h_left = resize_state.node_h_left->ratio_h;
				resize_state.start_ratio_h_right = resize_state.node_h_right->ratio_h;
				resize_state.parent_w = (selmon->w.width > 0) ? (float)selmon->w.width : 1920.0f;
			}
		}
		return;
	}

	/* C. Spiral & Fibonacci Layouts */
	if (lt->arrange == spiral || lt->arrange == fibonacci) {
		if (idx >= 0 && n > 1) {
			int is_dwindle = (lt->arrange == dwindle);
			int start_d = (idx < n - 1) ? idx : (n - 2);
			int d_h = -1, d_v = -1, d;

			for (d = start_d; d >= 0; d--) {
				int mode = is_dwindle ? (d % 2) : (d % 4);
				if (mode % 2 == 0 && d_h < 0)
					d_h = d;
				else if (mode % 2 == 1 && d_v < 0)
					d_v = d;
			}

			if (resize_h && d_h >= 0 && leaves[d_h] && leaves[d_h]->node) {
				Node *split_node = leaves[d_h]->node;
				int mode = is_dwindle ? (d_h % 2) : (d_h % 4);
				resize_state.single_node_h = split_node;
				resize_state.start_single_ratio_h = split_node->ratio_h;

				float base_scale = (mode == 0) ? 1.0f : -1.0f;
				if (idx != d_h)
					base_scale = -base_scale;
				if (is_edge_left)
					base_scale = -base_scale;

				resize_state.scale_h = base_scale;
			}

			if (resize_v && d_v >= 0 && leaves[d_v] && leaves[d_v]->node) {
				Node *split_node = leaves[d_v]->node;
				int mode = is_dwindle ? (d_v % 2) : (d_v % 4);
				resize_state.single_node_v = split_node;
				resize_state.start_single_ratio_v = split_node->ratio_v;

				float base_scale = (mode == 1) ? 1.0f : -1.0f;
				if (idx != d_v)
					base_scale = -base_scale;
				if (is_edge_top)
					base_scale = -base_scale;

				resize_state.scale_v = base_scale;
			}
		}
		return;
	}

	/* D. Tree layout / BSP (hierarchical N-ary split tree) */
	Node *curr;

	/* 1. Horizontal Split Selection */
	if (resize_h) {
		for (curr = c->node; curr; curr = curr->parent) {
			Node *parent = curr->parent;
			if (!parent || parent->split_type != SPLIT_HORIZONTAL)
				continue;

			Node *prev_sub = (curr->link.prev != &parent->children) ? wl_container_of(curr->link.prev, prev_sub, link) : NULL;
			Node *next_sub = (curr->link.next != &parent->children) ? wl_container_of(curr->link.next, next_sub, link) : NULL;

			if (is_edge_right) {
				if (next_sub) {
					resize_state.node_h_left = curr;
					resize_state.node_h_right = next_sub;
					resize_state.invert_h = 0;
					break;
				} else if (prev_sub) {
					resize_state.node_h_left = prev_sub;
					resize_state.node_h_right = curr;
					resize_state.invert_h = 1;
					break;
				}
			} else { /* is_edge_left */
				if (prev_sub) {
					resize_state.node_h_left = prev_sub;
					resize_state.node_h_right = curr;
					resize_state.invert_h = 0;
					break;
				} else if (next_sub) {
					resize_state.node_h_left = curr;
					resize_state.node_h_right = next_sub;
					resize_state.invert_h = 1;
					break;
				}
			}
		}

		if (resize_state.node_h_left && resize_state.node_h_right) {
			Node *parent = resize_state.node_h_left->parent;
			resize_state.start_ratio_h_left = resize_state.node_h_left->ratio_h;
			resize_state.start_ratio_h_right = resize_state.node_h_right->ratio_h;
			resize_state.parent_w = (parent && parent->geom.width > 0) ? (float)parent->geom.width : (selmon->w.width > 0 ? (float)selmon->w.width : 1920.0f);
		}
	}

	/* 2. Vertical Split Selection */
	if (resize_v) {
		for (curr = c->node; curr; curr = curr->parent) {
			Node *parent = curr->parent;
			if (!parent || parent->split_type != SPLIT_VERTICAL)
				continue;

			Node *prev_sub = (curr->link.prev != &parent->children) ? wl_container_of(curr->link.prev, prev_sub, link) : NULL;
			Node *next_sub = (curr->link.next != &parent->children) ? wl_container_of(curr->link.next, next_sub, link) : NULL;

			if (is_edge_bottom) {
				if (next_sub) {
					resize_state.node_v_top = curr;
					resize_state.node_v_bottom = next_sub;
					resize_state.invert_v = 0;
					break;
				} else if (prev_sub) {
					resize_state.node_v_top = prev_sub;
					resize_state.node_v_bottom = curr;
					resize_state.invert_v = 1;
					break;
				}
			} else { /* is_edge_top */
				if (prev_sub) {
					resize_state.node_v_top = prev_sub;
					resize_state.node_v_bottom = curr;
					resize_state.invert_v = 0;
					break;
				} else if (next_sub) {
					resize_state.node_v_top = curr;
					resize_state.node_v_bottom = next_sub;
					resize_state.invert_v = 1;
					break;
				}
			}
		}

		if (resize_state.node_v_top && resize_state.node_v_bottom) {
			Node *parent = resize_state.node_v_top->parent;
			resize_state.start_ratio_v_top = resize_state.node_v_top->ratio_v;
			resize_state.start_ratio_v_bottom = resize_state.node_v_bottom->ratio_v;
			resize_state.parent_h = (parent && parent->geom.height > 0) ? (float)parent->geom.height : (selmon->w.height > 0 ? (float)selmon->w.height : 1080.0f);
		}
	}
}

static void
apply_abs_sibling_ratios(Node *node_first, Node *node_second, int is_horiz,
		float start_r1, float start_r2, float delta, int invert)
{
	float min_weight = 0.05f;
	float sum = start_r1 + start_r2;
	if (invert)
		delta = -delta;
	float r1 = start_r1 + delta;
	float r2 = start_r2 - delta;

	if (r1 < min_weight) {
		r1 = min_weight;
		r2 = sum - min_weight;
	} else if (r2 < min_weight) {
		r2 = min_weight;
		r1 = sum - min_weight;
	}

	if (is_horiz) {
		node_first->ratio_h = clamp_ratio(r1);
		node_second->ratio_h = clamp_ratio(r2);
	} else {
		node_first->ratio_v = clamp_ratio(r1);
		node_second->ratio_v = clamp_ratio(r2);
	}
}

void
tree_mouse_resize(Client *c, double cursor_x, double cursor_y)
{
	Workspace *ws;
	const Layout *lt;
	float total_dx, total_dy;

	if (!selmon || !c || !c->node || c != resize_state.client)
		return;

	lt = resolve_layout(c, selmon, &ws);

	if (!lt || lt->arrange == monocle)
		return;

	/* Bail if the tree was mutated since resize-start (e.g. client unmapped) */
	if (ws && ws->tree_gen != resize_state.start_gen) {
		memset(&resize_state, 0, sizeof(resize_state));
		return;
	}

	total_dx = (float)(cursor_x - resize_state.start_x);
	total_dy = (float)(cursor_y - resize_state.start_y);

	/* 1. Tile & Master-Stack Layout */
	if (lt->arrange == tile || lt->arrange == master_stack) {
		if (resize_state.mfact_scale != 0.0f) {
			float mon_w = (selmon->w.width > 0) ? (float)selmon->w.width : 1920.0f;
			float avail_w = MAX(1.0f, mon_w - (float)gappx);
			selmon->mfact = MIN(0.9f, MAX(0.1f, resize_state.start_mfact + (total_dx / avail_w) * resize_state.mfact_scale));
		}

		if (resize_state.node_v_top && resize_state.node_v_bottom && resize_state.parent_h > 0.0f) {
			float delta_r = total_dy / resize_state.parent_h;
			apply_abs_sibling_ratios(resize_state.node_v_top, resize_state.node_v_bottom, 0,
					resize_state.start_ratio_v_top, resize_state.start_ratio_v_bottom, delta_r, resize_state.invert_v);
		}
		arrange(selmon);
		return;
	}

	/* 2. Spiral & Fibonacci Layouts */
	if (lt->arrange == spiral || lt->arrange == fibonacci) {
		float mon_w = (selmon->w.width > 0) ? (float)selmon->w.width : 1920.0f;
		float mon_h = (selmon->w.height > 0) ? (float)selmon->w.height : 1080.0f;

		if (resize_state.single_node_h && resize_state.scale_h != 0.0f) {
			float delta = (total_dx / mon_w) * resize_state.scale_h;
			resize_state.single_node_h->ratio_h = clamp_ratio(resize_state.start_single_ratio_h + delta);
		}
		if (resize_state.single_node_v && resize_state.scale_v != 0.0f) {
			float delta = (total_dy / mon_h) * resize_state.scale_v;
			resize_state.single_node_v->ratio_v = clamp_ratio(resize_state.start_single_ratio_v + delta);
		}
		arrange(selmon);
		return;
	}

	/* 3. Tree Layout (RT), BSP, Columns, and default node-pair layouts */
	if (resize_state.node_h_left && resize_state.node_h_right && resize_state.parent_w > 0.0f) {
		float delta_r = total_dx / resize_state.parent_w;
		apply_abs_sibling_ratios(resize_state.node_h_left, resize_state.node_h_right, 1,
				resize_state.start_ratio_h_left, resize_state.start_ratio_h_right, delta_r, resize_state.invert_h);
	}

	if (resize_state.node_v_top && resize_state.node_v_bottom && resize_state.parent_h > 0.0f) {
		float delta_r = total_dy / resize_state.parent_h;
		apply_abs_sibling_ratios(resize_state.node_v_top, resize_state.node_v_bottom, 0,
				resize_state.start_ratio_v_top, resize_state.start_ratio_v_bottom, delta_r, resize_state.invert_v);
	}

	arrange(selmon);
}

void
tree_mouse_resize_end(void)
{
	if (!resize_state.client)
		return;
	Workspace *ws = resize_state.client->ws ? resize_state.client->ws : (selmon ? selmon->active_workspace : NULL);
	if (ws && ws->root) {
		Client *leaves[128];
		int n = node_collect_leaves(ws->root, leaves, 128);
		int i;
		for (i = 0; i < n; i++) {
			client_set_resizing(leaves[i], 0);
		}
	}
	memset(&resize_state, 0, sizeof(resize_state));
	if (selmon)
		arrange(selmon);
}

