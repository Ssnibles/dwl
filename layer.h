/*
 * DWL - Layer Shell Surface Management Header
 * Declarations for handling Wayland wlr_layer_shell_v1 surfaces (status bars, wall papers, panels).
 */

#ifndef LAYER_H
#define LAYER_H

#include "dwl.h"

/* --- Public Layer Surface Management Prototypes --- */
void createlayersurface(struct wl_listener *listener, void *data);
void destroylayersurfacenotify(struct wl_listener *listener, void *data);
void unmaplayersurfacenotify(struct wl_listener *listener, void *data);
void commitlayersurfacenotify(struct wl_listener *listener, void *data);
void arrangelayers(Monitor *m);
void arrangelayer(Monitor *m, struct wl_list *list, struct wlr_box *usable_area, int exclusive);

#endif /* LAYER_H */
