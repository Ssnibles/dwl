/*
 * See LICENSE file for copyright and license details.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>

#include "dwl.h"
#include "cursor.h"
#include "layout.h"
#include "client.h"
#include "config.h"
#include "tree.h"

typedef enum {
	SNAP_NONE = 0,
	SNAP_CENTER,
	SNAP_LEFT,
	SNAP_RIGHT,
	SNAP_TOP,
	SNAP_BOTTOM
} SnapTargetType;

static int grabc_was_tiled;
static struct wlr_box grabc_start_geom;
static uint32_t grabc_edges;

static struct wlr_scene_tree *snap_overlay_tree = NULL;
static struct wlr_scene_rect *snap_overlay_border = NULL;
static struct wlr_scene_rect *snap_overlay_bg = NULL;
static SnapTargetType active_snap_type = SNAP_NONE;
static Client *active_snap_target = NULL;

int in_pointer_focus = 0;

void
destroy_snap_overlay(void)
{
	if (snap_overlay_tree) {
		wlr_scene_node_destroy(&snap_overlay_tree->node);
		snap_overlay_tree = NULL;
		snap_overlay_border = NULL;
		snap_overlay_bg = NULL;
	}
	active_snap_type = SNAP_NONE;
	active_snap_target = NULL;
}

static void
update_snap_overlay(struct wlr_box box, SnapTargetType type)
{
	const float border_center[4] = {0.85f, 0.20f, 0.20f, 0.75f};
	const float bg_center[4]     = {0.40f, 0.05f, 0.05f, 0.50f};
	const float border_edge[4]   = {0.75f, 0.15f, 0.15f, 0.70f};
	const float bg_edge[4]       = {0.50f, 0.05f, 0.05f, 0.50f};

	const float *b_color = (type == SNAP_CENTER) ? border_center : border_edge;
	const float *f_color = (type == SNAP_CENTER) ? bg_center : bg_edge;
	int bw = 2;

	if (box.width <= 0 || box.height <= 0) {
		destroy_snap_overlay();
		return;
	}

	if (!snap_overlay_tree) {
		snap_overlay_tree = wlr_scene_tree_create(layers[LyrOverlay]);
		if (!snap_overlay_tree)
			return;

		snap_overlay_border = wlr_scene_rect_create(snap_overlay_tree, box.width, box.height, b_color);
		snap_overlay_bg = wlr_scene_rect_create(snap_overlay_tree, MAX(1, box.width - 2 * bw), MAX(1, box.height - 2 * bw), f_color);
		if (snap_overlay_bg)
			wlr_scene_node_set_position(&snap_overlay_bg->node, bw, bw);
	} else {
		if (snap_overlay_border) {
			wlr_scene_rect_set_size(snap_overlay_border, box.width, box.height);
			wlr_scene_rect_set_color(snap_overlay_border, b_color);
		}
		if (snap_overlay_bg) {
			wlr_scene_rect_set_size(snap_overlay_bg, MAX(1, box.width - 2 * bw), MAX(1, box.height - 2 * bw));
			wlr_scene_rect_set_color(snap_overlay_bg, f_color);
			wlr_scene_node_set_position(&snap_overlay_bg->node, bw, bw);
		}
	}

	wlr_scene_node_set_position(&snap_overlay_tree->node, box.x, box.y);
	wlr_scene_node_set_enabled(&snap_overlay_tree->node, true);
	wlr_scene_node_raise_to_top(&snap_overlay_tree->node);
}

void
axisnotify(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits an axis event,
	 * for example when you move the scroll wheel. */
	struct wlr_pointer_axis_event *event = data;
	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);
	/* TODO: allow usage of scroll wheel for mousebindings, it can be implemented
	 * by checking the event's orientation and the delta of the event */
	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(seat,
			event->time_msec, event->orientation, event->delta,
			event->delta_discrete, event->source, event->relative_direction);
}

