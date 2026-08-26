/*
 * See LICENSE file for copyright and license details.
 */
#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <wayland-server-core.h>
#include "tree.h"

/* Forward declarations */
struct Monitor;
struct Client;
typedef struct Layout Layout;

typedef struct Workspace Workspace;
struct Workspace {
	int id;                     /* Numeric ID (e.g. 1..9) */
	char name[32];              /* Display name (e.g., "1", "2") */
	struct Monitor *mon;        /* Parent monitor pointer */
	Node *root;                 /* Root node of workspace N-ary tree */
	Node *focused_node;         /* Focused leaf or container node */
	SplitType next_split;       /* Manual split direction for next window */
	const Layout *layout;       /* Active layout algorithm */
	struct wl_list link;        /* Linked list entry in Monitor->workspaces */
};

/* Workspace lifecycle & operations */
Workspace *workspace_create(struct Monitor *m, int id, const char *name);
void workspace_destroy(Workspace *ws);
void workspace_switch(Workspace *ws);
void client_move_to_workspace(struct Client *c, Workspace *ws);
Workspace *workspace_get_by_id(struct Monitor *m, int id);

/* Keybinding handlers */
void view_workspace(const union Arg *arg);
void move_to_workspace(const union Arg *arg);

#endif /* WORKSPACE_H */
