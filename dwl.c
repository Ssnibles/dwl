/*
 * DWL - Dynamic Window Manager for Wayland
 * Main Entry Point and Server Lifecycle Manager
 */

#include "dwl.h"

/* --- Global Variable Definitions --- */
pid_t child_pid = -1;
int locked;
void *exclusive_focus;
struct wl_display *dpy;
struct wl_event_loop *event_loop;
struct wlr_backend *backend;
struct wlr_scene *scene;
struct wlr_scene_tree *layers[NUM_LAYERS];
struct wlr_scene_tree *drag_icon;
const int layermap[] = { LyrBg, LyrBottom, LyrTop, LyrOverlay };
struct wlr_renderer *drw;
struct wlr_allocator *alloc;
struct wlr_compositor *compositor;
struct wlr_session *session;

struct wlr_xdg_shell *xdg_shell;
struct wlr_xdg_activation_v1 *activation;
struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
struct wl_list clients;
struct wl_list fstack;
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
unsigned int cursor_mode;
Client *grabc;
int grabcx, grabcy;
int grabc_was_tiled;

struct wlr_output_layout *output_layout;
struct wlr_box sgeom;
struct wl_list mons;
Monitor *selmon;

#ifdef XWAYLAND
struct wlr_xwayland *xwayland;
#endif

/* Forward declarations for internal lifecycle listeners */
static void cleanuplisteners(void);
static void gpureset(struct wl_listener *listener, void *data);
static void setpsel(struct wl_listener *listener, void *data);
static void setsel(struct wl_listener *listener, void *data);
static void run(char *startup_cmd);
static void setup(void);
static void cleanup(void);

/* --- Global Event Listeners --- */
static struct wl_listener cursor_axis = {.notify = axisnotify};
static struct wl_listener cursor_button = {.notify = buttonpress};
static struct wl_listener cursor_frame = {.notify = cursorframe};
static struct wl_listener cursor_motion = {.notify = motionrelative};
static struct wl_listener cursor_motion_absolute = {.notify = motionabsolute};
static struct wl_listener gpu_reset = {.notify = gpureset};
static struct wl_listener layout_change = {.notify = updatemons};
static struct wl_listener new_idle_inhibitor = {.notify = createidleinhibitor};
static struct wl_listener new_input_device = {.notify = inputdevice};
static struct wl_listener new_virtual_keyboard = {.notify = virtualkeyboard};
static struct wl_listener new_virtual_pointer = {.notify = virtualpointer};
static struct wl_listener new_pointer_constraint = {.notify = createpointerconstraint};
static struct wl_listener new_output = {.notify = createmon};
static struct wl_listener new_xdg_toplevel = {.notify = createnotify};
static struct wl_listener new_xdg_popup = {.notify = createpopup};
static struct wl_listener new_xdg_decoration = {.notify = createdecoration};
static struct wl_listener new_layer_surface = {.notify = createlayersurface};
static struct wl_listener output_mgr_apply = {.notify = outputmgrapply};
static struct wl_listener output_mgr_test = {.notify = outputmgrtest};
static struct wl_listener output_power_mgr_set_mode = {.notify = powermgrsetmode};
static struct wl_listener request_activate = {.notify = urgent};
static struct wl_listener request_cursor = {.notify = setcursor};
static struct wl_listener request_set_psel = {.notify = setpsel};
static struct wl_listener request_set_sel = {.notify = setsel};
static struct wl_listener request_set_cursor_shape = {.notify = setcursorshape};
static struct wl_listener request_start_drag = {.notify = requeststartdrag};
static struct wl_listener start_drag = {.notify = startdrag};
static struct wl_listener new_session_lock = {.notify = locksession};

/* Handles SIGCHLD signal for spawned child processes */
void
handlesig(int signo)
{
	if (signo == SIGCHLD) {
		siginfo_t in;
		while (!waitid(P_ALL, 0, &in, WNOHANG | WEXITED))
			;
	}
}

/* Switches Virtual Terminal (VT) */
void
chvt(const Arg *arg)
{
	wlr_session_change_vt(session, arg->ui);
}

/* Emits status updates for status bar integration */
void
printstatus(void)
{
	Client *c;
	unsigned int occ = 0, tagset;

	if (!selmon)
		return;

	wl_list_for_each(c, &clients, link) {
		if (c->mon == selmon)
			occ |= c->tags;
	}

	tagset = selmon->tagset[selmon->seltags];
	c = focustop(selmon);

	printf("%s %u %u %u %s %s\n",
			selmon->wlr_output->name,
			tagset,
			occ,
			selmon->seltags,
			selmon->ltsymbol,
			c ? client_get_title(c) : "");
	fflush(stdout);
}

