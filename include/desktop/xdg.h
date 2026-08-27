/*
 * See LICENSE file for copyright and license details.
 */
#ifndef DESKTOP_XDG_H
#define DESKTOP_XDG_H

#include <wayland-server-core.h>

void createnotify(struct wl_listener *listener, void *data);
void createpopup(struct wl_listener *listener, void *data);
void commitpopup(struct wl_listener *listener, void *data);
void createdecoration(struct wl_listener *listener, void *data);
void destroydecoration(struct wl_listener *listener, void *data);
void requestdecorationmode(struct wl_listener *listener, void *data);
void fullscreennotify(struct wl_listener *listener, void *data);
void maximizenotify(struct wl_listener *listener, void *data);

void commitnotify(struct wl_listener *listener, void *data);
void mapnotify(struct wl_listener *listener, void *data);
void unmapnotify(struct wl_listener *listener, void *data);
void destroynotify(struct wl_listener *listener, void *data);
void updatetitle(struct wl_listener *listener, void *data);

#endif /* DESKTOP_XDG_H */
