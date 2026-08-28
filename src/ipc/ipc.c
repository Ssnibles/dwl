/*
 * See LICENSE file for copyright and license details.
 */
#include <stdio.h>
#include <stdlib.h>

#include "ipc/ipc.h"
#include "tree/tree.h"
#include "tree/workspace.h"
#include "dwl.h"
#include "client.h"

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
	Client *c;
	int first = 1;
	const char *tmp_path = "/tmp/dwl-tree.state.tmp";
	const char *ipc_path = "/tmp/dwl-tree.state";

	if (!ws || !ws->root)
		return;

	f = fopen(tmp_path, "w");
	if (!f)
		return;

	fprintf(f, "{\"workspace\":%d,\"root\":", ws->id);
	tree_export_node_json(ws->root, f, (ws->root == ws->focused_node));

	fprintf(f, ",\"floating\":[");
	wl_list_for_each(c, &clients, link) {
		if (c && c->ws == ws && c->isfloating) {
			const char *title;
			const char *p;
			int is_focused;

			if (!first) fprintf(f, ",");
			title = client_get_title(c);
			if (!title) title = "Window";
			is_focused = (c == focustop(selmon));
			fprintf(f, "{\"title\":\"");
			for (p = title; *p; p++) {
				if (*p == '"' || *p == '\\') fputc('\\', f);
				if ((unsigned char)*p >= 0x20) fputc(*p, f);
			}
			fprintf(f, "\",\"focused\":%d,\"geom\":[%d,%d,%d,%d]}",
				is_focused, c->geom.x, c->geom.y, c->geom.width, c->geom.height);
			first = 0;
		}
	}
	fprintf(f, "]");

	fprintf(f, "}\n");

	fclose(f);
	rename(tmp_path, ipc_path);
}
