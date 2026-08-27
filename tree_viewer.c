/*
 * DWL N-ary Workspace Tree Interactive Data Structure Viewer
 * Built with Raylib
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#include "raylib.h"
#include <wlr/util/box.h>
#include <wayland-server-core.h>

/* Data Types (Synchronized with DWL tree.h) */
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
typedef struct Client Client;
typedef struct Workspace Workspace;

struct Client {
	int id;
	char title[64];
	int isfloating;
	Node *node;
	struct wlr_box geom;
};

struct Node {
	NodeType type;
	SplitType split_type;
	float ratio_h;
	float ratio_v;
	struct wlr_box geom;

	Node *parent;
	struct wl_list children;
	struct wl_list link;

	Client *client;
	Workspace *ws;

	/* Viewer layout coordinates */
	Vector2 graph_pos;
	Rectangle graph_bounds;
};

struct Workspace {
	int id;
	char name[32];
	Node *root;
	Node *focused_node;
};

/* Color Palette (Catppuccin Macchiato style) */
#define COLOR_BG          (Color){ 24, 24, 37, 255 }
#define COLOR_PANEL       (Color){ 30, 30, 46, 255 }
#define COLOR_BORDER      (Color){ 69, 71, 90, 255 }
#define COLOR_ROOT        (Color){ 203, 166, 247, 255 }
#define COLOR_CONTAINER_H (Color){ 250, 179, 135, 255 }
#define COLOR_CONTAINER_V (Color){ 249, 226, 175, 255 }
#define COLOR_TABBED      (Color){ 137, 220, 235, 255 }
#define COLOR_STACKED     (Color){ 242, 205, 205, 255 }
#define COLOR_LEAF_TILED  (Color){ 137, 180, 250, 255 }
#define COLOR_LEAF_FLOAT  (Color){ 245, 194, 231, 255 }
#define COLOR_FOCUS       (Color){ 166, 227, 161, 255 }
#define COLOR_TEXT        (Color){ 205, 214, 244, 255 }
#define COLOR_MUTED       (Color){ 147, 153, 178, 255 }
#define COLOR_CANVAS      (Color){ 17, 17, 27, 255 }

static int client_id_counter = 1;

/* Node allocation */
static Node *
node_create(NodeType type, Workspace *ws)
{
	Node *n = (Node *)calloc(1, sizeof(Node));
	if (!n) return NULL;
	n->type = type;
	n->split_type = SPLIT_HORIZONTAL;
	n->ratio_h = 0.5f;
	n->ratio_v = 0.5f;
	n->ws = ws;
	wl_list_init(&n->children);
	wl_list_init(&n->link);
	return n;
}

static void
node_insert_child(Node *parent, Node *child)
{
	if (!parent || !child) return;
	child->parent = parent;
	child->ws = parent->ws;
	wl_list_remove(&child->link);
	wl_list_insert(parent->children.prev, &child->link);
}

static void
node_free_tree(Node *node)
{
	Node *child, *tmp;
	if (!node) return;
	wl_list_for_each_safe(child, tmp, &node->children, link) {
		node_free_tree(child);
	}
	if (node->client) {
		free(node->client);
		node->client = NULL;
	}
	free(node);
}

static Client *
client_create(const char *title, int isfloating)
{
	Client *c = (Client *)calloc(1, sizeof(Client));
	if (!c) return NULL;
	c->id = client_id_counter++;
	c->isfloating = isfloating;
	snprintf(c->title, sizeof(c->title), "%s", title);
	return c;
}

