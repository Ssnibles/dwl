/*
 * See LICENSE file for copyright and license details.
 */
#ifndef OUTPUT_OUTPUT_H
#define OUTPUT_OUTPUT_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_layout.h>

#include "dwl.h"

void createmon(struct wl_listener *listener, void *data);
void cleanupmon(struct wl_listener *listener, void *data);
void closemon(Monitor *m);
void updatemons(struct wl_listener *listener, void *data);
void rendermon(struct wl_listener *listener, void *data);
void outputmgrapply(struct wl_listener *listener, void *data);
void outputmgrapplyortest(struct wlr_output_configuration_v1 *config, int test);
void outputmgrtest(struct wl_listener *listener, void *data);
void powermgrsetmode(struct wl_listener *listener, void *data);
void requestmonstate(struct wl_listener *listener, void *data);
Monitor *dirtomon(enum wlr_direction dir);

#endif /* OUTPUT_OUTPUT_H */
