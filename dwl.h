/*
 * See LICENSE file for copyright and license details.
 */
#ifndef DWL_H
#define DWL_H

#include <getopt.h>
#include <libinput.h>
#include <linux/input-event-codes.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_ext_data_control_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <scenefx/render/fx_renderer/fx_renderer.h>
#include <scenefx/types/wlr_scene.h>
#include <scenefx/types/fx/corner_location.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <wlr/util/region.h>
#include <xkbcommon/xkbcommon.h>
#ifdef XWAYLAND
#include <wlr/xwayland.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#endif

#include "util.h"
#include "tree.h"
#include "workspace.h"

/* macros */
#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#define MIN(A, B)               ((A) < (B) ? (A) : (B))
#define CLEANMASK(mask)         (mask & ~WLR_MODIFIER_CAPS)
#define VISIBLEON(C, M)         ((M) && (C)->mon == (M) && ((C)->ws == (M)->active_workspace || ((C)->ws && (C)->ws->id == SCRATCHPAD_WORKSPACE && (M)->scratchpad_showing)))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define END(A)                  ((A) + LENGTH(A))
#define LISTEN(E, L, H)         wl_signal_add((E), ((L)->notify = (H), (L)))
#define LISTEN_STATIC(E, H)     do { struct wl_listener *_l = ecalloc(1, sizeof(*_l)); _l->notify = (H); wl_signal_add((E), _l); } while (0)

/* enums */
enum { CurNormal, CurPressed, CurMove, CurResize }; /* cursor */
enum { XDGShell, LayerShell, X11 }; /* client types */
enum { LyrBg, LyrBottom, LyrTile, LyrFloat, LyrTop, LyrFS, LyrOverlay, LyrBlock, NUM_LAYERS }; /* scene layers */

typedef union Arg Arg;
union Arg {
	int i;
	uint32_t ui;
	float f;
	const void *v;
};

typedef struct {
	unsigned int mod;
	unsigned int button;
	void (*func)(const Arg *);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
	/* Must keep this field first */
	unsigned int type; /* XDGShell or X11* */

	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_rect *border; /* background border rect */
	struct wlr_scene_tree *scene_surface;
	struct wl_list link;
	struct wl_list flink;
	struct wlr_box geom; /* layout-relative, includes border */
	struct wlr_box prev; /* layout-relative, includes border */
	struct wlr_box bounds; /* only width and height are used */
	union {
		struct wlr_xdg_surface *xdg;
		struct wlr_xwayland_surface *xwayland;
	} surface;
	struct wlr_xdg_toplevel_decoration_v1 *decoration;
	struct wl_listener commit;
	struct wl_listener map;
	struct wl_listener maximize;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener set_title;
	struct wl_listener fullscreen;
	struct wl_listener set_decoration_mode;
	struct wl_listener destroy_decoration;
#ifdef XWAYLAND
	struct wl_listener activate;
	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener configure;
	struct wl_listener set_hints;
#endif
	unsigned int bw;
	int isfloating, isurgent, isfullscreen, wasfloating;
	uint32_t resize; /* configure serial of a pending resize */
	char label;
	struct wlr_scene_tree *label_tree;
	int prev_workspace;
	Workspace *ws;
	Node *node;
};

typedef struct {
	uint32_t mod;
	xkb_keysym_t keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	struct wlr_keyboard_group *wlr_group;

	int nsyms;
	const xkb_keysym_t *keysyms; /* invalid if nsyms == 0 */
	uint32_t mods; /* invalid if nsyms == 0 */
	struct wl_event_source *key_repeat_source;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
} KeyboardGroup;

typedef struct {
	/* Must keep this field first */
	unsigned int type; /* LayerShell */

	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_tree *popups;
	struct wlr_scene_layer_surface_v1 *scene_layer;
	struct wl_list link;
	int mapped;
	struct wlr_layer_surface_v1 *layer_surface;

	struct wl_listener destroy;
	struct wl_listener unmap;
	struct wl_listener surface_commit;
} LayerSurface;

