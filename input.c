/*
 * DWL - Input Handling Module
 * Processes pointer motion, scroll wheel events, key press & bindings,
 * interactive window move/resize operations, and input device initialization.
 */

#include "dwl.h"

/* Processes mouse scroll wheel events */
void
axisnotify(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_axis_event *event = data;

	wlr_seat_pointer_notify_axis(seat, event->time_msec, event->orientation,
			event->delta, event->delta_discrete, event->source, event->relative_direction);
}

/* Processes mouse button presses and clicks */
void
buttonpress(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_button_event *event = data;
	struct wlr_surface *surface = NULL;
	Client *c = NULL;
	LayerSurface *l = NULL;
	const Button *b;
	double sx, sy;

	/* Send button notify event to seat */
	wlr_seat_pointer_notify_button(seat, event->time_msec, event->button, event->state);

	xytonode(cursor->x, cursor->y, &surface, &c, &l, &sx, &sy);

	if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
		/* Overview Mode click-to-focus behavior */
		if (selmon && selmon->isoverview && c) {
			focusclient(c, 1);
			toggleoverview(NULL);
			return;
		}

		/* Check mouse button bindings */
		for (b = buttons; b < END(buttons); b++) {
			if (CLEANMASK(kb_group->mods) == CLEANMASK(b->mod)
					&& event->button == b->button && b->func) {
				b->func(&b->arg);
				return;
			}
		}

		if (c)
			focusclient(c, 1);
	}
}

/* Configures newly attached keyboard device */
void
createkeyboard(struct wlr_keyboard *keyboard)
{
	struct xkb_context *context;
	struct xkb_keymap *keymap;

	wlr_keyboard_set_repeat_info(keyboard, repeat_rate, repeat_delay);

	context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	keymap = xkb_keymap_new_from_names(context, &xkb_rules,
			XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);

	wlr_keyboard_group_add_keyboard(kb_group->wlr_group, keyboard);
}

/* Creates keyboard group structure */
KeyboardGroup *
createkeyboardgroup(void)
{
	KeyboardGroup *group = ecalloc(1, sizeof(*group));
	group->wlr_group = wlr_keyboard_group_create();

	LISTEN(&group->wlr_group->keyboard.events.key, &group->key, keypress);
	LISTEN(&group->wlr_group->keyboard.events.modifiers, &group->modifiers, keypressmod);

	return group;
}

/* Destroys keyboard group resources */
void
destroykeyboardgroup(struct wl_listener *listener, void *data)
{
	KeyboardGroup *group = wl_container_of(listener, group, destroy);
	wlr_keyboard_group_destroy(group->wlr_group);
	free(group);
}

/* Configures newly attached pointer device */
void
createpointer(struct wlr_pointer *pointer)
{
	if (wlr_input_device_is_libinput(&pointer->base)) {
		struct libinput_device *libinput_dev =
			wlr_libinput_get_device_handle(&pointer->base);

		if (libinput_device_config_tap_get_finger_count(libinput_dev) > 0) {
			libinput_device_config_tap_set_enabled(libinput_dev, tap_to_click);
			libinput_device_config_tap_set_drag_enabled(libinput_dev, tap_and_drag);
			libinput_device_config_tap_set_drag_lock_enabled(libinput_dev, drag_lock);
			libinput_device_config_tap_set_button_map(libinput_dev, button_map);
		}
		if (libinput_device_config_scroll_has_natural_scroll(libinput_dev)) {
			if (pointer->base.type == WLR_INPUT_DEVICE_POINTER)
				libinput_device_config_scroll_set_natural_scroll_enabled(libinput_dev, mouse_natural_scrolling);
			else
				libinput_device_config_scroll_set_natural_scroll_enabled(libinput_dev, natural_scrolling);
		}
		if (libinput_device_config_dwt_is_available(libinput_dev))
			libinput_device_config_dwt_set_enabled(libinput_dev, disable_while_typing);
		if (libinput_device_config_send_events_get_modes(libinput_dev))
			libinput_device_config_send_events_set_mode(libinput_dev, send_events_mode);
		if (libinput_device_config_accel_is_available(libinput_dev)) {
			libinput_device_config_accel_set_profile(libinput_dev, accel_profile);
			libinput_device_config_accel_set_speed(libinput_dev, accel_speed);
		}
	}

	wlr_cursor_attach_input_device(cursor, &pointer->base);
}

/* Handles pointer constraint creation requests */
void
createpointerconstraint(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_constraint_v1 *constraint = data;
	PointerConstraint *pc = ecalloc(1, sizeof(*pc));
	pc->constraint = constraint;

	LISTEN(&constraint->events.destroy, &pc->destroy, destroypointerconstraint);

	if (seat->pointer_state.focused_surface == constraint->surface)
		cursorconstrain(constraint);
}

