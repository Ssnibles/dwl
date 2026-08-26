/*
 * See LICENSE file for copyright and license details.
 */
#ifndef LAYOUT_H
#define LAYOUT_H

#include "dwl.h"

void arrange(Monitor *m);
void tree_layout(Monitor *m);
void bsp_layout(Monitor *m);
void dwindle(Monitor *m);
void fibonacci(Monitor *m, int s);
void master_stack(Monitor *m);
void columns(Monitor *m);
void tile(Monitor *m);
void monocle(Monitor *m);
void spiral(Monitor *m);
void overview(Monitor *m);
void focusdir(const Arg *arg);
void focusstack(const Arg *arg);
void incnmaster(const Arg *arg);
void setlayout(const Arg *arg);
void setmfact(const Arg *arg);
void clearlabeloverlays(Monitor *m);
void destroylabeloverlay(Client *c);
void toggleoverview(const Arg *arg);
void updatelabeloverlays(Monitor *m);

#endif /* LAYOUT_H */
