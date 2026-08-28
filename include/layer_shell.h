/*
 * See LICENSE file for copyright and license details.
 */
#ifndef DESKTOP_LAYER_SHELL_H
#define DESKTOP_LAYER_SHELL_H

#include <wayland-server-core.h>
#include "dwl.h"

void createlayersurface(struct wl_listener *listener, void *data);
void arrangelayer(Monitor *m, struct wl_list *list, struct wlr_box *usable_area, int exclusive);
void arrangelayers(Monitor *m);
void commitlayersurfacenotify(struct wl_listener *listener, void *data);
void unmaplayersurfacenotify(struct wl_listener *listener, void *data);
void destroylayersurfacenotify(struct wl_listener *listener, void *data);

#endif /* DESKTOP_LAYER_SHELL_H */