/* Live IPC JSON Parser */
static Node *
parse_node_json(const char **p, Workspace *ws)
{
	if (!p || !*p || **p != '{') return NULL;

	Node *n = node_create(NODE_ROOT, ws);
	(*p)++;

	while (**p && **p != '}') {
		if (strncmp(*p, "\"type\":", 7) == 0) {
			*p += 7;
			n->type = (NodeType)atoi(*p);
			while (**p && **p != ',' && **p != '}') (*p)++;
		} else if (strncmp(*p, "\"split\":", 8) == 0) {
			*p += 8;
			n->split_type = (SplitType)atoi(*p);
			while (**p && **p != ',' && **p != '}') (*p)++;
		} else if (strncmp(*p, "\"rh\":", 5) == 0) {
			*p += 5;
			n->ratio_h = (float)atof(*p);
			while (**p && **p != ',' && **p != '}') (*p)++;
		} else if (strncmp(*p, "\"rv\":", 5) == 0) {
			*p += 5;
			n->ratio_v = (float)atof(*p);
			while (**p && **p != ',' && **p != '}') (*p)++;
		} else if (strncmp(*p, "\"focused\":", 10) == 0) {
			*p += 10;
			if (atoi(*p)) ws->focused_node = n;
			while (**p && **p != ',' && **p != '}') (*p)++;
		} else if (strncmp(*p, "\"geom\":[", 8) == 0) {
			*p += 8;
			int x = atoi(*p); while (**p && **p != ',') (*p)++; if (**p == ',') (*p)++;
			int y = atoi(*p); while (**p && **p != ',') (*p)++; if (**p == ',') (*p)++;
			int w = atoi(*p); while (**p && **p != ',') (*p)++; if (**p == ',') (*p)++;
			int h = atoi(*p); while (**p && **p != ']') (*p)++; if (**p == ']') (*p)++;
			n->geom = (struct wlr_box){ x, y, w, h };
			while (**p && **p != ',' && **p != '}') (*p)++;
		} else if (strncmp(*p, "\"client\":{", 10) == 0) {
			*p += 10;
			char title[64] = "Window";
			int isfloating = 0;
			while (**p && **p != '}') {
				if (strncmp(*p, "\"title\":\"", 9) == 0) {
					*p += 9;
					int idx = 0;
					while (**p && **p != '"' && idx < 63) {
						title[idx++] = **p;
						(*p)++;
					}
					title[idx] = '\0';
					if (**p == '"') (*p)++;
				} else if (strncmp(*p, "\"floating\":", 11) == 0) {
					*p += 11;
					isfloating = atoi(*p);
					while (**p && **p != ',' && **p != '}') (*p)++;
				} else {
					(*p)++;
				}
				if (**p == ',') (*p)++;
			}
			n->client = client_create(title, isfloating);
			if (**p == '}') (*p)++;
			while (**p && **p != ',' && **p != '}') (*p)++;
		} else if (strncmp(*p, "\"children\":[", 12) == 0) {
			*p += 12;
			while (**p && **p != ']') {
				if (**p == '{') {
					Node *child = parse_node_json(p, ws);
					if (child) node_insert_child(n, child);
				} else {
					(*p)++;
				}
			}
			if (**p == ']') (*p)++;
			while (**p && **p != ',' && **p != '}') (*p)++;
		} else {
			(*p)++;
		}
		if (**p == ',') (*p)++;
	}
	if (**p == '}') (*p)++;
	return n;
}

static int
load_live_ipc_state(Workspace *ws)
{
	static long last_mtime = 0;
	struct stat st;
	const char *ipc_path = "/tmp/dwl-tree.state";

	if (stat(ipc_path, &st) != 0)
		return 0;

	if (st.st_mtime == last_mtime)
		return 1;

	FILE *f = fopen(ipc_path, "r");
	if (!f) return 0;

	char buffer[32768];
	size_t n = fread(buffer, 1, sizeof(buffer) - 1, f);
	fclose(f);
	buffer[n] = '\0';

	const char *p = strstr(buffer, "\"root\":");
	if (!p) return 0;
	p += 7;

	if (ws->root) {
		node_free_tree(ws->root);
	}
	client_id_counter = 1;
	ws->focused_node = NULL;

	ws->root = parse_node_json(&p, ws);
	last_mtime = st.st_mtime;
	return 1;
}