/* Spawns child process via shell execution */
void
spawn(const Arg *arg)
{
	if (fork() == 0) {
		if (dpy)
			close(wl_event_loop_get_fd(event_loop));
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		die("dwl: execvp %s failed:", ((char **)arg->v)[0]);
	}
}

/* Terminates compositor event loop */
void
quit(const Arg *arg)
{
	wl_display_terminate(dpy);
}

/* GPU reset notification handler */
static void
gpureset(struct wl_listener *listener, void *data)
{
	wlr_scene_node_set_enabled(&scene->tree.node, 1);
}

/* Sets primary selection clipboard */
static void
setpsel(struct wl_listener *listener, void *data)
{
	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(seat, event->source, event->serial);
}

/* Sets regular selection clipboard */
static void
setsel(struct wl_listener *listener, void *data)
{
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(seat, event->source, event->serial);
}

/* Unlinks global event listeners upon server shutdown */
static void
cleanuplisteners(void)
{
	wl_list_remove(&cursor_axis.link);
	wl_list_remove(&cursor_button.link);
	wl_list_remove(&cursor_frame.link);
	wl_list_remove(&cursor_motion.link);
	wl_list_remove(&cursor_motion_absolute.link);
	wl_list_remove(&gpu_reset.link);
	wl_list_remove(&layout_change.link);
	wl_list_remove(&new_idle_inhibitor.link);
	wl_list_remove(&new_input_device.link);
	wl_list_remove(&new_virtual_keyboard.link);
	wl_list_remove(&new_virtual_pointer.link);
	wl_list_remove(&new_pointer_constraint.link);
	wl_list_remove(&new_output.link);
	wl_list_remove(&new_xdg_toplevel.link);
	wl_list_remove(&new_xdg_popup.link);
	wl_list_remove(&new_xdg_decoration.link);
	wl_list_remove(&new_layer_surface.link);
	wl_list_remove(&output_mgr_apply.link);
	wl_list_remove(&output_mgr_test.link);
	wl_list_remove(&output_power_mgr_set_mode.link);
	wl_list_remove(&request_activate.link);
	wl_list_remove(&request_cursor.link);
	wl_list_remove(&request_set_psel.link);
	wl_list_remove(&request_set_sel.link);
	wl_list_remove(&request_set_cursor_shape.link);
	wl_list_remove(&request_start_drag.link);
	wl_list_remove(&start_drag.link);
	wl_list_remove(&new_session_lock.link);
}

/* Destroys compositor resources upon shutdown */
static void
cleanup(void)
{
	cleanuplisteners();

#ifdef XWAYLAND
	wlr_xwayland_destroy(xwayland);
#endif

	wlr_seat_destroy(seat);
	wlr_xcursor_manager_destroy(cursor_mgr);
	wlr_cursor_destroy(cursor);
	wlr_output_layout_destroy(output_layout);
	wlr_allocator_destroy(alloc);
	wlr_renderer_destroy(drw);
	wlr_backend_destroy(backend);
	wl_display_destroy_clients(dpy);
	wl_display_destroy(dpy);
}