/* Constrains cursor within active constraint boundary */
void
cursorconstrain(struct wlr_pointer_constraint_v1 *constraint)
{
	if (active_constraint == constraint)
		return;

	if (active_constraint)
		wlr_pointer_constraint_v1_send_deactivated(active_constraint);

	active_constraint = constraint;
	if (constraint)
		wlr_pointer_constraint_v1_send_activated(constraint);
}

/* Cursor frame notify callback */
void
cursorframe(struct wl_listener *listener, void *data)
{
	wlr_seat_pointer_notify_frame(seat);
}

/* Warps cursor to center of focused window if needed */
void
cursorwarptohint(void)
{
	Client *c = focustop(selmon);
	if (c)
		wlr_cursor_warp(cursor, NULL, c->geom.x + c->geom.width / 2, c->geom.y + c->geom.height / 2);
}

/* Destroys pointer constraint object */
void
destroypointerconstraint(struct wl_listener *listener, void *data)
{
	PointerConstraint *pc = wl_container_of(listener, pc, destroy);
	if (active_constraint == pc->constraint)
		cursorconstrain(NULL);
	wl_list_remove(&pc->destroy.link);
	free(pc);
}

/* Handles new input device attachment */
void
inputdevice(struct wl_listener *listener, void *data)
{
	struct wlr_input_device *device = data;
	uint32_t caps;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		createkeyboard(wlr_keyboard_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_POINTER:
		createpointer(wlr_pointer_from_input_device(device));
		break;
	default:
		break;
	}

	caps = WL_SEAT_CAPABILITY_POINTER;
	if (kb_group->nsyms > 0)
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(seat, caps);
}

/* Matches key combination against configured keys array */
int
keybinding(uint32_t mods, xkb_keysym_t sym)
{
	const Key *k;

	/* Handle ESC / Return in Overview Mode */
	if (selmon && selmon->isoverview) {
		if (sym == XKB_KEY_Escape || sym == XKB_KEY_Return) {
			toggleoverview(NULL);
			return 1;
		}
	}

	for (k = keys; k < END(keys); k++) {
		if (CLEANMASK(mods) == CLEANMASK(k->mod)
				&& sym == k->keysym && k->func) {
			k->func(&k->arg);
			return 1;
		}
	}
	return 0;
}

/* Processes keyboard key press events */
void
keypress(struct wl_listener *listener, void *data)
{
	struct wlr_keyboard_key_event *event = data;
	uint32_t keycode = event->keycode + 8;
	const xkb_keysym_t *syms;
	int nsyms;
	int handled = 0;

	nsyms = xkb_state_key_get_syms(kb_group->wlr_group->keyboard.xkb_state, keycode, &syms);

	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (int i = 0; i < nsyms; i++)
			handled |= keybinding(kb_group->mods, syms[i]);
	}

	if (!handled)
		wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, event->state);
}

/* Updates keyboard modifier status */
void
keypressmod(struct wl_listener *listener, void *data)
{
	KeyboardGroup *group = wl_container_of(listener, group, modifiers);

	wlr_seat_set_keyboard(seat, &group->wlr_group->keyboard);
	wlr_seat_keyboard_notify_modifiers(seat, &group->wlr_group->keyboard.modifiers);
}

/* Key repeat timer callback handler */
int
keyrepeat(void *data)
{
	return 1;
}

/* Absolute cursor motion handler (touchpads / graphics tablets) */
void
motionabsolute(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(cursor, &event->pointer->base, event->x, event->y);
	motionnotify(event->time_msec, &event->pointer->base, 0, 0, 0, 0);
}

/* Processes mouse movement and cursor motion notifications */
void
motionnotify(uint32_t time, struct wlr_input_device *device, double sx,
		double sy, double sx_unaccel, double sy_unaccel)
{
	struct wlr_surface *surface = NULL;
	Client *c = NULL;
	LayerSurface *l = NULL;
	double sub_x, sub_y;

	if (time)
		wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	/* Handle interactive moving or resizing window */
	if (cursor_mode == CurMove && grabc) {
		resize(grabc, (struct wlr_box){
			.x = (int)cursor->x - grabcx,
			.y = (int)cursor->y - grabcy,
			.width = grabc->geom.width,
			.height = grabc->geom.height,
		}, 1);
		return;
	} else if (cursor_mode == CurResize && grabc) {
		resize(grabc, (struct wlr_box){
			.x = grabc->geom.x,
			.y = grabc->geom.y,
			.width = MAX(1, (int)cursor->x - grabc->geom.x),
			.height = MAX(1, (int)cursor->y - grabc->geom.y),
		}, 1);
		return;
	}

	xytonode(cursor->x, cursor->y, &surface, &c, &l, &sub_x, &sub_y);

	/* Sloppy focus window under cursor */
	if (sloppyfocus && c && c != focustop(selmon) && !selmon->isoverview)
		focusclient(c, 0);

	pointerfocus(c, surface, sub_x, sub_y, time);
}

