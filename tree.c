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
	} else if (target_parent->split_type == SPLIT_HORIZONTAL) {
		desired_split = SPLIT_VERTICAL;
	} else {
		desired_split = SPLIT_HORIZONTAL;
	}

	if (target_parent->split_type != desired_split && focus_ref->type == NODE_LEAF) {
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
	return (r > 0.05f) ? r : 1.0f;
}

Node *
node_find_client(Node *root, Client *c)
{
	if (!root || !c || !c->node)
		return NULL;

	if (node_is_ancestor(root, c->node))
		return c->node;

	return NULL;
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
adjust_node_ratio(Node *target, Node *prev, Node *next, int dir, int is_horiz, float delta)
{
	float min_weight = 0.15f;
	float act_d;

	if (!target)
		return;

	if (is_horiz) {
		if (dir == WLR_DIRECTION_LEFT) {
			if (prev) {
				float cur_w = prev->ratio_h;
				act_d = MIN(delta, MAX(0.0f, cur_w - min_weight));
				if (act_d > 0.001f) {
					prev->ratio_h -= act_d;
					target->ratio_h += act_d;
				}
			} else if (next) {
				float cur_w = target->ratio_h;
				act_d = MIN(delta, MAX(0.0f, cur_w - min_weight));
				if (act_d > 0.001f) {
					target->ratio_h -= act_d;
					next->ratio_h += act_d;
				}
			} else {
				target->ratio_h -= delta;
			}
		} else if (dir == WLR_DIRECTION_RIGHT) {
			if (next) {
				float cur_w = next->ratio_h;
				act_d = MIN(delta, MAX(0.0f, cur_w - min_weight));
				if (act_d > 0.001f) {
					next->ratio_h -= act_d;
					target->ratio_h += act_d;
				}
			} else if (prev) {
				float cur_w = target->ratio_h;
				act_d = MIN(delta, MAX(0.0f, cur_w - min_weight));
				if (act_d > 0.001f) {
					target->ratio_h -= act_d;
					prev->ratio_h += act_d;
				}
			} else {
				target->ratio_h += delta;
			}
		}
	} else {
		if (dir == WLR_DIRECTION_UP) {
			if (prev) {
				float cur_w = prev->ratio_v;
				act_d = MIN(delta, MAX(0.0f, cur_w - min_weight));
				if (act_d > 0.001f) {
					prev->ratio_v -= act_d;
					target->ratio_v += act_d;
				}
			} else if (next) {
				float cur_w = target->ratio_v;
				act_d = MIN(delta, MAX(0.0f, cur_w - min_weight));
				if (act_d > 0.001f) {
					target->ratio_v -= act_d;
					next->ratio_v += act_d;
				}
			} else {
				target->ratio_v -= delta;
			}
		} else if (dir == WLR_DIRECTION_DOWN) {
			if (next) {
				float cur_w = next->ratio_v;
				act_d = MIN(delta, MAX(0.0f, cur_w - min_weight));
				if (act_d > 0.001f) {
					next->ratio_v -= act_d;
					target->ratio_v += act_d;
				}
			} else if (prev) {
				float cur_w = target->ratio_v;
				act_d = MIN(delta, MAX(0.0f, cur_w - min_weight));
				if (act_d > 0.001f) {
					target->ratio_v -= act_d;
					prev->ratio_v += act_d;
				}
			} else {
				target->ratio_v += delta;
			}
		}
	}

	target->ratio_h = clamp_ratio(target->ratio_h);
	target->ratio_v = clamp_ratio(target->ratio_v);
	if (prev) {
		prev->ratio_h = clamp_ratio(prev->ratio_h);
		prev->ratio_v = clamp_ratio(prev->ratio_v);
	}
	if (next) {
		next->ratio_h = clamp_ratio(next->ratio_h);
		next->ratio_v = clamp_ratio(next->ratio_v);
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

	ws = sel->ws ? sel->ws : selmon->active_workspace;
	lt = (ws && ws->layout) ? ws->layout : selmon->lt[selmon->sellt];

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
	ws = sel->ws ? sel->ws : selmon->active_workspace;
	lt = (ws && ws->layout) ? ws->layout : selmon->lt[selmon->sellt];

	if (lt && lt->arrange == monocle)
		return;

	/* 1. Handling tree_layout & bsp_layout */
	if (lt && (lt->arrange == tree_layout || lt->arrange == bsp_layout)) {
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