void
buttonpress(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_button_event *event = data;
	struct wlr_keyboard *keyboard;
	uint32_t mods;
	Client *c;
	const Button *b;

	in_pointer_focus = 1;

	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	switch (event->state) {
	case WL_POINTER_BUTTON_STATE_PRESSED:
		cursor_mode = CurPressed;
		selmon = xytomon(cursor->x, cursor->y);
		if (!seat->drag)
			wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
		if (locked)
			break;

		/* Change focus if the button was _pressed_ over a client */
		xytonode(cursor->x, cursor->y, NULL, &c, NULL, NULL, NULL);
		if (selmon && selmon->isoverview) {
			if (c)
				focusclient(c, 1);
			toggleoverview(&(Arg){.i = c ? 1 : 0});
			in_pointer_focus = 0;
			return;
		}

		if (selmon && selmon->scratchpad_showing && c && c->ws && c->ws->id != SCRATCHPAD_WORKSPACE) {
			selmon->scratchpad_showing = 0;
			arrange(selmon);
		}

		if (c && (!client_is_unmanaged(c) || client_wants_focus(c)))
			focusclient(c, 1);

		keyboard = wlr_seat_get_keyboard(seat);
		mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;
		for (b = buttons; b < END(buttons); b++) {
			if (CLEANMASK(mods) == CLEANMASK(b->mod) &&
					event->button == b->button && b->func) {
				b->func(&b->arg);
				in_pointer_focus = 0;
				return;
			}
		}
		break;
	case WL_POINTER_BUTTON_STATE_RELEASED:
		/* If you released any buttons, we exit interactive move/resize mode. */
		/* TODO: should reset to the pointer focus's current setcursor */
		if (!locked && cursor_mode != CurNormal && cursor_mode != CurPressed) {
			int was_resize = (cursor_mode == CurResize);
			int was_move = (cursor_mode == CurMove);
			wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
			cursor_mode = CurNormal;
			/* Drop the window off on its new monitor */
			if (grabc) {
				Monitor *m = xytomon(cursor->x, cursor->y);
				if (!m)
					m = grabc->mon ? grabc->mon : selmon;
				if (m) {
					selmon = m;
					setmon(grabc, selmon);
				}
				if (was_move && grabc_was_tiled && active_snap_target && active_snap_type != SNAP_NONE) {
					Client *at = active_snap_target;
					Workspace *ws = grabc->ws ? grabc->ws : (grabc->mon ? grabc->mon->active_workspace : selmon->active_workspace);

					if (active_snap_type == SNAP_CENTER) {
						/* Center Zone: SWAP windows */
						if (grabc->node && at->node) {
							tree_swap_nodes(grabc->node, at->node);
						}
						/* Re-order client list to reflect swap */
						wl_list_remove(&grabc->link);
						wl_list_insert(&at->link, &grabc->link);
					} else {
						/* Edge Zones: SPLIT/INSERT window */
						int dir = WLR_DIRECTION_RIGHT;
						int before = 0;

						switch (active_snap_type) {
						case SNAP_LEFT:
							dir = WLR_DIRECTION_LEFT;
							before = 1;
							break;
						case SNAP_RIGHT:
							dir = WLR_DIRECTION_RIGHT;
							before = 0;
							break;
						case SNAP_TOP:
							dir = WLR_DIRECTION_UP;
							before = 1;
							break;
						case SNAP_BOTTOM:
							dir = WLR_DIRECTION_DOWN;
							before = 0;
							break;
						default:
							break;
						}

						wl_list_remove(&grabc->link);
						if (before)
							wl_list_insert(at->link.prev, &grabc->link);
						else
							wl_list_insert(&at->link, &grabc->link);

						if (ws)
							node_insert_client_at(ws, grabc, at, dir);
					}
					setfloating(grabc, 0);
					arrange(selmon);
				} else if (grabc_was_tiled) {
					setfloating(grabc, 0);
					arrange(selmon);
				}

				if (was_resize && grabc_was_tiled && grabc->node) {
					int old_w = grabc_start_geom.width;
					int old_h = grabc_start_geom.height;
					int new_w = grabc->geom.width;
					int new_h = grabc->geom.height;
					float scale_w = (old_w > 0) ? (float)new_w / (float)old_w : 1.0f;
					float scale_h = (old_h > 0) ? (float)new_h / (float)old_h : 1.0f;
					Workspace *ws = grabc->ws ? grabc->ws : (selmon ? selmon->active_workspace : NULL);
					const Layout *lt = (ws && ws->layout) ? ws->layout : (selmon ? selmon->lt[selmon->sellt] : NULL);

					if (fabsf(scale_w - 1.0f) > 0.01f && scale_w > 0.05f && scale_w < 20.0f)
						grabc->node->ratio_h = clamp_ratio(grabc->node->ratio_h * scale_w);

					if (fabsf(scale_h - 1.0f) > 0.01f && scale_h > 0.05f && scale_h < 20.0f)
						grabc->node->ratio_v = clamp_ratio(grabc->node->ratio_v * scale_h);

					if (selmon && lt && (lt->arrange == tile || lt->arrange == master_stack)) {
						if (fabsf(scale_w - 1.0f) > 0.01f && selmon->w.width > 0) {
							Client *leaves[128];
							int n = node_collect_leaves(ws ? ws->root : NULL, leaves, 128);
							int nm = MIN(n, selmon->nmaster);
							int is_master = 0;
							int i;
							float delta_mfact;
							for (i = 0; i < nm; i++) {
								if (leaves[i] == grabc) {
									is_master = 1;
									break;
								}
							}
							delta_mfact = (float)(new_w - old_w) / (float)selmon->w.width;
							if (is_master)
								selmon->mfact = MIN(0.9f, MAX(0.1f, selmon->mfact + delta_mfact));
							else
								selmon->mfact = MIN(0.9f, MAX(0.1f, selmon->mfact - delta_mfact));
						}
					}
					arrange(selmon);
				}
			}
			destroy_snap_overlay();
			grabc = NULL;
			in_pointer_focus = 0;
			return;
		}
		cursor_mode = CurNormal;
		break;
	}
	/* If the event wasn't handled by the compositor, notify the client with
	 * pointer focus that a button press has occurred */
	wlr_seat_pointer_notify_button(seat,
			event->time_msec, event->button, event->state);
	in_pointer_focus = 0;
}