/* Configures Wayland display, backend, scene graph, and protocols */
static void
setup(void)
{
	struct sigaction sa;

	/* Clean up child processes */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = handlesig;
	sigaction(SIGCHLD, &sa, NULL);

	wl_list_init(&clients);
	wl_list_init(&fstack);
	wl_list_init(&mons);

	dpy = wl_display_create();
	event_loop = wl_display_get_event_loop(dpy);

	backend = wlr_backend_autocreate(event_loop, &session);
	if (!backend)
		die("could not create backend");

	drw = wlr_renderer_autocreate(backend);
	if (!drw)
		die("could not create renderer");

	wlr_renderer_init_wl_display(drw, dpy);

	alloc = wlr_allocator_autocreate(backend, drw);
	if (!alloc)
		die("could not create allocator");

	compositor = wlr_compositor_create(dpy, 5, drw);
	wlr_subcompositor_create(dpy);
	wlr_data_device_manager_create(dpy);

	/* Initialize scene graph and scene layers */
	scene = wlr_scene_create();
	root_bg = wlr_scene_rect_create(&scene->tree, 0, 0, rootcolor);

	for (size_t i = 0; i < NUM_LAYERS; i++)
		layers[i] = wlr_scene_tree_create(&scene->tree);

	output_layout = wlr_output_layout_create(dpy);

	/* Wayland protocols */
	xdg_shell = wlr_xdg_shell_create(dpy, 6);
	LISTEN(&xdg_shell->events.new_toplevel, &new_xdg_toplevel, createnotify);
	LISTEN(&xdg_shell->events.new_popup, &new_xdg_popup, createpopup);

	layer_shell = wlr_layer_shell_v1_create(dpy, 4);
	LISTEN(&layer_shell->events.new_surface, &new_layer_surface, createlayersurface);

	/* Input initialization */
	cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(cursor, output_layout);

	cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	wlr_xcursor_manager_load(cursor_mgr, 1);

	seat = wlr_seat_create(dpy, "seat0");
	kb_group = createkeyboardgroup();

	LISTEN(&backend->events.new_input, &new_input_device, inputdevice);
	LISTEN(&backend->events.new_output, &new_output, createmon);
	LISTEN(&cursor->events.axis, &cursor_axis, axisnotify);
	LISTEN(&cursor->events.button, &cursor_button, buttonpress);
	LISTEN(&cursor->events.frame, &cursor_frame, cursorframe);
	LISTEN(&cursor->events.motion, &cursor_motion, motionrelative);
	LISTEN(&cursor->events.motion_absolute, &cursor_motion_absolute, motionabsolute);

	/* Additional protocols */
	activation = wlr_xdg_activation_v1_create(dpy);
	LISTEN(&activation->events.request_activate, &request_activate, urgent);

	idle_notifier = wlr_idle_notifier_v1_create(dpy);
	idle_inhibit_mgr = wlr_idle_inhibit_v1_create(dpy);
	LISTEN(&idle_inhibit_mgr->events.new_inhibitor, &new_idle_inhibitor, createidleinhibitor);

	output_mgr = wlr_output_manager_v1_create(dpy);
	LISTEN(&output_mgr->events.apply, &output_mgr_apply, outputmgrapply);
	LISTEN(&output_mgr->events.test, &output_mgr_test, outputmgrtest);

	power_mgr = wlr_output_power_manager_v1_create(dpy);
	LISTEN(&power_mgr->events.set_mode, &output_power_mgr_set_mode, powermgrsetmode);

	session_lock_mgr = wlr_session_lock_manager_v1_create(dpy);
	LISTEN(&session_lock_mgr->events.new_lock, &new_session_lock, locksession);

	virtual_keyboard_mgr = wlr_virtual_keyboard_manager_v1_create(dpy);
	LISTEN(&virtual_keyboard_mgr->events.new_virtual_keyboard, &new_virtual_keyboard, virtualkeyboard);

	virtual_pointer_mgr = wlr_virtual_pointer_manager_v1_create(dpy);
	LISTEN(&virtual_pointer_mgr->events.new_virtual_pointer, &new_virtual_pointer, virtualpointer);

	cursor_shape_mgr = wlr_cursor_shape_manager_v1_create(dpy, 1);
	LISTEN(&cursor_shape_mgr->events.request_set_shape, &request_set_cursor_shape, setcursorshape);
}

/* Starts backend and runs compositor main loop */
static void
run(char *startup_cmd)
{
	const char *socket;

	if (!wlr_backend_start(backend))
		die("failed to start backend");

	socket = wl_display_add_socket_auto(dpy);
	if (!socket)
		die("failed to add Wayland socket");

	setenv("WAYLAND_DISPLAY", socket, 1);

	if (startup_cmd) {
		Arg a = {.v = (const char *[]){ "/bin/sh", "-c", startup_cmd, NULL }};
		spawn(&a);
	}

	wlr_log(WLR_INFO, "Running dwl on Wayland display '%s'", socket);
	wl_display_run(dpy);
}

/* Application entry point */
int
main(int argc, char *argv[])
{
	char *startup_cmd = NULL;
	int c;

	while ((c = getopt(argc, argv, "s:v")) != -1) {
		switch (c) {
		case 's':
			startup_cmd = optarg;
			break;
		case 'v':
			puts("dwl " VERSION);
			return 0;
		default:
			return 1;
		}
	}

	setup();
	run(startup_cmd);
	cleanup();

	return 0;
}
