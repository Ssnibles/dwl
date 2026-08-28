/*
 * See LICENSE file for copyright and license details.
 */
#ifndef INPUT_SEAT_H
#define INPUT_SEAT_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <xkbcommon/xkbcommon.h>

#include "dwl.h"

KeyboardGroup *createkeyboardgroup(void);
void destroykeyboardgroup(struct wl_listener *listener, void *data);
void virtualkeyboard(struct wl_listener *listener, void *data);
void virtualpointer(struct wl_listener *listener, void *data);

void inputdevice(struct wl_listener *listener, void *data);
void requeststartdrag(struct wl_listener *listener, void *data);
void startdrag(struct wl_listener *listener, void *data);
void createidleinhibitor(struct wl_listener *listener, void *data);

#endif /* INPUT_SEAT_H */