void
cursorconstrain(struct wlr_pointer_constraint_v1 *constraint)
{
	if (active_constraint == constraint)
		return;

	if (active_constraint)
		wlr_pointer_constraint_v1_send_deactivated(active_constraint);

	active_constraint = constraint;
	wlr_pointer_constraint_v1_send_activated(constraint);
}

void
cursorframe(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits a frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen at the
	 * same time, in which case a frame event won't be sent in between. */
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(seat);
}

void
cursorwarptohint(void)
{
	Client *c = NULL;
	double sx = active_constraint->current.cursor_hint.x;
	double sy = active_constraint->current.cursor_hint.y;

	toplevel_from_wlr_surface(active_constraint->surface, &c, NULL);
	if (c && active_constraint->current.cursor_hint.enabled) {
		wlr_cursor_warp(cursor, NULL, sx + c->geom.x + c->bw, sy + c->geom.y + c->bw);
		wlr_seat_pointer_warp(active_constraint->seat, sx, sy);
	}
}

void
warptocenter(Client *c)
{
	if (!c)
		return;
	wlr_cursor_warp_closest(cursor, NULL,
			c->geom.x + c->geom.width / 2.0,
			c->geom.y + c->geom.height / 2.0);
	motionnotify(0, NULL, 0, 0, 0, 0);
}

void
motionabsolute(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits an _absolute_
	 * motion event, from 0..1 on each axis. This happens, for example, when
	 * wlroots is running under a Wayland window rather than KMS+DRM, and you
	 * move the mouse over the window. You could enter the window from any edge,
	 * so we have to warp the mouse there. Also, some hardware emits these events. */
	struct wlr_pointer_motion_absolute_event *event = data;
	double lx, ly, dx, dy;

	if (!event->time_msec) /* this is 0 with virtual pointers */
		wlr_cursor_warp_absolute(cursor, &event->pointer->base, event->x, event->y);

	wlr_cursor_absolute_to_layout_coords(cursor, &event->pointer->base, event->x, event->y, &lx, &ly);
	dx = lx - cursor->x;
	dy = ly - cursor->y;
	motionnotify(event->time_msec, &event->pointer->base, dx, dy, dx, dy);
}

