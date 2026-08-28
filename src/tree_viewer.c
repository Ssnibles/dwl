/*
 * DWL N-ary Workspace Tree Interactive Data Structure Viewer
 * Built with Raylib - Modern Responsive UI, System Vector Fonts & Persistent
 * Node Inspection
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "raylib.h"
#include <wayland-server-core.h>
#include <wlr/util/box.h>

/* Data Types (Synchronized with DWL tree.h) */
typedef enum { NODE_ROOT, NODE_CONTAINER, NODE_LEAF } NodeType;

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

typedef struct {
  char title[64];
  int focused;
  struct wlr_box geom;
} FloatingClientInfo;

/* Color Palette (Catppuccin Macchiato style) */
#define COLOR_BG (Color){24, 24, 37, 255}
#define COLOR_PANEL (Color){30, 30, 46, 255}
#define COLOR_BORDER (Color){69, 71, 90, 255}
#define COLOR_ROOT (Color){203, 166, 247, 255}
#define COLOR_CONTAINER_H (Color){250, 179, 135, 255}
#define COLOR_CONTAINER_V (Color){249, 226, 175, 255}
#define COLOR_TABBED (Color){137, 220, 235, 255}
#define COLOR_STACKED (Color){242, 205, 205, 255}
#define COLOR_LEAF_TILED (Color){137, 180, 250, 255}
#define COLOR_LEAF_FLOAT (Color){245, 194, 231, 255}
#define COLOR_FOCUS (Color){166, 227, 161, 255}
#define COLOR_TEXT (Color){205, 214, 244, 255}
#define COLOR_MUTED (Color){147, 153, 178, 255}
#define COLOR_CANVAS (Color){17, 17, 27, 255}

static int client_id_counter = 1;
static Font global_font = {0};

/* Manual Selection & Inspection Lock State */
static char selected_node_key[128] = "";
static int manual_select_mode = 0;

/* Floating layer state tracking */
static FloatingClientInfo floating_clients[16];
static int floating_client_count = 0;

/* Typography system: Detect system vector font for sharp antialiased rendering
 */
static void init_system_font(void) {
  char font_path[512] = {0};
  FILE *fp = popen("fc-match -f \"%{file}\" sans-serif 2>/dev/null", "r");
  if (fp) {
    if (fgets(font_path, sizeof(font_path), fp) != NULL) {
      size_t len = strlen(font_path);
      if (len > 0 && font_path[len - 1] == '\n')
        font_path[len - 1] = '\0';
    }
    pclose(fp);
  }

  if (font_path[0] != '\0' && FileExists(font_path)) {
    global_font = LoadFontEx(font_path, 48, NULL, 250);
    if (global_font.texture.id != 0) {
      SetTextureFilter(global_font.texture, TEXTURE_FILTER_BILINEAR);
      return;
    }
  }

  /* Fallback to Raylib default font with bilinear filter */
  global_font = GetFontDefault();
  SetTextureFilter(global_font.texture, TEXTURE_FILTER_BILINEAR);
}

static void draw_text(const char *text, float x, float y, float size,
                      Color color) {
  DrawTextEx(global_font, text, (Vector2){x, y}, size, 1.0f, color);
}

static Vector2 measure_text(const char *text, float size) {
  return MeasureTextEx(global_font, text, size, 1.0f);
}

/* Persistent Node Keying System */
static void get_node_key(Node *node, char *buf, size_t buf_size) {
  if (!node) {
    buf[0] = '\0';
    return;
  }
  if (node->client) {
    snprintf(buf, buf_size, "client:%d", node->client->id);
    return;
  }
  if (node->type == NODE_ROOT) {
    snprintf(buf, buf_size, "root:%d", node->ws ? node->ws->id : 1);
    return;
  }
  char parent_key[128] = "";
  get_node_key(node->parent, parent_key, sizeof(parent_key));

  int index = 0;
  if (node->parent) {
    Node *child;
    wl_list_for_each(child, &node->parent->children, link) {
      if (child == node)
        break;
      index++;
    }
  }
  snprintf(buf, buf_size, "%s/c%d_s%d", parent_key, index, node->split_type);
}

