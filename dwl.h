/*
 * DWL - Dynamic Window Manager for Wayland
 * Main header file containing shared macros, data structures, global variables,
 * and module interface declarations.
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

/* --- Core Macros --- */
#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#define MIN(A, B)               ((A) < (B) ? (A) : (B))
#define CLEANMASK(mask)         (mask & ~WLR_MODIFIER_CAPS)
#define VISIBLEON(C, M)         ((M) && (C)->mon == (M) && ((M)->isoverview || ((C)->tags & (M)->tagset[(M)->seltags])))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define END(A)                  ((A) + LENGTH(A))
#define TAGMASK                 ((1u << TAGCOUNT) - 1)
#define LISTEN(E, L, H)         wl_signal_add((E), ((L)->notify = (H), (L)))
#define LISTEN_STATIC(E, H)     do { struct wl_listener *_l = ecalloc(1, sizeof(*_l)); _l->notify = (H); wl_signal_add((E), _l); } while (0)

/* --- Enums --- */
enum { CurNormal, CurPressed, CurMove, CurResize }; /* Cursor modes */
enum { XDGShell, LayerShell, X11 };                 /* Client window types */
enum { LyrBg, LyrBottom, LyrTile, LyrFloat, LyrTop, LyrFS, LyrOverlay, LyrBlock, NUM_LAYERS }; /* Scene graph layers */

/* --- Data Structures --- */

/* Argument union passed to keybindings and commands */
typedef union {
	int i;
	uint32_t ui;
	float f;
	const void *v;
} Arg;

/* Mouse button binding structure */
typedef struct {
	unsigned int mod;
	unsigned int button;
	void (*func)(const Arg *);
	const Arg arg;
} Button;

/* Forward declaration for Monitor */
typedef struct Monitor Monitor;

/* Representation of an application window client */
typedef struct {
	/* Must keep type field first */
	unsigned int type; /* XDGShell or X11 */

	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_rect *border; /* Background border rect */
	struct wlr_scene_tree *scene_surface;
	struct wl_list link;
	struct wl_list flink;
	struct wlr_box geom;   /* Layout-relative geometry, including border */
	struct wlr_box prev;   /* Saved layout-relative geometry */
	struct wlr_box bounds; /* Width and height bounds */
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
	uint32_t tags;
	int isfloating, isurgent, isfullscreen;
	uint32_t resize; /* Configure serial of pending resize */
} Client;

/* Keyboard keybinding structure */
typedef struct {
	uint32_t mod;
	xkb_keysym_t keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

/* Keyboard group abstraction */
typedef struct {
	struct wlr_keyboard_group *wlr_group;

	int nsyms;
	const xkb_keysym_t *keysyms;
	uint32_t mods;
	struct wl_event_source *key_repeat_source;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
} KeyboardGroup;

/* Layer shell surface (status bars, overlays) */
typedef struct {
	/* Must keep type field first */
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

/* Layout arrangement descriptor */
typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

/* Monitor / Display output representation */
struct Monitor {
	struct wl_list link;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_scene_rect *fullscreen_bg;
	struct wl_listener frame;
	struct wl_listener destroy;
	struct wl_listener request_state;
	struct wl_listener destroy_lock_surface;
	struct wlr_session_lock_surface_v1 *lock_surface;
	struct wlr_box m; /* Monitor area, layout-relative */
	struct wlr_box w; /* Usable window area, layout-relative */
	struct wl_list layers[4]; /* LayerSurface link lists */
	const Layout *lt[2];
	unsigned int seltags;
	unsigned int sellt;
	uint32_t tagset[2];
	float mfact;
	int gamma_lut_changed;
	int nmaster;
	char ltsymbol[16];
	int asleep;
	int isoverview;
	uint32_t prevtagset;
};

/* Rule for monitor configurations */
typedef struct {
	const char *name;
	float mfact;
	int nmaster;
	float scale;
	const Layout *lt;
	enum wl_output_transform rr;
	int x, y;
} MonitorRule;

/* Pointer constraint wrapper */
typedef struct {
	struct wlr_pointer_constraint_v1 *constraint;
	struct wl_listener destroy;
} PointerConstraint;

/* Application window layout rule */
typedef struct {
	const char *id;
	const char *title;
	uint32_t tags;
	int isfloating;
	int monitor;
} Rule;

/* Session lock structure */
typedef struct {
	struct wlr_scene_tree *scene;
	struct wlr_session_lock_v1 *lock;
	struct wl_listener new_surface;
	struct wl_listener unlock;
	struct wl_listener destroy;
} SessionLock;

/* --- Shared Global Variables --- */
extern pid_t child_pid;
extern int locked;
extern void *exclusive_focus;
extern struct wl_display *dpy;
extern struct wl_event_loop *event_loop;
extern struct wlr_backend *backend;
extern struct wlr_scene *scene;
extern struct wlr_scene_tree *layers[NUM_LAYERS];
extern struct wlr_scene_tree *drag_icon;
extern const int layermap[];
extern struct wlr_renderer *drw;
extern struct wlr_allocator *alloc;
extern struct wlr_compositor *compositor;
extern struct wlr_session *session;

extern struct wlr_xdg_shell *xdg_shell;
extern struct wlr_xdg_activation_v1 *activation;
extern struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
extern struct wl_list clients;
extern struct wl_list fstack;
extern struct wlr_idle_notifier_v1 *idle_notifier;
extern struct wlr_idle_inhibit_manager_v1 *idle_inhibit_mgr;
extern struct wlr_layer_shell_v1 *layer_shell;
extern struct wlr_output_manager_v1 *output_mgr;
extern struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_mgr;
extern struct wlr_virtual_pointer_manager_v1 *virtual_pointer_mgr;
extern struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;
extern struct wlr_output_power_manager_v1 *power_mgr;

extern struct wlr_pointer_constraints_v1 *pointer_constraints;
extern struct wlr_relative_pointer_manager_v1 *relative_pointer_mgr;
extern struct wlr_pointer_constraint_v1 *active_constraint;

extern struct wlr_cursor *cursor;
extern struct wlr_xcursor_manager *cursor_mgr;

extern struct wlr_scene_rect *root_bg;
extern struct wlr_session_lock_manager_v1 *session_lock_mgr;
extern struct wlr_scene_rect *locked_bg;
extern struct wlr_session_lock_v1 *cur_lock;

extern struct wlr_seat *seat;
extern KeyboardGroup *kb_group;
extern unsigned int cursor_mode;
extern Client *grabc;
extern int grabcx, grabcy;
extern int grabc_was_tiled;

extern struct wlr_output_layout *output_layout;
extern struct wlr_box sgeom;
extern struct wl_list mons;
extern Monitor *selmon;

#ifdef XWAYLAND
extern struct wlr_xwayland *xwayland;
#endif

/* --- Core Helper Declarations --- */
void spawn(const Arg *arg);
void quit(const Arg *arg);
void chvt(const Arg *arg);
void printstatus(void);
void handlesig(int signo);

/* --- Module Headers --- */
#include "client.h"
#include "monitor.h"
#include "layout.h"
#include "input.h"
#include "layer.h"
#include "session.h"
#include "config.h"

#endif /* DWL_H */