void
motionnotify(uint32_t time, struct wlr_input_device *device, double dx, double dy,
		double dx_unaccel, double dy_unaccel)
{
	double sx = 0, sy = 0, sx_confined, sy_confined;
	Client *c = NULL, *w = NULL;
	LayerSurface *l = NULL;
	struct wlr_surface *surface = NULL;
	struct wlr_pointer_constraint_v1 *constraint;

	in_pointer_focus = 1;

	/* Find the client under the pointer and send the event along. */
	xytonode(cursor->x, cursor->y, &surface, &c, NULL, &sx, &sy);

	if (cursor_mode == CurPressed && !seat->drag
			&& surface != seat->pointer_state.focused_surface
			&& toplevel_from_wlr_surface(seat->pointer_state.focused_surface, &w, &l) >= 0) {
		c = w;
		surface = seat->pointer_state.focused_surface;
		sx = cursor->x - (l ? l->scene->node.x : w->geom.x);
		sy = cursor->y - (l ? l->scene->node.y : w->geom.y);
	}

	/* time is 0 in internal calls meant to restore pointer focus. */
	if (time) {
		wlr_relative_pointer_manager_v1_send_relative_motion(
				relative_pointer_mgr, seat, (uint64_t)time * 1000,
				dx, dy, dx_unaccel, dy_unaccel);

		wl_list_for_each(constraint, &pointer_constraints->constraints, link) {
			if (constraint->surface == seat->pointer_state.focused_surface) {
				cursorconstrain(constraint);
				break;
			}
		}
		if (&constraint->link == &pointer_constraints->constraints
				&& active_constraint) {
			wlr_pointer_constraint_v1_send_deactivated(active_constraint);
			active_constraint = NULL;
		}

		if (active_constraint && cursor_mode != CurResize && cursor_mode != CurMove) {
			toplevel_from_wlr_surface(active_constraint->surface, &c, NULL);
			if (c && active_constraint->surface == seat->pointer_state.focused_surface) {
				sx = cursor->x - c->geom.x - c->bw;
				sy = cursor->y - c->geom.y - c->bw;
				if (wlr_region_confine(&active_constraint->region, sx, sy,
						sx + dx, sy + dy, &sx_confined, &sy_confined)) {
					dx = sx_confined - sx;
					dy = sy_confined - sy;
				}

				if (active_constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED) {
					in_pointer_focus = 0;
					return;
				}
			}
		}

		wlr_cursor_move(cursor, device, dx, dy);
		wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

		/* Update selmon (even while dragging a window) */
		if (sloppyfocus)
			selmon = xytomon(cursor->x, cursor->y);
	}

	/* Update drag icon's position */
	wlr_scene_node_set_position(&drag_icon->node, (int)round(cursor->x), (int)round(cursor->y));

	/* If we are currently grabbing the mouse, handle and return */
	if (cursor_mode == CurMove) {
		/* Move the grabbed client to the new position. */
		resize(grabc, (struct wlr_box){.x = (int)round(cursor->x) - grabcx, .y = (int)round(cursor->y) - grabcy,
			.width = grabc->geom.width, .height = grabc->geom.height}, 1);

		/* Find nearest tileable client target for snapping feedback only if window was tiled */
		if (grabc_was_tiled) {
			Client *tc, *at = NULL;
			double min_dist = 1e9;

			wl_list_for_each(tc, &clients, link) {
				double tdx = 0, tdy = 0, dist;
				if (!client_is_tileable(tc) || tc == grabc || tc->mon != selmon || tc->ws != grabc->ws)
					continue;
				if (cursor->x < tc->geom.x)
					tdx = tc->geom.x - cursor->x;
				else if (cursor->x > tc->geom.x + tc->geom.width)
					tdx = cursor->x - (tc->geom.x + tc->geom.width);

				if (cursor->y < tc->geom.y)
					tdy = tc->geom.y - cursor->y;
				else if (cursor->y > tc->geom.y + tc->geom.height)
					tdy = cursor->y - (tc->geom.y + tc->geom.height);

				dist = tdx * tdx + tdy * tdy;
				if (dist < min_dist) {
					min_dist = dist;
					at = tc;
				}
			}

			if (at && at->geom.width > 0 && at->geom.height > 0) {
				double norm_x = (cursor->x - at->geom.x) / (double)at->geom.width - 0.5;
				double norm_y = (cursor->y - at->geom.y) / (double)at->geom.height - 0.5;
				struct wlr_box snap_box;

				if (fabs(norm_x) < 0.25 && fabs(norm_y) < 0.25) {
					active_snap_type = SNAP_CENTER;
					active_snap_target = at;
					snap_box = at->geom;
				} else {
					active_snap_target = at;
					if (fabs(norm_x) > fabs(norm_y)) {
						int edge_w = MAX(1, (int)round(at->geom.width * 0.20));
						if (norm_x < 0) {
							active_snap_type = SNAP_LEFT;
							snap_box = (struct wlr_box){
								.x = at->geom.x,
								.y = at->geom.y,
								.width = edge_w,
								.height = at->geom.height
							};
						} else {
							active_snap_type = SNAP_RIGHT;
							snap_box = (struct wlr_box){
								.x = at->geom.x + at->geom.width - edge_w,
								.y = at->geom.y,
								.width = edge_w,
								.height = at->geom.height
							};
						}
					} else {
						int edge_h = MAX(1, (int)round(at->geom.height * 0.20));
						if (norm_y < 0) {
							active_snap_type = SNAP_TOP;
							snap_box = (struct wlr_box){
								.x = at->geom.x,
								.y = at->geom.y,
								.width = at->geom.width,
								.height = edge_h
							};
						} else {
							active_snap_type = SNAP_BOTTOM;
							snap_box = (struct wlr_box){
								.x = at->geom.x,
								.y = at->geom.y + at->geom.height - edge_h,
								.width = at->geom.width,
								.height = edge_h
							};
						}
					}
				}
				update_snap_overlay(snap_box, active_snap_type);
			} else {
				destroy_snap_overlay();
			}
		} else {
			destroy_snap_overlay();
		}

		in_pointer_focus = 0;
		return;
	} else if (cursor_mode == CurResize) {
		int min_w = grabc->isfloating ? (int)min_width + 2 * (int)grabc->bw : 2 * (int)grabc->bw + 1;
		int min_h = grabc->isfloating ? (int)min_height + 2 * (int)grabc->bw : 2 * (int)grabc->bw + 1;
		int fixed_left = grabc_start_geom.x;
		int fixed_right = grabc_start_geom.x + grabc_start_geom.width;
		int fixed_top = grabc_start_geom.y;
		int fixed_bottom = grabc_start_geom.y + grabc_start_geom.height;
		int new_x = grabc->geom.x;
		int new_y = grabc->geom.y;
		int new_w = grabc->geom.width;
		int new_h = grabc->geom.height;

		destroy_snap_overlay();

		if (grabc_edges & WLR_EDGE_LEFT) {
			new_x = (int)round(cursor->x) - grabcx;
			if (fixed_right - new_x < min_w)
				new_x = fixed_right - min_w;
			new_w = fixed_right - new_x;
		} else if (grabc_edges & WLR_EDGE_RIGHT) {
			new_x = fixed_left;
			new_w = (int)round(cursor->x) - fixed_left - grabcx;
			if (new_w < min_w)
				new_w = min_w;
		} else {
			new_x = fixed_left;
			new_w = grabc_start_geom.width;
		}

		if (grabc_edges & WLR_EDGE_TOP) {
			new_y = (int)round(cursor->y) - grabcy;
			if (fixed_bottom - new_y < min_h)
				new_y = fixed_bottom - min_h;
			new_h = fixed_bottom - new_y;
		} else if (grabc_edges & WLR_EDGE_BOTTOM) {
			new_y = fixed_top;
			new_h = (int)round(cursor->y) - fixed_top - grabcy;
			if (new_h < min_h)
				new_h = min_h;
		} else {
			new_y = fixed_top;
			new_h = grabc_start_geom.height;
		}

		resize(grabc, (struct wlr_box){.x = new_x, .y = new_y, .width = new_w, .height = new_h}, 1);
		in_pointer_focus = 0;
		return;
	}

	/* If there's no client surface under the cursor or cursor was moved,
	 * set the cursor image to default. */
	if ((!surface || time) && !seat->drag)
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");

	if (sloppyfocus && time && c && selmon && !selmon->isoverview && !client_is_unmanaged(c)
			&& client_surface(c) != seat->keyboard_state.focused_surface)
		focusclient(c, 0);

	pointerfocus(c, surface, sx, sy, time);
	in_pointer_focus = 0;
}

