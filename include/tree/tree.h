/*
 * See LICENSE file for copyright and license details.
 */
#ifndef TREE_TREE_H
#define TREE_TREE_H

#include <wlr/util/box.h>
#include <wayland-server-core.h>

/* Forward declarations */
struct Client;
struct Monitor;
struct Workspace;
union Arg;

typedef enum {
	NODE_ROOT,
	NODE_CONTAINER,
	NODE_LEAF
} NodeType;

typedef enum {
	SPLIT_NONE,
	SPLIT_HORIZONTAL,
	SPLIT_VERTICAL,
	SPLIT_TABBED,
	SPLIT_STACKED
} SplitType;

typedef struct Node Node;
struct Node {
	NodeType type;             /* NODE_ROOT, NODE_CONTAINER, or NODE_LEAF */
	SplitType split_type;      /* Direction/mode children split in container */
	float ratio_h;             /* Horizontal ratio / weight (default 1.0f) */
	float ratio_v;             /* Vertical ratio / weight (default 1.0f) */
	struct wlr_box geom;       /* Computed absolute geometry on screen */

	Node *parent;              /* Parent container/root (NULL if root) */
	struct wl_list children;   /* List of child Node structures (via child->link) */
	struct wl_list link;       /* Entry in parent's children list */

	struct Client *client;     /* Non-NULL only for NODE_LEAF nodes */
	struct Workspace *ws;      /* Back-reference to parent workspace */
};

static inline float
clamp_ratio(float r)
{
	if (r < 0.05f) return 0.05f;
	if (r > 0.95f) return 0.95f;
	return r;
}

/* Returns non-zero if node has children list containing exactly one node */
static inline int
node_has_single_child(const Node *node)
{
	return node && !wl_list_empty(&node->children) && (node->children.next == node->children.prev);
}

/* N-ary Tree Lifecycle & Management */
Node *node_create(NodeType type, struct Workspace *ws);
void node_insert_child(Node *parent, Node *child);
void node_insert_after(Node *sibling, Node *child);
int node_is_ancestor(Node *ancestor, Node *node);
Node *node_insert_client(struct Workspace *ws, struct Client *c);
Node *node_insert_client_at(struct Workspace *ws, struct Client *c, struct Client *at, int dir);
void node_remove(Node *node);
Node *node_find_client(Node *root, struct Client *c);
int node_count_leaves(Node *node);
int node_collect_leaves(Node *node, struct Client **array, int max);
void node_arrange_recursive(Node *node, struct wlr_box box);
void node_free_tree(Node *node);

/* Live IPC Exporter */
void tree_export_ipc(struct Workspace *ws);

/* Tree Manipulation & Sizing Helpers */
void tree_resize_node(Node *node, float delta);
void tree_swap_nodes(Node *a, Node *b);
void tree_equalize_node(Node *node);

/* User-facing Action & Keybinding Callbacks */
void tree_mouse_resize_start(struct Client *c, uint32_t grabc_edges, double cursor_x, double cursor_y);
void tree_mouse_resize(struct Client *c, double cursor_x, double cursor_y);
void tree_mouse_resize_end(void);
void tree_swap_dir(const union Arg *arg);
void tree_resize_active(const union Arg *arg);
void tree_resize_dir(const union Arg *arg);
void tree_equalize_active(const union Arg *arg);
void tree_set_split_type(const union Arg *arg);

#endif /* TREE_TREE_H */
