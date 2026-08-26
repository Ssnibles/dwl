/*
 * DWL - Session Lock & Idle Management Module
 * Manages screen locking (via ext-session-lock-v1 protocol), lock surfaces,
 * and idle inhibition states.
 */

#include "dwl.h"

/* Processes request to lock screen session */
void
locksession(struct wl_listener *listener, void *data)
{
	struct wlr_session_lock_v1 *wlr_lock = data;
	SessionLock *lock;

	if (cur_lock) {
		wlr_session_lock_v1_destroy(wlr_lock);
		return;
	}

	lock = ecalloc(1, sizeof(*lock));
	cur_lock = wlr_lock;
	lock->lock = wlr_lock;

	lock->scene = wlr_scene_tree_create(layers[LyrBlock]);
	wlr_scene_node_set_enabled(&lock->scene->node, 1);

	LISTEN(&wlr_lock->events.new_surface, &lock->new_surface, createlocksurface);
	LISTEN(&wlr_lock->events.unlock, &lock->unlock, unlocksession);
	LISTEN(&wlr_lock->events.destroy, &lock->destroy, destroysessionlock);

	wlr_session_lock_v1_send_locked(wlr_lock);
}

/* Unlocks current session and hides lock surfaces */
void
unlocksession(struct wl_listener *listener, void *data)
{
	SessionLock *lock = wl_container_of(listener, lock, unlock);
	destroylock(lock, 1);
}

/* Destroys session lock handler when client destroys lock protocol object */
void
destroysessionlock(struct wl_listener *listener, void *data)
{
	SessionLock *lock = wl_container_of(listener, lock, destroy);
	destroylock(lock, 0);
}

/* Cleans up session lock state */
void
destroylock(SessionLock *lock, int unlock)
{
	wlr_seat_keyboard_clear_focus(seat);
	if ((locked = !unlock))
		goto destroy;

	wlr_scene_node_set_enabled(&locked_bg->node, 0);

	focusclient(focustop(selmon), 0);
	motionnotify(0, NULL, 0, 0, 0, 0);

destroy:
	wl_list_remove(&lock->new_surface.link);
	wl_list_remove(&lock->unlock.link);
	wl_list_remove(&lock->destroy.link);

	wlr_scene_node_destroy(&lock->scene->node);
	cur_lock = NULL;
	free(lock);
}

/* Handles creation of session lock surface on monitor */
void
createlocksurface(struct wl_listener *listener, void *data)
{
	struct wlr_session_lock_surface_v1 *lock_surface = data;
	Monitor *m = lock_surface->output->data;
	SessionLock *lock = wl_container_of(listener, lock, new_surface);

	m->lock_surface = lock_surface;
	wlr_scene_subsurface_tree_create(lock->scene, lock_surface->surface);

	LISTEN(&lock_surface->events.destroy, &m->destroy_lock_surface, destroylocksurface);

	wlr_session_lock_surface_v1_configure(lock_surface, m->m.width, m->m.height);
}

/* Destroys session lock surface */
void
destroylocksurface(struct wl_listener *listener, void *data)
{
	Monitor *m = wl_container_of(listener, m, destroy_lock_surface);
	m->lock_surface = NULL;
	wl_list_remove(&m->destroy_lock_surface.link);
}

/* Handles new idle inhibitor creation */
void
createidleinhibitor(struct wl_listener *listener, void *data)
{
	struct wlr_idle_inhibitor_v1 *idle_inhibitor = data;
	LISTEN_STATIC(&idle_inhibitor->events.destroy, destroyidleinhibitor);
	checkidleinhibitor(NULL);
}

/* Destroys idle inhibitor */
void
destroyidleinhibitor(struct wl_listener *listener, void *data)
{
	wl_list_remove(&listener->link);
	free(listener);
	checkidleinhibitor(NULL);
}

/* Checks active idle inhibitors and enables/disables idle tracking */
void
checkidleinhibitor(struct wlr_surface *exclude)
{
	int inhibited = 0;
	struct wlr_idle_inhibitor_v1 *inhibitor;

	wl_list_for_each(inhibitor, &idle_inhibit_mgr->inhibitors, link) {
		struct wlr_surface *surface = wlr_surface_get_root_surface(inhibitor->surface);
		Client *c = NULL;

		if (surface == exclude)
			continue;

		toplevel_from_wlr_surface(surface, &c, NULL);

		if (bypass_surface_visibility || (c && VISIBLEON(c, c->mon))) {
			inhibited = 1;
			break;
		}
	}

	wlr_idle_notifier_v1_set_inhibited(idle_notifier, inhibited);
}