void
motionrelative(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits a _relative_
	 * pointer motion event (i.e. a delta) */
	struct wlr_pointer_motion_event *event = data;
	/* The cursor doesn't move unless we tell it to. The cursor automatically
	 * handles constraining the motion to the output layout, as well as any
	 * special configuration applied for the specific input device which
	 * generated the event. You can pass NULL for the device if you want to move
	 * the cursor around without any input. */
	motionnotify(event->time_msec, &event->pointer->base, event->delta_x, event->delta_y,
			event->unaccel_dx, event->unaccel_dy);
}

void
moveresize(const Arg *arg)
{
	double norm_x, norm_y, d_left, d_right, d_top, d_bottom;
	const char *cursor_icon;

	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	xytonode(cursor->x, cursor->y, NULL, &grabc, NULL, NULL, NULL);
	if (!grabc || client_is_unmanaged(grabc) || grabc->isfullscreen)
		return;

	destroy_snap_overlay();
	/* Float the window and tell motionnotify to grab it */
	grabc_was_tiled = !grabc->isfloating;
	grabc_start_geom = grabc->geom;
	setfloating(grabc, 1);
	switch (cursor_mode = arg->ui) {
	case CurMove:
		grabcx = (int)round(cursor->x) - grabc->geom.x;
		grabcy = (int)round(cursor->y) - grabc->geom.y;
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "all-scroll");
		break;
	case CurResize:
		norm_x = (grabc->geom.width > 0) ? (cursor->x - grabc->geom.x) / (double)grabc->geom.width : 0.5;
		norm_y = (grabc->geom.height > 0) ? (cursor->y - grabc->geom.y) / (double)grabc->geom.height : 0.5;
		cursor_icon = "se-resize";

		grabc_edges = WLR_EDGE_NONE;
		if (norm_x < 0.35)
			grabc_edges |= WLR_EDGE_LEFT;
		else if (norm_x > 0.65)
			grabc_edges |= WLR_EDGE_RIGHT;

		if (norm_y < 0.35)
			grabc_edges |= WLR_EDGE_TOP;
		else if (norm_y > 0.65)
			grabc_edges |= WLR_EDGE_BOTTOM;

		if (grabc_edges == WLR_EDGE_NONE) {
			d_left = fabs(cursor->x - grabc->geom.x);
			d_right = fabs(cursor->x - (grabc->geom.x + grabc->geom.width));
			d_top = fabs(cursor->y - grabc->geom.y);
			d_bottom = fabs(cursor->y - (grabc->geom.y + grabc->geom.height));

			if (d_left < d_right)
				grabc_edges |= WLR_EDGE_LEFT;
			else
				grabc_edges |= WLR_EDGE_RIGHT;

			if (d_top < d_bottom)
				grabc_edges |= WLR_EDGE_TOP;
			else
				grabc_edges |= WLR_EDGE_BOTTOM;
		}

		if (grabc_edges & WLR_EDGE_LEFT)
			grabcx = (int)round(cursor->x) - grabc->geom.x;
		else if (grabc_edges & WLR_EDGE_RIGHT)
			grabcx = (int)round(cursor->x) - (grabc->geom.x + grabc->geom.width);
		else
			grabcx = 0;

		if (grabc_edges & WLR_EDGE_TOP)
			grabcy = (int)round(cursor->y) - grabc->geom.y;
		else if (grabc_edges & WLR_EDGE_BOTTOM)
			grabcy = (int)round(cursor->y) - (grabc->geom.y + grabc->geom.height);
		else
			grabcy = 0;

		switch (grabc_edges) {
		case WLR_EDGE_TOP:
			cursor_icon = "n-resize";
			break;
		case WLR_EDGE_BOTTOM:
			cursor_icon = "s-resize";
			break;
		case WLR_EDGE_LEFT:
			cursor_icon = "w-resize";
			break;
		case WLR_EDGE_RIGHT:
			cursor_icon = "e-resize";
			break;
		case WLR_EDGE_TOP | WLR_EDGE_LEFT:
			cursor_icon = "nw-resize";
			break;
		case WLR_EDGE_TOP | WLR_EDGE_RIGHT:
			cursor_icon = "ne-resize";
			break;
		case WLR_EDGE_BOTTOM | WLR_EDGE_LEFT:
			cursor_icon = "sw-resize";
			break;
		case WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT:
			cursor_icon = "se-resize";
			break;
		}

		wlr_cursor_set_xcursor(cursor, cursor_mgr, cursor_icon);
		break;
	}
}