static Node *find_node_by_key(Node *node, const char *key) {
  if (!node || !key || key[0] == '\0')
    return NULL;

  char current_key[128];
  get_node_key(node, current_key, sizeof(current_key));
  if (strcmp(current_key, key) == 0)
    return node;

  Node *child, *found;
  wl_list_for_each(child, &node->children, link) {
    found = find_node_by_key(child, key);
    if (found)
      return found;
  }
  return NULL;
}

/* Precise Node Hit Testing */
static Node *find_hovered_node(Node *node, Vector2 mouse_world_pos) {
  if (!node)
    return NULL;

  Node *child, *hit;
  wl_list_for_each(child, &node->children, link) {
    hit = find_hovered_node(child, mouse_world_pos);
    if (hit)
      return hit;
  }

  if (CheckCollisionPointRec(mouse_world_pos, node->graph_bounds)) {
    return node;
  }

  return NULL;
}

/* Node allocation */
static Node *node_create(NodeType type, Workspace *ws) {
  Node *n = (Node *)calloc(1, sizeof(Node));
  if (!n)
    return NULL;
  n->type = type;
  n->split_type = SPLIT_HORIZONTAL;
  n->ratio_h = 0.5f;
  n->ratio_v = 0.5f;
  n->ws = ws;
  wl_list_init(&n->children);
  wl_list_init(&n->link);
  return n;
}

static void node_insert_child(Node *parent, Node *child) {
  if (!parent || !child)
    return;
  child->parent = parent;
  child->ws = parent->ws;
  wl_list_remove(&child->link);
  wl_list_insert(parent->children.prev, &child->link);
}

static void node_free_tree(Node *node) {
  Node *child, *tmp;
  if (!node)
    return;
  wl_list_for_each_safe(child, tmp, &node->children, link) {
    node_free_tree(child);
  }
  if (node->client) {
    free(node->client);
    node->client = NULL;
  }
  free(node);
}

static Client *client_create(const char *title, int isfloating) {
  Client *c = (Client *)calloc(1, sizeof(Client));
  if (!c)
    return NULL;
  c->id = client_id_counter++;
  c->isfloating = isfloating;
  snprintf(c->title, sizeof(c->title), "%s", title);
  return c;
}

