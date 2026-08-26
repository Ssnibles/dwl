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
	node->ratio_h = 1.0f;
	node->ratio_v = 1.0f;
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

static void
node_collapse_container(Node *container)
{
	Node *only_child, *gparent;

	if (!container || container->type != NODE_CONTAINER || container->type == NODE_ROOT)
		return;

	if (wl_list_empty(&container->children) || container->children.next != container->children.prev)
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
	if (ws) {
		if (ws->focused_node == node || node_is_ancestor(node, ws->focused_node)) {
			Node *sibling = NULL;
			if (node->link.next != &parent->children)
				sibling = wl_container_of(node->link.next, sibling, link);
			else if (node->link.prev != &parent->children)
				sibling = wl_container_of(node->link.prev, sibling, link);

			ws->focused_node = sibling ? sibling : (parent ? parent : ws->root);
		}
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

	if (node->type == NODE_LEAF) {
		Client *c = node->client;
		if (c && VISIBLEON(c, c->mon) && !c->isfloating && !c->isfullscreen)
			return 1;
		return 0;
	}

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

	if (!node || count >= max)
		return 0;

	if (node->type == NODE_LEAF) {
		Client *c = node->client;
		if (c && VISIBLEON(c, c->mon) && !c->isfloating && !c->isfullscreen) {
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
		if (c && c->mon && VISIBLEON(c, c->mon) && !c->isfloating && !c->isfullscreen) {
			struct wlr_box gbox = {
				.x = box.x + g,
				.y = box.y + g,
				.width = MAX(1, box.width - 2 * g),
				.height = MAX(1, box.height - 2 * g)
			};
			resize(c, gbox, 0);
		}
		return;
	}

	wl_list_for_each(child, &node->children, link) {
		float r;
		child_count++;
		r = (node->split_type == SPLIT_HORIZONTAL) ? child->ratio_h : child->ratio_v;
		total_ratio += (r > 0.05f) ? r : 1.0f;
	}

	if (child_count == 0)
		return;

	if (total_ratio <= 0.0f)
		total_ratio = (float)child_count;

	wl_list_for_each(child, &node->children, link) {
		struct wlr_box child_box = box;
		float r = (node->split_type == SPLIT_HORIZONTAL) ? child->ratio_h : child->ratio_v;
		float child_weight = (r > 0.05f) ? r : 1.0f;

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

	node->ratio_h = 1.0f;
	node->ratio_v = 1.0f;
	wl_list_for_each(child, &node->children, link) {
		tree_equalize_node(child);
	}
}

void
tree_equalize_active(const Arg *arg)
{
	if (selmon && selmon->active_workspace && selmon->active_workspace->root) {
		tree_equalize_node(selmon->active_workspace->root);
		arrange(selmon);
	}
}

void
tree_resize_active(const Arg *arg)
{
	Client *sel;

	if (!selmon || !arg || !selmon->active_workspace)
		return;

	sel = focustop(selmon);
	if (sel && sel->node) {
		tree_resize_node(sel->node, arg->f);
		if (selmon->lt[selmon->sellt]->arrange == tile || selmon->lt[selmon->sellt]->arrange == master_stack) {
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
	float delta = 0.05f, f;
	Node *target_node = NULL;
	Node *curr;
	int dir, is_horiz;
	int is_right_half = 0, is_bottom_half = 0;
	struct wlr_box ref_box;
	double cx, cy;

	if (!selmon || !arg || !selmon->active_workspace)
		return;

	sel = focustop(selmon);
	if (!sel)
		return;

	if (sel->isfloating) {
		int step = 40;
		struct wlr_box g = sel->geom;
		switch (arg->i) {
		case WLR_DIRECTION_RIGHT:
			g.width += step;
			break;
		case WLR_DIRECTION_LEFT:
			g.width = MAX(100, g.width - step);
			break;
		case WLR_DIRECTION_DOWN:
			g.height += step;
			break;
		case WLR_DIRECTION_UP:
			g.height = MAX(100, g.height - step);
			break;
		}
		resize(sel, g, 1);
		return;
	}

	if (!sel->node)
		return;

	dir = arg->i;
	is_horiz = (dir == WLR_DIRECTION_LEFT || dir == WLR_DIRECTION_RIGHT);

	if (selmon->lt[selmon->sellt]->arrange == tree_layout) {
		for (curr = sel->node; curr; curr = curr->parent) {
			if (curr->parent && curr->parent->split_type != SPLIT_NONE) {
				if ((is_horiz && curr->parent->split_type == SPLIT_HORIZONTAL) ||
				    (!is_horiz && curr->parent->split_type == SPLIT_VERTICAL)) {
					target_node = curr;
					break;
				}
			}
		}
	}

	if (!target_node)
		target_node = sel->node;

	ref_box = (target_node->parent && target_node->parent->type != NODE_ROOT && target_node->parent->geom.width > 0)
		? target_node->parent->geom
		: selmon->w;

	cx = (target_node->geom.width > 0)
		? (target_node->geom.x + target_node->geom.width / 2.0)
		: (sel->geom.x + sel->geom.width / 2.0);
	cy = (target_node->geom.height > 0)
		? (target_node->geom.y + target_node->geom.height / 2.0)
		: (sel->geom.y + sel->geom.height / 2.0);

	is_right_half = (cx > (ref_box.x + ref_box.width / 2.0));
	is_bottom_half = (cy > (ref_box.y + ref_box.height / 2.0));

	if (is_horiz) {
		if (dir == WLR_DIRECTION_LEFT)
			target_node->ratio_h += is_right_half ? delta : -delta;
		else
			target_node->ratio_h += is_right_half ? -delta : delta;

		target_node->ratio_h = clamp_ratio(target_node->ratio_h);

		if (selmon->lt[selmon->sellt]->arrange == tile || selmon->lt[selmon->sellt]->arrange == master_stack) {
			if (dir == WLR_DIRECTION_RIGHT)
				f = selmon->mfact + delta;
			else
				f = selmon->mfact - delta;

			if (f >= 0.1f && f <= 0.9f)
				selmon->mfact = f;
		}
	} else {
		if (dir == WLR_DIRECTION_UP)
			target_node->ratio_v += is_bottom_half ? delta : -delta;
		else
			target_node->ratio_v += is_bottom_half ? -delta : delta;

		target_node->ratio_v = clamp_ratio(target_node->ratio_v);
	}

	arrange(selmon);
}

void
tree_swap_dir(const Arg *arg)
{
	Client *c, *tc, *best = NULL;
	double cx, cy, tx, ty, dx, dy;
	double min_dist = 1e18;
	int dir;

	if (!selmon || !arg || !selmon->active_workspace)
		return;

	if (!(c = focustop(selmon)))
		return;

	dir = arg->i;
	cx = c->geom.x + c->geom.width / 2.0;
	cy = c->geom.y + c->geom.height / 2.0;

	wl_list_for_each(tc, &clients, link) {
		double dist;

		if (tc == c || !VISIBLEON(tc, selmon) || tc->isfloating)
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
	if (!selmon || !selmon->active_workspace || !arg)
		return;

	selmon->active_workspace->next_split = (SplitType)arg->i;
}
