/*
 * DWL - Monitor & Display Management Header
 * Functions for managing physical/virtual outputs, display layout, and monitor state.
 */

#ifndef MONITOR_H
#define MONITOR_H

#include "dwl.h"

/* --- Public Monitor Management Prototypes --- */
void createmon(struct wl_listener *listener, void *data);
void cleanupmon(struct wl_listener *listener, void *data);
void closemon(Monitor *m);
void updatemons(struct wl_listener *listener, void *data);
Monitor *dirtomon(enum wlr_direction dir);
Monitor *xytomon(double x, double y);
void focusmon(const Arg *arg);
void tagmon(const Arg *arg);
void rendermon(struct wl_listener *listener, void *data);
void requestmonstate(struct wl_listener *listener, void *data);
void outputmgrapply(struct wl_listener *listener, void *data);
void outputmgrapplyortest(struct wlr_output_configuration_v1 *config, int test);
void outputmgrtest(struct wl_listener *listener, void *data);
void powermgrsetmode(struct wl_listener *listener, void *data);

#endif /* MONITOR_H */