/* Relative cursor motion handler */
void
motionrelative(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_motion_event *event = data;
	wlr_cursor_move(cursor, &event->pointer->base, event->delta_x, event->delta_y);
	motionnotify(event->time_msec, &event->pointer->base, event->delta_x,
			event->delta_y, event->unaccel_dx, event->unaccel_dy);
}

/* Interactive window move/resize keybinding trigger */
void
moveresize(const Arg *arg)
{
	if (!(grabc = focustop(selmon)))
		return;

	if (arg->ui == CurMove) {
		grabcx = (int)cursor->x - grabc->geom.x;
		grabcy = (int)cursor->y - grabc->geom.y;
		cursor_mode = CurMove;
	} else if (arg->ui == CurResize) {
		cursor_mode = CurResize;
	}

	if (!grabc->isfloating)
		setfloating(grabc, 1);
}

/* Updates pointer focus surface and passes motion coordinates */
void
pointerfocus(Client *c, struct wlr_surface *surface,
		double sx, double sy, uint32_t time)
{
	if (surface)
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
	else
		wlr_seat_pointer_clear_focus(seat);

	if (surface && time)
		wlr_seat_pointer_notify_motion(seat, time, sx, sy);
}

/* Request start drag handler */
void
requeststartdrag(struct wl_listener *listener, void *data)
{
	struct wlr_seat_request_start_drag_event *event = data;

	if (wlr_seat_validate_pointer_grab_serial(seat, event->origin, event->serial))
		wlr_seat_start_pointer_drag(seat, event->drag, event->serial);
	else
		wlr_data_source_destroy(event->drag->source);
}

/* Sets cursor theme / image */
void
setcursor(struct wl_listener *listener, void *data)
{
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	if (event->seat_client == seat->pointer_state.focused_client)
		wlr_cursor_set_surface(cursor, event->surface, event->hotspot_x, event->hotspot_y);
}

/* Sets cursor shape via cursor shape protocol */
void
setcursorshape(struct wl_listener *listener, void *data)
{
	struct wlr_cursor_shape_manager_v1_request_set_shape_event *event = data;
	wlr_cursor_set_xcursor(cursor, cursor_mgr, wlr_cursor_shape_v1_name(event->shape));
}

/* Destroys drag icon node */
void
destroydragicon(struct wl_listener *listener, void *data)
{
	/* Focus enter isn't sent during drag, so refocus the focused node. */
	focusclient(focustop(selmon), 1);
	motionnotify(0, NULL, 0, 0, 0, 0);
	wl_list_remove(&listener->link);
	free(listener);
}

/* Drag-and-drop start callback */
void
startdrag(struct wl_listener *listener, void *data)
{
	struct wlr_drag *drag = data;
	if (!drag->icon)
		return;

	drag->icon->data = &wlr_scene_drag_icon_create(drag_icon, drag->icon)->node;
	LISTEN_STATIC(&drag->icon->events.destroy, destroydragicon);
}

/* Virtual keyboard creation request handler */
void
virtualkeyboard(struct wl_listener *listener, void *data)
{
	struct wlr_virtual_keyboard_v1 *keyboard = data;
	createkeyboard(&keyboard->keyboard);
}

/* Virtual pointer creation request handler */
void
virtualpointer(struct wl_listener *listener, void *data)
{
	struct wlr_virtual_pointer_v1_new_pointer_event *event = data;
	createpointer(&event->new_pointer->pointer);
}

/* Resolves screen coordinates (x,y) to scene node, surface, client, or layer surface */
void
xytonode(double x, double y, struct wlr_surface **psurface,
		Client **pc, LayerSurface **pl, double *nx, double *ny)
{
	struct wlr_scene_node *node = wlr_scene_node_at(&scene->tree.node, x, y, nx, ny);
	struct wlr_scene_surface *scene_surface;

	if (!node || node->type != WLR_SCENE_NODE_BUFFER)
		return;

	scene_surface = wlr_scene_surface_try_from_buffer(wlr_scene_buffer_from_node(node));
	if (!scene_surface)
		return;

	*psurface = scene_surface->surface;
	toplevel_from_wlr_surface(*psurface, pc, pl);
}