/* Live IPC JSON Parser */
static Node *parse_node_json(const char **p, Workspace *ws) {
  if (!p || !*p || **p != '{')
    return NULL;

  Node *n = node_create(NODE_ROOT, ws);
  (*p)++;

  while (**p && **p != '}') {
    if (strncmp(*p, "\"type\":", 7) == 0) {
      *p += 7;
      n->type = (NodeType)atoi(*p);
      while (**p && **p != ',' && **p != '}')
        (*p)++;
    } else if (strncmp(*p, "\"split\":", 8) == 0) {
      *p += 8;
      n->split_type = (SplitType)atoi(*p);
      while (**p && **p != ',' && **p != '}')
        (*p)++;
    } else if (strncmp(*p, "\"rh\":", 5) == 0) {
      *p += 5;
      n->ratio_h = (float)atof(*p);
      while (**p && **p != ',' && **p != '}')
        (*p)++;
    } else if (strncmp(*p, "\"rv\":", 5) == 0) {
      *p += 5;
      n->ratio_v = (float)atof(*p);
      while (**p && **p != ',' && **p != '}')
        (*p)++;
    } else if (strncmp(*p, "\"focused\":", 10) == 0) {
      *p += 10;
      if (atoi(*p))
        ws->focused_node = n;
      while (**p && **p != ',' && **p != '}')
        (*p)++;
    } else if (strncmp(*p, "\"geom\":[", 8) == 0) {
      *p += 8;
      int x = atoi(*p);
      while (**p && **p != ',')
        (*p)++;
      if (**p == ',')
        (*p)++;
      int y = atoi(*p);
      while (**p && **p != ',')
        (*p)++;
      if (**p == ',')
        (*p)++;
      int w = atoi(*p);
      while (**p && **p != ',')
        (*p)++;
      if (**p == ',')
        (*p)++;
      int h = atoi(*p);
      while (**p && **p != ']')
        (*p)++;
      if (**p == ']')
        (*p)++;
      n->geom = (struct wlr_box){x, y, w, h};
      while (**p && **p != ',' && **p != '}')
        (*p)++;
    } else if (strncmp(*p, "\"client\":{", 10) == 0) {
      *p += 10;
      char title[64] = "Window";
      int isfloating = 0;
      while (**p && **p != '}') {
        if (strncmp(*p, "\"title\":\"", 9) == 0) {
          *p += 9;
          int idx = 0;
          while (**p && **p != '"' && idx < 63) {
            if (**p == '\\' && *(*p + 1))
              (*p)++;
            title[idx++] = **p;
            (*p)++;
          }
          title[idx] = '\0';
          if (**p == '"')
            (*p)++;
        } else if (strncmp(*p, "\"floating\":", 11) == 0) {
          *p += 11;
          isfloating = atoi(*p);
          while (**p && **p != ',' && **p != '}')
            (*p)++;
        } else {
          (*p)++;
        }
        if (**p == ',')
          (*p)++;
      }
      n->client = client_create(title, isfloating);
      if (**p == '}')
        (*p)++;
      while (**p && **p != ',' && **p != '}')
        (*p)++;
    } else if (strncmp(*p, "\"children\":[", 12) == 0) {
      *p += 12;
      while (**p && **p != ']') {
        if (**p == '{') {
          Node *child = parse_node_json(p, ws);
          if (child)
            node_insert_child(n, child);
        } else {
          (*p)++;
        }
      }
      if (**p == ']')
        (*p)++;
      while (**p && **p != ',' && **p != '}')
        (*p)++;
    } else {
      (*p)++;
    }
    if (**p == ',')
      (*p)++;
  }
  if (**p == '}')
    (*p)++;
  return n;
}

static int load_live_ipc_state(Workspace *ws) {
  static long last_mtime = 0;
  struct stat st;
  const char *ipc_path = "/tmp/dwl-tree.state";

  if (stat(ipc_path, &st) != 0)
    return 0;

  if (st.st_mtime == last_mtime) {
    /* Re-sync manual node selection if locked */
    if (manual_select_mode && selected_node_key[0] != '\0') {
      Node *pinned = find_node_by_key(ws->root, selected_node_key);
      if (pinned)
        ws->focused_node = pinned;
    }
    return 1;
  }

  FILE *f = fopen(ipc_path, "r");
  if (!f)
    return 0;

  char buffer[32768];
  size_t n = fread(buffer, 1, sizeof(buffer) - 1, f);
  fclose(f);
  buffer[n] = '\0';

  const char *p = strstr(buffer, "\"root\":");
  if (!p)
    return 0;
  p += 7;

  if (ws->root) {
    node_free_tree(ws->root);
  }
  client_id_counter = 1;
  ws->focused_node = NULL;

  ws->root = parse_node_json(&p, ws);
  last_mtime = st.st_mtime;

  /* Parse floating clients array */
  floating_client_count = 0;
  const char *fp = strstr(buffer, "\"floating\":[");
  if (fp) {
    fp += 12;
    while (*fp && *fp != ']' && floating_client_count < 16) {
      if (strncmp(fp, "{\"title\":\"", 10) == 0) {
        fp += 10;
        char title[64] = "";
        int idx = 0;
        while (*fp && *fp != '"' && idx < 63) {
          if (*fp == '\\' && *(fp + 1))
            fp++;
          title[idx++] = *fp++;
        }
        title[idx] = '\0';
        if (*fp == '"')
          fp++;

        int is_focused = 0;
        const char *foc = strstr(fp, "\"focused\":");
        if (foc && foc < strchr(fp, '}')) {
          is_focused = atoi(foc + 10);
        }

        snprintf(floating_clients[floating_client_count].title, 64, "%s",
                 title);
        floating_clients[floating_client_count].focused = is_focused;
        floating_client_count++;
      } else {
        fp++;
      }
    }
  }

  /* Restore manual pinned selection across IPC tree reloads */
  if (manual_select_mode && selected_node_key[0] != '\0') {
    Node *pinned = find_node_by_key(ws->root, selected_node_key);
    if (pinned) {
      ws->focused_node = pinned;
    } else {
      /* Pinned node no longer exists in workspace */
      manual_select_mode = 0;
      selected_node_key[0] = '\0';
    }
  }

  return 1;
}

