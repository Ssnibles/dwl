/*
 * See LICENSE file for copyright and license details.
 */
#ifndef LAYOUT_H
#define LAYOUT_H

#include "dwl.h"

void arrange(Monitor *m);
void dwindle(Monitor *m);
void fibonacci(Monitor *m, int s);
void focusdir(const Arg *arg);
void focusstack(const Arg *arg);
void incnmaster(const Arg *arg);
void monocle(Monitor *m);
void overview(Monitor *m);
void setlayout(const Arg *arg);
void setmfact(const Arg *arg);
void spiral(Monitor *m);
void tile(Monitor *m);
void clearlabeloverlays(Monitor *m);
void destroylabeloverlay(Client *c);
void toggleoverview(const Arg *arg);
void updatelabeloverlays(Monitor *m);

#endif /* LAYOUT_H */
