/*
 * See LICENSE file for copyright and license details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dwl.h"
#include "client.h"
#include "layout.h"
#include "config.h"
#include "util.h"
#include "tree.h"
#include "workspace.h"

Workspace *
workspace_create(Monitor *m, int id, const char *name)
{
	Workspace *ws;

	if (!m)
		return NULL;

	ws = ecalloc(1, sizeof(Workspace));
	ws->id = id;
	if (name)
		snprintf(ws->name, sizeof(ws->name), "%s", name);
	else
		snprintf(ws->name, sizeof(ws->name), "%d", id);

	ws->mon = m;
	ws->root = node_create(NODE_ROOT, ws);
	ws->focused_node = ws->root;
	ws->layout = &layouts[0];
	wl_list_init(&ws->link);
	wl_list_insert(m->workspaces.prev, &ws->link);

	return ws;
}

void
workspace_destroy(Workspace *ws)
{
	if (!ws)
		return;

	if (ws->mon) {
		if (ws->mon->active_workspace == ws)
			ws->mon->active_workspace = NULL;
		if (ws->mon->overview_prev_ws == ws)
			ws->mon->overview_prev_ws = NULL;
	}

	wl_list_remove(&ws->link);
	wl_list_init(&ws->link);

	if (ws->root)
		node_free_tree(ws->root);

	free(ws);
}

Workspace *
workspace_get_by_id(Monitor *m, int id)
{
	Workspace *ws;

	if (!m)
		return NULL;

	wl_list_for_each(ws, &m->workspaces, link) {
		if (ws->id == id)
			return ws;
	}
	return NULL;
}

/* Switch active workspace on a monitor and restore keyboard focus */
void
workspace_switch(Workspace *ws)
{
	Monitor *m;

	if (!ws || !ws->mon || ws->mon->active_workspace == ws)
		return;

	m = ws->mon;
	m->active_workspace = ws;
	if (ws->layout)
		m->lt[m->sellt] = ws->layout;

	arrange(m);

	if (ws->focused_node && ws->focused_node->client)
		focusclient(ws->focused_node->client, 1);
	else
		focusclient(focustop(m), 1);
	printstatus();
}

/* Move client between workspaces, updating N-ary tree nodes and surface visibility */
void
client_move_to_workspace(Client *c, Workspace *ws)
{
	Monitor *old_mon;
	int visible;

	if (!c || !ws || c->ws == ws)
		return;

	old_mon = c->mon;

	/* Remove client from current workspace tree */
	if (c->node)
		node_remove(c->node);

	if (c->mon != ws->mon) {
		c->mon = ws->mon;
		c->prev = c->geom;
		resize(c, c->geom, 0);
	}

	c->ws = ws;
	if (ws->id != SCRATCHPAD_WORKSPACE)
		c->prev_workspace = 0;

	if (!c->isfloating)
		node_insert_client(ws, c);

	/* Visibility check */
	visible = (ws == ws->mon->active_workspace) ||
	          (ws->id == SCRATCHPAD_WORKSPACE && ws->mon->scratchpad_showing);
	wlr_scene_node_set_enabled(&c->scene->node, visible);
	client_set_suspended(c, !visible);

	if (old_mon != ws->mon)
		arrange(old_mon);
	arrange(ws->mon);

	focusclient(focustop(selmon), 1);
	printstatus();
}

void
view_workspace(const Arg *arg)
{
	Workspace *ws;

	if (!selmon || !arg)
		return;

	ws = workspace_get_by_id(selmon, arg->i);
	if (ws)
		workspace_switch(ws);
}

void
move_to_workspace(const Arg *arg)
{
	Client *c;
	Workspace *ws;

	if (!selmon || !arg)
		return;

	c = focustop(selmon);
	if (!c)
		return;

	ws = workspace_get_by_id(selmon, arg->i);
	if (ws)
		client_move_to_workspace(c, ws);
}

int
scratchpad_client_count(Monitor *m)
{
	Client *c;
	int n = 0;
	if (!m)
		return 0;
	wl_list_for_each(c, &clients, link) {
		if (c->mon == m && c->ws && c->ws->id == SCRATCHPAD_WORKSPACE)
			n++;
	}
	return n;
}

void
togglescratchpad_client(const Arg *arg)
{
	Client *target_c = NULL;
	Workspace *scratch_ws, *target_ws = NULL;
	int target_id;
	(void)arg;

	if (!selmon)
		return;

	scratch_ws = workspace_get_by_id(selmon, SCRATCHPAD_WORKSPACE);
	if (!scratch_ws)
		return;

	target_c = focustop(selmon);
	if (!target_c)
		return;

	if (target_c->ws != scratch_ws) {
		target_c->wasfloating = target_c->isfloating;
		target_c->prev_workspace = target_c->ws ? target_c->ws->id : 1;
		client_move_to_workspace(target_c, scratch_ws);
		if (!target_c->isfloating)
			setfloating(target_c, 1);

		selmon->scratchpad_showing = 1;
		arrange(selmon);
		focusclient(target_c, 1);
		motionnotify(0, NULL, 0, 0, 0, 0);
		return;
	}

	target_id = target_c->prev_workspace;
	target_c->prev_workspace = 0;

	if (target_id > 0 && target_id != SCRATCHPAD_WORKSPACE)
		target_ws = workspace_get_by_id(selmon, target_id);

	if (!target_ws || target_ws->id == SCRATCHPAD_WORKSPACE)
		target_ws = selmon->active_workspace;

	if (!target_ws)
		return;

	/* Move client to target workspace */
	client_move_to_workspace(target_c, target_ws);

	/* Restore floating/tiling state on target workspace */
	if (!target_c->wasfloating && target_c->isfloating)
		setfloating(target_c, 0);
	target_c->wasfloating = 0;

	/* Check if any scratchpad clients remain on selmon */
	if (scratchpad_client_count(selmon) == 0)
		selmon->scratchpad_showing = 0;

	arrange(selmon);
	focusclient(target_c, 1);
	motionnotify(0, NULL, 0, 0, 0, 0);
}

void
togglescratchpad_view(const Arg *arg)
{
	Workspace *scratch_ws;
	Client *c, *scratch_c = NULL;
	Monitor *m;

	if (!selmon)
		return;

	scratch_ws = workspace_get_by_id(selmon, SCRATCHPAD_WORKSPACE);
	if (!scratch_ws)
		return;

	selmon->scratchpad_showing = !selmon->scratchpad_showing;

	if (selmon->scratchpad_showing) {
		/* Close scratchpad showing on other monitors */
		wl_list_for_each(m, &mons, link) {
			if (m != selmon && m->scratchpad_showing) {
				m->scratchpad_showing = 0;
				arrange(m);
			}
		}

		/* Gather scratchpad clients from other monitors onto selmon */
		wl_list_for_each(c, &clients, link) {
			if (c->ws && c->ws->id == SCRATCHPAD_WORKSPACE && c->mon != selmon) {
				setmon(c, selmon);
				client_move_to_workspace(c, scratch_ws);
			}
		}

		arrange(selmon);

		/* Search fstack for the most recently focused scratchpad client */
		wl_list_for_each(c, &fstack, flink) {
			if (c->mon == selmon && c->ws == scratch_ws) {
				scratch_c = c;
				break;
			}
		}
		if (scratch_c)
			focusclient(scratch_c, 1);
		else if ((c = focustop(selmon)))
			focusclient(c, 1);
	} else {
		arrange(selmon);
		/* Focus top active workspace client */
		c = focustop(selmon);
		focusclient(c, 1);
	}
	motionnotify(0, NULL, 0, 0, 0, 0);
}
