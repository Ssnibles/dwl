/*
 * See LICENSE file for copyright and license details.
 */
#ifndef SERVER_H
#define SERVER_H

#include "dwl.h"

struct Server {
	struct wl_display *dpy;
	struct wl_event_loop *event_loop;
	struct wlr_backend *backend;
	struct wlr_scene *scene;
	struct wlr_scene_tree *layers[NUM_LAYERS];
	struct wlr_scene_tree *drag_icon;
	struct wlr_renderer *drw;
	struct wlr_allocator *alloc;
	struct wlr_compositor *compositor;
	struct wlr_session *session;

	struct wlr_xdg_shell *xdg_shell;
	struct wlr_xdg_activation_v1 *activation;
	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
	struct wlr_idle_notifier_v1 *idle_notifier;
	struct wlr_idle_inhibit_manager_v1 *idle_inhibit_mgr;
	struct wlr_layer_shell_v1 *layer_shell;
	struct wlr_output_manager_v1 *output_mgr;
	struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_mgr;
	struct wlr_virtual_pointer_manager_v1 *virtual_pointer_mgr;
	struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;
	struct wlr_output_power_manager_v1 *power_mgr;

	struct wlr_pointer_constraints_v1 *pointer_constraints;
	struct wlr_relative_pointer_manager_v1 *relative_pointer_mgr;
	struct wlr_pointer_constraint_v1 *active_constraint;

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;

	struct wlr_scene_rect *root_bg;
	struct wlr_session_lock_manager_v1 *session_lock_mgr;
	struct wlr_scene_rect *locked_bg;
	struct wlr_session_lock_v1 *cur_lock;

	struct wlr_seat *seat;
	KeyboardGroup *kb_group;
	struct wlr_output_layout *output_layout;

	pid_t child_pid;
	const char *socket;
};

extern struct Server server;

/* Global server variables */
extern struct wl_display *dpy;
extern struct wl_event_loop *event_loop;
extern struct wlr_backend *backend;
extern struct wlr_scene *scene;
extern struct wlr_scene_tree *layers[NUM_LAYERS];
extern struct wlr_scene_tree *drag_icon;
extern struct wlr_renderer *drw;
extern struct wlr_allocator *alloc;
extern struct wlr_compositor *compositor;
extern struct wlr_session *session;
extern struct wlr_xdg_shell *xdg_shell;
extern struct wlr_xdg_activation_v1 *activation;
extern struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
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
extern struct wlr_output_layout *output_layout;
extern pid_t child_pid;

#ifdef XWAYLAND
extern struct wlr_xwayland *xwayland;
extern struct wl_listener new_xwayland_surface;
extern struct wl_listener xwayland_ready;
#endif

extern struct wl_listener cursor_axis;
extern struct wl_listener cursor_button;
extern struct wl_listener cursor_frame;
extern struct wl_listener cursor_motion;
extern struct wl_listener cursor_motion_absolute;
extern struct wl_listener gpu_reset;
extern struct wl_listener layout_change;
extern struct wl_listener new_idle_inhibitor;
extern struct wl_listener new_input_device;
extern struct wl_listener new_virtual_keyboard;
extern struct wl_listener new_virtual_pointer;
extern struct wl_listener new_pointer_constraint;
extern struct wl_listener new_output;
extern struct wl_listener new_xdg_toplevel;
extern struct wl_listener new_xdg_popup;
extern struct wl_listener new_xdg_decoration;
extern struct wl_listener new_layer_surface;
extern struct wl_listener output_mgr_apply;
extern struct wl_listener output_mgr_test;
extern struct wl_listener output_power_mgr_set_mode;
extern struct wl_listener request_activate;
extern struct wl_listener request_cursor;
extern struct wl_listener request_set_psel;
extern struct wl_listener request_set_sel;
extern struct wl_listener request_set_cursor_shape;
extern struct wl_listener request_start_drag;
extern struct wl_listener start_drag;
extern struct wl_listener new_session_lock;

void setup(void);
void run(const char *startup_cmd);
void cleanup(void);

#endif /* SERVER_H */
