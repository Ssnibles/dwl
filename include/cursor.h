/*
 * See LICENSE file for copyright and license details.
 */
#ifndef INPUT_CURSOR_H
#define INPUT_CURSOR_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>

#include "dwl.h"

extern int in_pointer_focus;

void axisnotify(struct wl_listener *listener, void *data);
void buttonpress(struct wl_listener *listener, void *data);
void cursorconstrain(struct wlr_pointer_constraint_v1 *constraint);
void cursorframe(struct wl_listener *listener, void *data);
void cursorwarptohint(void);
void motionabsolute(struct wl_listener *listener, void *data);
void motionnotify(uint32_t time, struct wlr_input_device *device, double dx, double dy, double dx_unaccel, double dy_unaccel);
void motionrelative(struct wl_listener *listener, void *data);
void moveresize(const Arg *arg);
void setcursor(struct wl_listener *listener, void *data);
void setcursorshape(struct wl_listener *listener, void *data);
void destroy_snap_overlay(void);
void warptocenter(Client *c);

void createpointerconstraint(struct wl_listener *listener, void *data);
void destroypointerconstraint(struct wl_listener *listener, void *data);

#endif /* INPUT_CURSOR_H */