typedef struct Layout {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

struct Monitor {
	struct wl_list link;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_scene_rect *fullscreen_bg; /* See createmon() for info */
	struct wl_listener frame;
	struct wl_listener destroy;
	struct wl_listener request_state;
	struct wl_listener destroy_lock_surface;
	struct wlr_session_lock_surface_v1 *lock_surface;
	struct wlr_box m; /* monitor area, layout-relative */
	struct wlr_box w; /* window area, layout-relative */
	struct wl_list layers[4]; /* LayerSurface.link */
	const Layout *lt[2];
	unsigned int sellt;
	float mfact;
	int gamma_lut_changed;
	int nmaster;
	char ltsymbol[16];
	int asleep;
	int isoverview;
	Client *overview_prev_client;
	Workspace *overview_prev_ws;
	int scratchpad_showing;
	struct wl_list workspaces; /* List of Workspace structs */
	Workspace *active_workspace;
};

typedef struct {
	const char *name;
	float mfact;
	int nmaster;
	float scale;
	const Layout *lt;
	enum wl_output_transform rr;
	int x, y;
} MonitorRule;

typedef struct {
	struct wlr_pointer_constraint_v1 *constraint;
	struct wl_listener destroy;
} PointerConstraint;

typedef struct {
	const char *id;
	const char *title;
	int isfloating;
	int monitor;
} Rule;

typedef struct {
	struct wlr_scene_tree *scene;

	struct wlr_session_lock_v1 *lock;
	struct wl_listener new_surface;
	struct wl_listener unlock;
	struct wl_listener destroy;
} SessionLock;

/* global variables */
extern struct wl_list clients;
extern struct wl_list fstack;
extern struct wl_list mons;
extern Monitor *selmon;
extern struct wlr_seat *seat;
extern struct wlr_scene_tree *layers[NUM_LAYERS];
extern struct wlr_cursor *cursor;
extern struct wlr_xcursor_manager *cursor_mgr;
extern unsigned int cursor_mode;
extern Client *grabc;
extern int grabcx, grabcy;
extern struct wlr_pointer_constraint_v1 *active_constraint;
extern struct wlr_pointer_constraints_v1 *pointer_constraints;
extern struct wlr_relative_pointer_manager_v1 *relative_pointer_mgr;
extern struct wlr_idle_notifier_v1 *idle_notifier;
extern struct wlr_scene_tree *drag_icon;
extern int locked;
extern KeyboardGroup *kb_group;
extern struct wl_event_loop *event_loop;
extern struct wlr_output_layout *output_layout;
extern struct wlr_box sgeom;
extern struct wlr_scene *scene;
extern struct wlr_renderer *drw;
extern struct wlr_allocator *alloc;
extern struct wlr_output_manager_v1 *output_mgr;
extern struct wlr_scene_rect *root_bg;
extern struct wlr_scene_rect *locked_bg;
extern void *exclusive_focus;
extern const int layermap[];
extern int log_level;

/* function declarations */
void checkidleinhibitor(struct wlr_surface *exclude);
KeyboardGroup *createkeyboardgroup(void);
void destroykeyboardgroup(struct wl_listener *listener, void *data);
void destroylocksurface(struct wl_listener *listener, void *data);
void requestmonstate(struct wl_listener *listener, void *data);
void focusclient(Client *c, int lift);
Client *focustop(Monitor *m);
int keybinding(uint32_t mods, xkb_keysym_t sym);
void motionnotify(uint32_t time, struct wlr_input_device *device, double dx, double dy, double dx_unaccel, double dy_unaccel);
void pointerfocus(Client *c, struct wlr_surface *surface, double sx, double sy, uint32_t time);
void printstatus(void);
void resize(Client *c, struct wlr_box geo, int interact);
void setfloating(Client *c, int floating);
void setfullscreen(Client *c, int fullscreen);
void setmon(Client *c, Monitor *m);
Monitor *xytomon(double x, double y);
void xytonode(double x, double y, struct wlr_surface **psurface, Client **pc, LayerSurface **pl, double *nx, double *ny);

/* action & binding callbacks */
void chvt(const Arg *arg);
void focusmon(const Arg *arg);
void focusstack(const Arg *arg);
void incnmaster(const Arg *arg);
void killclient(const Arg *arg);
void moveresize(const Arg *arg);
void quit(const Arg *arg);
void setlayout(const Arg *arg);
void setmfact(const Arg *arg);
void spawn(const Arg *arg);
void tagmon(const Arg *arg);
void togglefloating(const Arg *arg);
void togglefullscreen(const Arg *arg);
void zoom(const Arg *arg);

/* layout algorithms */
void tile(Monitor *m);
void monocle(Monitor *m);
void dwindle(Monitor *m);
void spiral(Monitor *m);

#endif /* DWL_H */
