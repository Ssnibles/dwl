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

void createkeyboard(struct wlr_keyboard *keyboard);
KeyboardGroup *createkeyboardgroup(void);
void destroykeyboardgroup(struct wl_listener *listener, void *data);
int keybinding(uint32_t mods, xkb_keysym_t sym);
void keypress(struct wl_listener *listener, void *data);
void keypressmod(struct wl_listener *listener, void *data);
int keyrepeat(void *data);
void virtualkeyboard(struct wl_listener *listener, void *data);
void virtualpointer(struct wl_listener *listener, void *data);

void inputdevice(struct wl_listener *listener, void *data);
void createpointer(struct wlr_pointer *pointer);
void requeststartdrag(struct wl_listener *listener, void *data);
void startdrag(struct wl_listener *listener, void *data);
void destroydragicon(struct wl_listener *listener, void *data);
void createidleinhibitor(struct wl_listener *listener, void *data);
void destroyidleinhibitor(struct wl_listener *listener, void *data);

#endif /* INPUT_SEAT_H */
