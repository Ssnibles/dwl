/*
 * DWL - Input Handling Header
 * Functions for processing seat input, pointer events, keyboard groups, keybindings,
 * cursor warping, and drag-and-drop operations.
 */

#ifndef INPUT_H
#define INPUT_H

#include "dwl.h"

/* --- Public Input Handling Prototypes --- */
void axisnotify(struct wl_listener *listener, void *data);
void buttonpress(struct wl_listener *listener, void *data);
void createkeyboard(struct wlr_keyboard *keyboard);
KeyboardGroup *createkeyboardgroup(void);
void destroykeyboardgroup(struct wl_listener *listener, void *data);
void createpointer(struct wlr_pointer *pointer);
void createpointerconstraint(struct wl_listener *listener, void *data);
void cursorconstrain(struct wlr_pointer_constraint_v1 *constraint);
void cursorframe(struct wl_listener *listener, void *data);
void cursorwarptohint(void);
void destroydragicon(struct wl_listener *listener, void *data);
void destroypointerconstraint(struct wl_listener *listener, void *data);
void inputdevice(struct wl_listener *listener, void *data);
int keybinding(uint32_t mods, xkb_keysym_t sym);
void keypress(struct wl_listener *listener, void *data);
void keypressmod(struct wl_listener *listener, void *data);
int keyrepeat(void *data);
void motionabsolute(struct wl_listener *listener, void *data);
void motionnotify(uint32_t time, struct wlr_input_device *device, double sx,
		double sy, double sx_unaccel, double sy_unaccel);
void motionrelative(struct wl_listener *listener, void *data);
void moveresize(const Arg *arg);
void pointerfocus(Client *c, struct wlr_surface *surface,
		double sx, double sy, uint32_t time);
void requeststartdrag(struct wl_listener *listener, void *data);
void setcursor(struct wl_listener *listener, void *data);
void setcursorshape(struct wl_listener *listener, void *data);
void startdrag(struct wl_listener *listener, void *data);
void virtualkeyboard(struct wl_listener *listener, void *data);
void virtualpointer(struct wl_listener *listener, void *data);
void xytonode(double x, double y, struct wlr_surface **psurface,
		Client **pc, LayerSurface **pl, double *nx, double *ny);

#endif /* INPUT_H */
