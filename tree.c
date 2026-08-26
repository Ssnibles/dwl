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
	node->ratio = 1.0f;
	node->ratio_h = 1.0f;
	node->ratio_v = 1.0f;
	node->ws = ws;
	node->client = NULL;
	node->parent = NULL;
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
	wl_list_insert(parent->children.prev, &child->link);
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
	c->ws = ws;

	/* Case 1: Empty workspace root */
	if (wl_list_empty(&ws->root->children)) {
		node_insert_child(ws->root, leaf);
		ws->focused_node = leaf;
		return leaf;
	}

	/* Case 2: Identify focus reference node */
	focus_ref = (ws->focused_node && ws->focused_node != ws->root) ? ws->focused_node : NULL;
	if (!focus_ref) {
		Node *tmp;
		wl_list_for_each(tmp, &ws->root->children, link) {
			focus_ref = tmp;
		}
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

	if (target_parent->split_type == desired_split) {
		leaf->parent = target_parent;
		leaf->ws = ws;
		wl_list_insert(&focus_ref->link, &leaf->link);
	} else if (focus_ref->type == NODE_LEAF) {
		Node *container = node_create(NODE_CONTAINER, ws);
		container->split_type = desired_split;
		container->parent = target_parent;

		wl_list_insert(&focus_ref->link, &container->link);
		wl_list_remove(&focus_ref->link);

		node_insert_child(container, focus_ref);
		node_insert_child(container, leaf);
	} else {
		leaf->parent = target_parent;
		leaf->ws = ws;
		wl_list_insert(&focus_ref->link, &leaf->link);
	}

	ws->focused_node = leaf;
	return leaf;
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

	/* If parent container is empty and not root, prune parent */
	if (parent && parent->type == NODE_CONTAINER && wl_list_empty(&parent->children)) {
		node_remove(parent);
	}
	/* If parent container has only 1 child and is not root, collapse parent */
	else if (parent && parent->type == NODE_CONTAINER && parent->type != NODE_ROOT && wl_list_length(&parent->children) == 1) {
		Node *only_child = wl_container_of(parent->children.next, only_child, link);
		Node *gparent = parent->parent;
		if (gparent) {
			wl_list_insert(&parent->link, &only_child->link);
			wl_list_remove(&parent->link);
			wl_list_init(&parent->link);
			only_child->parent = gparent;
			if (ws && ws->focused_node == parent)
				ws->focused_node = only_child;
			free(parent);
		}
	}
}

Node *
node_find_client(Node *root, Client *c)
{
	Node *child, *found;

	if (!root || !c)
		return NULL;
	if (root->client == c)
		return root;

	wl_list_for_each(child, &root->children, link) {
		found = node_find_client(child, c);
		if (found)
			return found;
	}
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
		child_count++;
		float r = (node->split_type == SPLIT_HORIZONTAL) ? child->ratio_h : child->ratio_v;
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
	node->ratio += delta;
	node->ratio_h += delta;
	node->ratio_v += delta;
	if (node->ratio < 0.1f) node->ratio = 0.1f;
	if (node->ratio > 10.0f) node->ratio = 10.0f;
	if (node->ratio_h < 0.1f) node->ratio_h = 0.1f;
	if (node->ratio_h > 10.0f) node->ratio_h = 10.0f;
	if (node->ratio_v < 0.1f) node->ratio_v = 0.1f;
	if (node->ratio_v > 10.0f) node->ratio_v = 10.0f;
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

	node->ratio = 1.0f;
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
	float delta = 0.05f;
	Node *target_node = NULL;
	Node *curr;

	if (!selmon || !arg || !selmon->active_workspace)
		return;

	sel = focustop(selmon);
	if (!sel || !sel->node)
		return;

	int is_horiz = (arg->i == WLR_DIRECTION_LEFT || arg->i == WLR_DIRECTION_RIGHT);

	/* Traverse upward to find the node participating in the requested split orientation */
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

	struct wlr_box ref_box = (target_node->parent && target_node->parent->type != NODE_ROOT && target_node->parent->geom.width > 0)
		? target_node->parent->geom
		: selmon->w;

	double cx = (target_node->geom.width > 0) ? (target_node->geom.x + target_node->geom.width / 2.0) : (sel->geom.x + sel->geom.width / 2.0);
	double cy = (target_node->geom.height > 0) ? (target_node->geom.y + target_node->geom.height / 2.0) : (sel->geom.y + sel->geom.height / 2.0);

	int is_right_half = cx > (ref_box.x + ref_box.width / 2.0);
	int is_bottom_half = cy > (ref_box.y + ref_box.height / 2.0);

	switch (arg->i) {
	case WLR_DIRECTION_LEFT:
		target_node->ratio_h += is_right_half ? delta : -delta;
		break;
	case WLR_DIRECTION_RIGHT:
		target_node->ratio_h += is_right_half ? -delta : delta;
		break;
	case WLR_DIRECTION_UP:
		target_node->ratio_v += is_bottom_half ? delta : -delta;
		break;
	case WLR_DIRECTION_DOWN:
		target_node->ratio_v += is_bottom_half ? -delta : delta;
		break;
	}

	if (target_node->ratio_h < 0.1f) target_node->ratio_h = 0.1f;
	if (target_node->ratio_h > 10.0f) target_node->ratio_h = 10.0f;
	if (target_node->ratio_v < 0.1f) target_node->ratio_v = 0.1f;
	if (target_node->ratio_v > 10.0f) target_node->ratio_v = 10.0f;
	target_node->ratio = (target_node->ratio_h + target_node->ratio_v) / 2.0f;

	if (selmon->lt[selmon->sellt]->arrange == tile || selmon->lt[selmon->sellt]->arrange == master_stack) {
		if (arg->i == WLR_DIRECTION_LEFT || arg->i == WLR_DIRECTION_RIGHT) {
			float f = selmon->mfact + ((arg->i == WLR_DIRECTION_RIGHT && !is_right_half) || (arg->i == WLR_DIRECTION_LEFT && is_right_half) ? delta : -delta);
			if (f >= 0.1f && f <= 0.9f)
				selmon->mfact = f;
		}
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
		int in_dir = 0;
		double primary = 0, secondary = 0, dist;

		if (tc == c || !VISIBLEON(tc, selmon) || tc->isfloating)
			continue;

		tx = tc->geom.x + tc->geom.width / 2.0;
		ty = tc->geom.y + tc->geom.height / 2.0;
		dx = tx - cx;
		dy = ty - cy;

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

		if (in_dir) {
			dist = primary * primary + 3.0 * secondary * secondary;
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