void
setcursor(struct wl_listener *listener, void *data)
{
	/* This event is raised by the seat when a client provides a cursor image */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	/* If we're "grabbing" the cursor, don't use the client's image, we will
	 * restore it after "grabbing" sending a leave event, followed by a enter
	 * event, which will result in the client requesting set the cursor surface */
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	/* This can be sent by any client, so we check to make sure this one
	 * actually has pointer focus first. If so, we can tell the cursor to
	 * use the provided surface as the cursor image. It will set the
	 * hardware cursor on the output that it's currently on and continue to
	 * do so as the cursor moves between outputs. */
	if (event->seat_client == seat->pointer_state.focused_client)
		wlr_cursor_set_surface(cursor, event->surface,
				event->hotspot_x, event->hotspot_y);
}

void
setcursorshape(struct wl_listener *listener, void *data)
{
	struct wlr_cursor_shape_manager_v1_request_set_shape_event *event = data;
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	/* This can be sent by any client, so we check to make sure this one
	 * actually has pointer focus first. If so, we can tell the cursor to
	 * use the provided cursor shape. */
	if (event->seat_client == seat->pointer_state.focused_client)
		wlr_cursor_set_xcursor(cursor, cursor_mgr,
				wlr_cursor_shape_v1_name(event->shape));
}