/* Preset Tree Generator for standalone testing */
static void
load_preset(Workspace *ws, int preset)
{
	if (ws->root) {
		node_free_tree(ws->root);
	}
	client_id_counter = 1;
	ws->root = node_create(NODE_ROOT, ws);

	if (preset == 1) {
		Node *container = node_create(NODE_CONTAINER, ws);
		container->split_type = SPLIT_HORIZONTAL;
		container->ratio_h = 0.5f;
		node_insert_child(ws->root, container);

		Node *l1 = node_create(NODE_LEAF, ws);
		l1->client = client_create("Terminal", 0);
		l1->ratio_h = 0.5f;
		node_insert_child(container, l1);

		Node *l2 = node_create(NODE_LEAF, ws);
		l2->client = client_create("Firefox (Floating)", 1);
		l2->ratio_h = 0.5f;
		node_insert_child(container, l2);

		ws->focused_node = l1;
	} else if (preset == 2) {
		Node *root_cont = node_create(NODE_CONTAINER, ws);
		root_cont->split_type = SPLIT_HORIZONTAL;
		node_insert_child(ws->root, root_cont);

		Node *left_v = node_create(NODE_CONTAINER, ws);
		left_v->split_type = SPLIT_VERTICAL;
		node_insert_child(root_cont, left_v);

		Node *l1 = node_create(NODE_LEAF, ws);
		l1->client = client_create("Browser", 0);
		node_insert_child(left_v, l1);

		Node *l2 = node_create(NODE_LEAF, ws);
		l2->client = client_create("Terminal", 0);
		node_insert_child(left_v, l2);

		Node *right_v = node_create(NODE_CONTAINER, ws);
		right_v->split_type = SPLIT_VERTICAL;
		node_insert_child(root_cont, right_v);

		Node *l3 = node_create(NODE_LEAF, ws);
		l3->client = client_create("Code Editor", 0);
		node_insert_child(right_v, l3);

		Node *l4 = node_create(NODE_LEAF, ws);
		l4->client = client_create("Calculator", 1);
		node_insert_child(right_v, l4);

		ws->focused_node = l3;
	}
}

/* Calculate Tree Hierarchy Layout */
static void
calculate_graph_positions(Node *node, float x, float y, float width, float y_step)
{
	if (!node) return;

	node->graph_pos = (Vector2){ x, y };
	node->graph_bounds = (Rectangle){ x - 70, y - 24, 140, 48 };

	int child_count = 0;
	Node *child;
	wl_list_for_each(child, &node->children, link) child_count++;

	if (child_count > 0) {
		float step = width / child_count;
		float start_x = x - (width / 2.0f) + (step / 2.0f);
		int i = 0;

		wl_list_for_each(child, &node->children, link) {
			float child_x = start_x + (i * step);
			calculate_graph_positions(child, child_x, y + y_step, step, y_step);
			i++;
		}
	}
}

/* Recursive Tree Renderer */
static void
draw_tree_graph(Node *node, Node **hovered_out, Vector2 mouse_world_pos, Node *focused)
{
	if (!node) return;

	Node *child;
	wl_list_for_each(child, &node->children, link) {
		DrawLineEx(node->graph_pos, child->graph_pos, 2.5f, COLOR_BORDER);
		draw_tree_graph(child, hovered_out, mouse_world_pos, focused);
	}

	/* Hit test */
	if (CheckCollisionPointRec(mouse_world_pos, node->graph_bounds)) {
		*hovered_out = node;
	}

	int is_focused = (node == focused);
	Color fill_color = COLOR_PANEL;
	Color border_color = COLOR_BORDER;
	Color text_color = COLOR_TEXT;
	char badge[48] = "";
	char sub_badge[32] = "";

	if (node->type == NODE_ROOT) {
		border_color = COLOR_ROOT;
		snprintf(badge, sizeof(badge), "NODE_ROOT");
		snprintf(sub_badge, sizeof(sub_badge), "Workspace %d", node->ws ? node->ws->id : 1);
	} else if (node->type == NODE_CONTAINER) {
		float r = (node->split_type == SPLIT_HORIZONTAL) ? node->ratio_h : node->ratio_v;
		switch (node->split_type) {
		case SPLIT_HORIZONTAL:
			border_color = COLOR_CONTAINER_H;
			snprintf(badge, sizeof(badge), "H-CONTAINER");
			snprintf(sub_badge, sizeof(sub_badge), "Ratio: %.2f", r);
			break;
		case SPLIT_VERTICAL:
			border_color = COLOR_CONTAINER_V;
			snprintf(badge, sizeof(badge), "V-CONTAINER");
			snprintf(sub_badge, sizeof(sub_badge), "Ratio: %.2f", r);
			break;
		case SPLIT_TABBED:
			border_color = COLOR_TABBED;
			snprintf(badge, sizeof(badge), "TABBED-CONTAINER");
			break;
		case SPLIT_STACKED:
			border_color = COLOR_STACKED;
			snprintf(badge, sizeof(badge), "STACKED-CONTAINER");
			break;
		default:
			border_color = COLOR_CONTAINER_H;
			snprintf(badge, sizeof(badge), "CONTAINER");
			break;
		}
	} else if (node->type == NODE_LEAF) {
		if (node->client) {
			if (node->client->isfloating) {
				border_color = COLOR_LEAF_FLOAT;
				snprintf(badge, sizeof(badge), "[FLOAT] #%d", node->client->id);
			} else {
				border_color = COLOR_LEAF_TILED;
				snprintf(badge, sizeof(badge), "[TILED] #%d", node->client->id);
			}
			snprintf(sub_badge, sizeof(sub_badge), "%s", node->client->title);
		} else {
			border_color = COLOR_LEAF_TILED;
			snprintf(badge, sizeof(badge), "LEAF NODE");
		}
	}

	if (is_focused) {
		fill_color = (Color){ 45, 55, 65, 255 };
		border_color = COLOR_FOCUS;
	}

	/* Draw Node Box */
	DrawRectangleRounded(node->graph_bounds, 0.25f, 4, fill_color);
	DrawRectangleRoundedLines(node->graph_bounds, 0.25f, 4, border_color);

	/* Label Text */
	int font_size = 11;
	int text_w = MeasureText(badge, font_size);
	DrawText(badge, (int)(node->graph_pos.x - text_w / 2), (int)(node->graph_pos.y - 12), font_size, is_focused ? COLOR_FOCUS : text_color);

	if (sub_badge[0]) {
		int sub_w = MeasureText(sub_badge, 10);
		DrawText(sub_badge, (int)(node->graph_pos.x - sub_w / 2), (int)(node->graph_pos.y + 4), 10, COLOR_MUTED);
	}
}