/* Preset Tree Generator for standalone testing */
static void load_preset(Workspace *ws, int preset) {
  if (ws->root) {
    node_free_tree(ws->root);
  }
  client_id_counter = 1;
  ws->root = node_create(NODE_ROOT, ws);
  floating_client_count = 0;

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
    l2->client = client_create("Code Editor", 0);
    l2->ratio_h = 0.5f;
    node_insert_child(container, l2);

    snprintf(floating_clients[0].title, 64, "Firefox (Floating Layer)");
    floating_clients[0].focused = 0;
    floating_client_count = 1;

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

    snprintf(floating_clients[0].title, 64, "Calculator (Floating Layer)");
    floating_clients[0].focused = 0;
    floating_client_count = 1;

    ws->focused_node = l3;
  }

  if (manual_select_mode && selected_node_key[0] != '\0') {
    Node *pinned = find_node_by_key(ws->root, selected_node_key);
    if (pinned) {
      ws->focused_node = pinned;
    } else {
      manual_select_mode = 0;
      selected_node_key[0] = '\0';
    }
  }
}

/* Calculate Tree Hierarchy Layout dynamically */
static void calculate_graph_positions(Node *node, float x, float y, float width,
                                      float y_step) {
  if (!node)
    return;

  node->graph_pos = (Vector2){x, y};
  node->graph_bounds = (Rectangle){x - 75.0f, y - 25.0f, 150.0f, 50.0f};

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
static void draw_tree_graph(Node *node, Node *hovered, Vector2 mouse_world_pos,
                            Node *focused) {
  if (!node)
    return;

  Node *child;
  wl_list_for_each(child, &node->children, link) {
    DrawLineEx(node->graph_pos, child->graph_pos, 2.5f, COLOR_BORDER);
    draw_tree_graph(child, hovered, mouse_world_pos, focused);
  }

  int is_focused = (node == focused);
  int is_hovered = (node == hovered);
  Color fill_color = COLOR_PANEL;
  Color border_color = COLOR_BORDER;
  Color text_color = COLOR_TEXT;
  char badge[48] = "";
  char sub_badge[64] = "";

  if (node->type == NODE_ROOT) {
    border_color = COLOR_ROOT;
    snprintf(badge, sizeof(badge), "NODE_ROOT");
    snprintf(sub_badge, sizeof(sub_badge), "Workspace %d",
             node->ws ? node->ws->id : 1);
  } else if (node->type == NODE_CONTAINER) {
    float r =
        (node->split_type == SPLIT_HORIZONTAL) ? node->ratio_h : node->ratio_v;
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
    fill_color = (Color){45, 55, 65, 255};
    border_color = COLOR_FOCUS;
  }

  /* Draw Node Box */
  DrawRectangleRounded(node->graph_bounds, 0.25f, 4, fill_color);
  DrawRectangleRoundedLines(node->graph_bounds, 0.25f, 4, border_color);

  /* Hover outline for tactile visual feedback */
  if (is_hovered) {
    DrawRectangleRoundedLines(node->graph_bounds, 0.25f, 4,
                              (Color){255, 255, 255, 220});
  }

  /* Crisp Vector Typography */
  float font_size = 13.0f;
  Vector2 text_sz = measure_text(badge, font_size);
  draw_text(badge, node->graph_pos.x - text_sz.x / 2.0f,
            node->graph_pos.y - 14.0f, font_size,
            is_focused ? COLOR_FOCUS : text_color);

  if (sub_badge[0]) {
    float sub_size = 11.0f;
    Vector2 sub_sz = measure_text(sub_badge, sub_size);
    draw_text(sub_badge, node->graph_pos.x - sub_sz.x / 2.0f,
              node->graph_pos.y + 4.0f, sub_size, COLOR_MUTED);
  }
}

int main(void) {
  const int initialWidth = 1360;
  const int initialHeight = 768;

  SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
  InitWindow(initialWidth, initialHeight,
             "DWL N-ary Workspace Tree Data Structure Viewer");
  SetWindowMinSize(850, 550);
  SetTargetFPS(60);

  init_system_font();

  Workspace ws = {.id = 1, .name = "1"};
  load_preset(&ws, 1);

  int current_preset = 1;

  Camera2D camera = {0};
  camera.target = (Vector2){(float)initialWidth / 2.0f, 180.0f};
  camera.offset = (Vector2){(float)initialWidth / 2.0f, 290.0f};
  camera.zoom = 1.0f;

  Vector2 pan_start = {0};
  bool dragging = false;

  while (!WindowShouldClose()) {
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    int is_live = load_live_ipc_state(&ws);

    /* Preset Selection (if not live) */
    if (!is_live) {
      if (IsKeyPressed(KEY_ONE)) {
        current_preset = 1;
        load_preset(&ws, 1);
      }
      if (IsKeyPressed(KEY_TWO)) {
        current_preset = 2;
        load_preset(&ws, 2);
      }
    }

    /* Ratio adjustments */
    if (ws.focused_node) {
      Node *f = ws.focused_node;
      float delta = 0.02f;
      if (IsKeyPressed(KEY_LEFT)) {
        f->ratio_h = fmaxf(0.1f, f->ratio_h - delta);
      }
      if (IsKeyPressed(KEY_RIGHT)) {
        f->ratio_h = fminf(0.9f, f->ratio_h + delta);
      }
      if (IsKeyPressed(KEY_UP)) {
        f->ratio_v = fmaxf(0.1f, f->ratio_v - delta);
      }
      if (IsKeyPressed(KEY_DOWN)) {
        f->ratio_v = fminf(0.9f, f->ratio_v + delta);
      }
    }

    /* Mouse Pan & Zoom */
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
      camera.zoom += wheel * 0.1f;
      if (camera.zoom < 0.4f)
        camera.zoom = 0.4f;
      if (camera.zoom > 2.5f)
        camera.zoom = 2.5f;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) ||
        IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
      pan_start = GetMousePosition();
      dragging = true;
    }
    if (dragging) {
      Vector2 current_mouse = GetMousePosition();
      Vector2 delta = {current_mouse.x - pan_start.x,
                       current_mouse.y - pan_start.y};
      camera.target.x -= delta.x / camera.zoom;
      camera.target.y -= delta.y / camera.zoom;
      pan_start = current_mouse;
      if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) ||
          IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE)) {
        dragging = false;
      }
    }

    /* Recalculate tree positions across responsive canvas width */
    float graph_width = fmaxf((float)screenWidth - 120.0f, 600.0f);
    calculate_graph_positions(ws.root, (float)screenWidth / 2.0f, 120.0f,
                              graph_width, 100.0f);

    /* Dynamic viewport offsets */
    float canvas_x = 15.0f;
    float canvas_y = 58.0f;
    float canvas_w = (float)screenWidth - 30.0f;
    float hud_h = 165.0f;
    float canvas_h = (float)screenHeight - canvas_y - hud_h - 15.0f;
    if (canvas_h < 150.0f)
      canvas_h = 150.0f;

    /* Keep camera offset centered in main canvas */
    camera.offset = (Vector2){(float)screenWidth / 2.0f,
                              canvas_y + (canvas_h / 2.0f) - 40.0f};

    Vector2 mouse_pos = GetMousePosition();
    Vector2 mouse_world = GetScreenToWorld2D(mouse_pos, camera);

    /* Hit test nodes BEFORE click processing */
    Node *hovered_node = find_hovered_node(ws.root, mouse_world);

    Rectangle main_canvas = {canvas_x, canvas_y, canvas_w, canvas_h};

    /* Mouse Click Selection & Canvas Pinning Interaction */
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      if (CheckCollisionPointRec(mouse_pos, main_canvas)) {
        if (hovered_node) {
          ws.focused_node = hovered_node;
          manual_select_mode = 1;
          get_node_key(hovered_node, selected_node_key,
                       sizeof(selected_node_key));
        } else {
          /* Clicked empty canvas space -> clear manual pin */
          manual_select_mode = 0;
          selected_node_key[0] = '\0';
        }
      }
    }

    /* Keyboard shortcuts to clear manual pin */
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F)) {
      manual_select_mode = 0;
      selected_node_key[0] = '\0';
    }

    BeginDrawing();
    ClearBackground(COLOR_BG);

    /* Header Bar */
    DrawRectangle(0, 0, screenWidth, 48, COLOR_PANEL);
    DrawLine(0, 48, screenWidth, 48, COLOR_BORDER);
    draw_text("DWL N-ary Tree Data Structure Viewer", 20, 14, 18, COLOR_TEXT);

    /* Mode Indicator */
    if (is_live) {
      DrawRectangle(450, 12, 140, 24, (Color){40, 90, 50, 255});
      DrawRectangleLines(450, 12, 140, 24, GREEN);
      draw_text("LIVE SESSION", 468, 17, 12, GREEN);
    } else {
      char preset_info[128];
      snprintf(preset_info, sizeof(preset_info),
               "Simulator Mode: Preset [%d] (1-2)", current_preset);
      draw_text(preset_info, 450, 16, 14, COLOR_FOCUS);
    }

    const char *controls_str = "Mouse Drag/Wheel: Pan/Zoom  |  Click Node: Pin "
                               "Inspection  |  ESC/Empty Click: Unpin";
    Vector2 ctrl_sz = measure_text(controls_str, 13);
    draw_text(controls_str, (float)screenWidth - ctrl_sz.x - 20.0f, 16, 13,
              COLOR_MUTED);

    /* Main Dynamic Canvas Box */
    DrawRectangleRounded(main_canvas, 0.015f, 4, COLOR_PANEL);
    DrawRectangleRoundedLines(main_canvas, 0.015f, 4, COLOR_BORDER);

    /* Begin Scissored Camera 2D for Tree Graph */
    BeginScissorMode((int)canvas_x, (int)canvas_y, (int)canvas_w,
                     (int)canvas_h);
    BeginMode2D(camera);
    draw_tree_graph(ws.root, hovered_node, mouse_world, ws.focused_node);
    EndMode2D();
    EndScissorMode();

    /* Bottom HUD Inspector Panel */
    float hud_y = (float)screenHeight - hud_h - 10.0f;
    Rectangle hud_panel = {canvas_x, hud_y, canvas_w, hud_h};
    DrawRectangleRounded(hud_panel, 0.015f, 4, (Color){22, 22, 34, 255});
    DrawRectangleRoundedLines(hud_panel, 0.015f, 4, COLOR_BORDER);

    draw_text("Selected Node Inspection & Details", 32, hud_y + 12.0f, 15,
              COLOR_FOCUS);

    if (manual_select_mode) {
      float badge_w = 340.0f;
      DrawRectangle((int)(canvas_x + canvas_w - badge_w - 20.0f),
                    (int)(hud_y + 10.0f), (int)badge_w, 22,
                    (Color){70, 50, 20, 255});
      DrawRectangleLines((int)(canvas_x + canvas_w - badge_w - 20.0f),
                         (int)(hud_y + 10.0f), (int)badge_w, 22, ORANGE);
      draw_text("PINNED SELECTION (Click empty canvas or ESC to unpin)",
                canvas_x + canvas_w - badge_w - 12.0f, hud_y + 14.0f, 11,
                ORANGE);
    } else {
      float badge_w = 260.0f;
      draw_text("LIVE AUTO-FOCUS (Click node to pin)",
                canvas_x + canvas_w - badge_w - 10.0f, hud_y + 14.0f, 11,
                COLOR_MUTED);
    }

    if (ws.focused_node) {
      Node *f = ws.focused_node;
      char line1[128], line2[128], line3[128], line4[128];

      snprintf(line1, sizeof(line1), "Node Type: %s",
               (f->type == NODE_ROOT)        ? "NODE_ROOT (Workspace Root)"
               : (f->type == NODE_CONTAINER) ? "NODE_CONTAINER (Branch)"
                                             : "NODE_LEAF (Window Leaf)");

      snprintf(line2, sizeof(line2), "Split Mode: %s",
               (f->split_type == SPLIT_HORIZONTAL) ? "SPLIT_HORIZONTAL"
               : (f->split_type == SPLIT_VERTICAL) ? "SPLIT_VERTICAL"
               : (f->split_type == SPLIT_TABBED)   ? "SPLIT_TABBED"
               : (f->split_type == SPLIT_STACKED)  ? "SPLIT_STACKED"
                                                   : "SPLIT_NONE");

      snprintf(line3, sizeof(line3),
               "Ratio Weights -> Horizontal: %.3f | Vertical: %.3f", f->ratio_h,
               f->ratio_v);

      snprintf(line4, sizeof(line4),
               "Calculated Screen Box -> X: %d, Y: %d, Width: %d, Height: %d",
               f->geom.x, f->geom.y, f->geom.width, f->geom.height);

      draw_text(line1, 32, hud_y + 38.0f, 13, COLOR_TEXT);
      draw_text(line2, 32, hud_y + 60.0f, 13, COLOR_TEXT);
      draw_text(line3, 32, hud_y + 82.0f, 13, COLOR_FOCUS);
      draw_text(line4, 32, hud_y + 104.0f, 13, COLOR_MUTED);

      float right_col_x = fmaxf((float)screenWidth * 0.52f, 450.0f);
      if (f->client) {
        char client_info[128];
        snprintf(client_info, sizeof(client_info),
                 "Client Window: \"%s\" | Floating: %s", f->client->title,
                 f->client->isfloating ? "YES [FLOAT]" : "NO [TILED]");
        draw_text(client_info, right_col_x, hud_y + 38.0f, 14,
                  f->client->isfloating ? COLOR_LEAF_FLOAT : COLOR_LEAF_TILED);
      }

      char zoom_info[128];
      snprintf(zoom_info, sizeof(zoom_info),
               "Camera Zoom: %.2fx (Scroll Wheel to Zoom, Right Drag to Pan)",
               camera.zoom);
      draw_text(zoom_info, right_col_x, hud_y + 60.0f, 13, COLOR_MUTED);

      /* Floating layer info summary */
      if (floating_client_count > 0) {
        char float_str[128];
        snprintf(float_str, sizeof(float_str),
                 "Floating Layer: %d Window(s) Detached from Tree (\"%s\")",
                 floating_client_count, floating_clients[0].title);
        draw_text(float_str, right_col_x, hud_y + 82.0f, 13, COLOR_LEAF_FLOAT);
      } else {
        draw_text("Floating Layer: None (All windows managed in Tiling Tree)",
                  right_col_x, hud_y + 82.0f, 13, COLOR_MUTED);
      }
    } else {
      draw_text(
          "No node selected. Click on any tree node above to inspect details.",
          32, hud_y + 40.0f, 13, COLOR_MUTED);
    }

    EndDrawing();
  }

  if (global_font.texture.id != 0 &&
      global_font.texture.id != GetFontDefault().texture.id) {
    UnloadFont(global_font);
  }

  node_free_tree(ws.root);
  CloseWindow();
  return 0;
}
