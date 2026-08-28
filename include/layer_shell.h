/*
 * See LICENSE file for copyright and license details.
 */
#ifndef DESKTOP_LAYER_SHELL_H
#define DESKTOP_LAYER_SHELL_H

#include <wayland-server-core.h>
#include "dwl.h"

void createlayersurface(struct wl_listener *listener, void *data);
void arrangelayers(Monitor *m);

#endif /* DESKTOP_LAYER_SHELL_H */
