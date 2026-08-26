/*
 * DWL - Layout Engine Header
 * Tiling algorithm declarations, Overview Mode interface, and window geometry calculation helpers.
 */

#ifndef LAYOUT_H
#define LAYOUT_H

#include "dwl.h"

/* --- Public Layout Engine Prototypes --- */
void arrange(Monitor *m);
void resize(Client *c, struct wlr_box geo, int interact);
void tile(Monitor *m);
void monocle(Monitor *m);
void dwindle(Monitor *m);
void spiral(Monitor *m);
void fibonacci(Monitor *m, int s);
void overview(Monitor *m);
void toggleoverview(const Arg *arg);
void setmfact(const Arg *arg);
void incnmaster(const Arg *arg);
void setlayout(const Arg *arg);
void zoom(const Arg *arg);
void togglefloating(const Arg *arg);
void togglefullscreen(const Arg *arg);
void focusstack(const Arg *arg);

#endif /* LAYOUT_H */