int main(void)
{
	const int screenWidth = 1360;
	const int screenHeight = 768;

	SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
	InitWindow(screenWidth, screenHeight, "DWL N-ary Workspace Tree Data Structure Viewer");
	SetTargetFPS(60);

	Workspace ws = { .id = 1, .name = "1" };
	load_preset(&ws, 1);

	int current_preset = 1;

	Camera2D camera = { 0 };
	camera.target = (Vector2){ 680, 200 };
	camera.offset = (Vector2){ 680, 200 };
	camera.zoom = 1.0f;

	Vector2 pan_start = { 0 };
	bool dragging = false;

	while (!WindowShouldClose()) {
		int is_live = load_live_ipc_state(&ws);

		/* Preset Selection (if not live) */
		if (!is_live) {
			if (IsKeyPressed(KEY_ONE)) { current_preset = 1; load_preset(&ws, 1); }
			if (IsKeyPressed(KEY_TWO)) { current_preset = 2; load_preset(&ws, 2); }
		}

		/* Ratio adjustments */
		if (ws.focused_node) {
			Node *f = ws.focused_node;
			float delta = 0.02f;
			if (IsKeyPressed(KEY_LEFT))  { f->ratio_h = fmaxf(0.1f, f->ratio_h - delta); }
			if (IsKeyPressed(KEY_RIGHT)) { f->ratio_h = fminf(0.9f, f->ratio_h + delta); }
			if (IsKeyPressed(KEY_UP))    { f->ratio_v = fmaxf(0.1f, f->ratio_v - delta); }
			if (IsKeyPressed(KEY_DOWN))  { f->ratio_v = fminf(0.9f, f->ratio_v + delta); }
		}

		/* Mouse Pan & Zoom */
		float wheel = GetMouseWheelMove();
		if (wheel != 0) {
			camera.zoom += wheel * 0.1f;
			if (camera.zoom < 0.4f) camera.zoom = 0.4f;
			if (camera.zoom > 2.5f) camera.zoom = 2.5f;
		}

		if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
			pan_start = GetMousePosition();
			dragging = true;
		}
		if (dragging) {
			Vector2 current_mouse = GetMousePosition();
			Vector2 delta = { current_mouse.x - pan_start.x, current_mouse.y - pan_start.y };
			camera.target.x -= delta.x / camera.zoom;
			camera.target.y -= delta.y / camera.zoom;
			pan_start = current_mouse;
			if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) || IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE)) {
				dragging = false;
			}
		}

		/* Recalculate tree positions across expanded full canvas (width: 1200) */
		calculate_graph_positions(ws.root, 680, 120, 1100, 100);

		Vector2 mouse_pos = GetMousePosition();
		Vector2 mouse_world = GetScreenToWorld2D(mouse_pos, camera);
		Node *hovered_node = NULL;

		BeginDrawing();
			ClearBackground(COLOR_BG);

			/* Header Bar */
			DrawRectangle(0, 0, screenWidth, 48, COLOR_PANEL);
			DrawLine(0, 48, screenWidth, 48, COLOR_BORDER);
			DrawText("DWL N-ary Tree Data Structure Viewer", 20, 14, 18, COLOR_TEXT);

			/* Mode Indicator */
			if (is_live) {
				DrawRectangle(430, 12, 140, 24, (Color){ 40, 90, 50, 255 });
				DrawRectangleLines(430, 12, 140, 24, GREEN);
				DrawText("LIVE SESSION", 448, 17, 12, GREEN);
			} else {
				char preset_info[128];
				snprintf(preset_info, sizeof(preset_info), "Simulator Mode: Preset [%d] (1-2)", current_preset);
				DrawText(preset_info, 430, 16, 14, COLOR_FOCUS);
			}

			DrawText("Mouse Drag/Wheel: Pan/Zoom  |  Click Node: Focus", 880, 16, 13, COLOR_MUTED);

			/* Full-Width Main Tree Graph Canvas */
			Rectangle main_canvas = { 15, 63, 1330, 510 };
			DrawRectangleRounded(main_canvas, 0.02f, 4, COLOR_PANEL);
			DrawRectangleRoundedLines(main_canvas, 0.02f, 4, COLOR_BORDER);

			/* Begin Camera 2D for Tree Graph */
			BeginMode2D(camera);
				draw_tree_graph(ws.root, &hovered_node, mouse_world, ws.focused_node);
			EndMode2D();

			/* Bottom HUD / Node Inspector Panel */
			Rectangle hud_panel = { 15, 585, 1330, 168 };
			DrawRectangleRounded(hud_panel, 0.02f, 4, (Color){ 22, 22, 34, 255 });
			DrawRectangleRoundedLines(hud_panel, 0.02f, 4, COLOR_BORDER);

			DrawText("Selected Node Inspection & Details", 32, 598, 14, COLOR_FOCUS);

			if (ws.focused_node) {
				Node *f = ws.focused_node;
				char line1[128], line2[128], line3[128], line4[128];

				snprintf(line1, sizeof(line1), "Node Type: %s", 
					(f->type == NODE_ROOT) ? "NODE_ROOT (Workspace Root)" :
					(f->type == NODE_CONTAINER) ? "NODE_CONTAINER (Branch)" : "NODE_LEAF (Window Leaf)");

				snprintf(line2, sizeof(line2), "Split Mode: %s",
					(f->split_type == SPLIT_HORIZONTAL) ? "SPLIT_HORIZONTAL" :
					(f->split_type == SPLIT_VERTICAL) ? "SPLIT_VERTICAL" :
					(f->split_type == SPLIT_TABBED) ? "SPLIT_TABBED" :
					(f->split_type == SPLIT_STACKED) ? "SPLIT_STACKED" : "SPLIT_NONE");

				snprintf(line3, sizeof(line3), "Ratio Weights -> Horizontal: %.3f | Vertical: %.3f", f->ratio_h, f->ratio_v);

				snprintf(line4, sizeof(line4), "Calculated Screen Box -> X: %d, Y: %d, Width: %d, Height: %d", 
					f->geom.x, f->geom.y, f->geom.width, f->geom.height);

				DrawText(line1, 32, 625, 13, COLOR_TEXT);
				DrawText(line2, 32, 650, 13, COLOR_TEXT);
				DrawText(line3, 32, 675, 13, COLOR_FOCUS);
				DrawText(line4, 32, 700, 13, COLOR_MUTED);

				if (f->client) {
					char client_info[128];
					snprintf(client_info, sizeof(client_info), "Client Window: \"%s\" | Floating: %s", 
						f->client->title, f->client->isfloating ? "YES [FLOAT]" : "NO [TILED]");
					DrawText(client_info, 680, 625, 14, f->client->isfloating ? COLOR_LEAF_FLOAT : COLOR_LEAF_TILED);
				}
			}

			/* Mouse Click Selection */
			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovered_node) {
				ws.focused_node = hovered_node;
			}

		EndDrawing();
	}

	node_free_tree(ws.root);
	CloseWindow();
	return 0;
}
