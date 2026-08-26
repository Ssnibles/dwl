/*
 * DWL - Session Lock & Idle Management Header
 * Declarations for Wayland session lock protocol (ext-session-lock-v1) and idle inhibition tracking.
 */

#ifndef SESSION_H
#define SESSION_H

#include "dwl.h"

/* --- Public Session & Idle Management Prototypes --- */
void locksession(struct wl_listener *listener, void *data);
void unlocksession(struct wl_listener *listener, void *data);
void destroysessionlock(struct wl_listener *listener, void *data);
void destroylock(SessionLock *lock, int unlock);
void destroylocksurface(struct wl_listener *listener, void *data);
void createlocksurface(struct wl_listener *listener, void *data);
void createidleinhibitor(struct wl_listener *listener, void *data);
void destroyidleinhibitor(struct wl_listener *listener, void *data);
void checkidleinhibitor(struct wlr_surface *exclude);

#endif /* SESSION_H */
