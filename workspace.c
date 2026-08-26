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

	if (ws->mon && ws->mon->active_workspace == ws)
		ws->mon->active_workspace = NULL;

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

void
workspace_switch(Workspace *ws)
{
	Monitor *m;
	Client *c;

	if (!ws || !ws->mon || ws->mon->active_workspace == ws)
		return;

	m = ws->mon;

	/* Hide clients in current active workspace */
	wl_list_for_each(c, &clients, link) {
		if (c->mon == m && c->ws == m->active_workspace) {
			wlr_scene_node_set_enabled(&c->scene->node, false);
			client_set_suspended(c, 1);
		}
	}

	m->active_workspace = ws;

	/* Reveal clients in target workspace */
	wl_list_for_each(c, &clients, link) {
		if (c->mon == m && c->ws == ws) {
			wlr_scene_node_set_enabled(&c->scene->node, true);
			client_set_suspended(c, 0);
		}
	}

	arrange(m);

	if (ws->focused_node && ws->focused_node->client)
		focusclient(ws->focused_node->client, 1);
	else
		focusclient(focustop(m), 1);
	printstatus();
}

void
client_move_to_workspace(Client *c, Workspace *ws)
{
	Monitor *old_mon;

	if (!c || !ws || c->ws == ws)
		return;

	old_mon = c->mon;

	/* Remove client from current workspace tree */
	if (c->node)
		node_remove(c->node);

	if (c->mon != ws->mon)
		setmon(c, ws->mon);

	c->ws = ws;
	node_insert_client(ws, c);

	/* Visibility check */
	if (ws == ws->mon->active_workspace) {
		wlr_scene_node_set_enabled(&c->scene->node, true);
		client_set_suspended(c, 0);
	} else {
		wlr_scene_node_set_enabled(&c->scene->node, false);
		client_set_suspended(c, 1);
	}

	if (old_mon != ws->mon)
		arrange(old_mon);
	arrange(ws->mon);
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
