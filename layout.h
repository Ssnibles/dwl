/*
 * See LICENSE file for copyright and license details.
 */
#ifndef LAYOUT_H
#define LAYOUT_H

#include "dwl.h"

void arrange(Monitor *m);
void dwindle(Monitor *m);
void fibonacci(Monitor *m, int s);
void incnmaster(const Arg *arg);
void monocle(Monitor *m);
void setlayout(const Arg *arg);
void setmfact(const Arg *arg);
void spiral(Monitor *m);
void tile(Monitor *m);

#endif /* LAYOUT_H */
