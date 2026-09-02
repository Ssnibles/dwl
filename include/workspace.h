/*
 * See LICENSE file for copyright and license details.
 */
#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <wayland-server-core.h>

/* Forward declarations */
struct Monitor;
struct Client;
typedef struct Layout Layout;
typedef union Arg Arg;

typedef struct Workspace Workspace;
struct Workspace {
	int id;                     /* Numeric ID (e.g. 1..9) */
	char name[32];              /* Display name (e.g., "1", "2") */
	struct Monitor *mon;        /* Parent monitor pointer */
	const Layout *layout;       /* Active layout algorithm */
	float mfact;                /* Master area size factor */
	int nmaster;                /* Number of master windows */
	int dir;                    /* Layout orientation: 0=Left, 1=Top, 2=Right, 3=Bottom */
	struct wl_list link;        /* Linked list entry in Monitor->workspaces */
};

/* Workspace lifecycle & operations */
Workspace *workspace_create(struct Monitor *m, int id, const char *name);
void workspace_destroy(Workspace *ws);
Workspace *workspace_get_by_id(struct Monitor *m, int id);

#define SCRATCHPAD_WORKSPACE 0

/* Keybinding handlers */
void view_workspace(const Arg *arg);
void move_to_workspace(const Arg *arg);
void togglescratchpad_client(const Arg *arg);
void togglescratchpad_view(const Arg *arg);

int scratchpad_client_count(const struct Monitor *m);

#endif /* WORKSPACE_H */
