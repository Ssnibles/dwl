/*
 * DWL - Client & Window Management Module
 * Manages Wayland application window state, surface mapping, rules, focus,
 * decorations, fullscreen, floating status, and tag assignment.
 */

#include "dwl.h"

/* Restricts client geometry to fit within designated monitor/screen bounding box */
void
applybounds(Client *c, struct wlr_box *bbox)
{
	/* Adjust width and height according to min/max constraints */
	int min_w = 0, max_w = 0, min_h = 0, max_h = 0;
	if (c->isfullscreen)
		return;

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		xcb_size_hints_t *hints = c->surface.xwayland->size_hints;
		if (hints) {
			min_w = hints->min_width;
			max_w = hints->max_width;
			min_h = hints->min_height;
			max_h = hints->max_height;
		}
	} else
#endif
	{
		struct wlr_xdg_toplevel_state *state = &c->surface.xdg->toplevel->current;
		min_w = state->min_width;
		max_w = state->max_width;
		min_h = state->min_height;
		max_h = state->max_height;
	}

	if (min_w > 0 && c->geom.width < min_w)
		c->geom.width = min_w;
	if (max_w > 0 && c->geom.width > max_w)
		c->geom.width = max_w;
	if (min_h > 0 && c->geom.height < min_h)
		c->geom.height = min_h;
	if (max_h > 0 && c->geom.height > max_h)
		c->geom.height = max_h;

	/* Keep window inside monitor boundary */
	c->geom.x = MAX(bbox->x, MIN(c->geom.x, bbox->x + bbox->width - c->geom.width));
	c->geom.y = MAX(bbox->y, MIN(c->geom.y, bbox->y + bbox->height - c->geom.height));
}

/* Applies configured rules (tag placement, floating state, monitor assignment) to a new client */
void
applyrules(Client *c)
{
	const Rule *r;
	Monitor *mon = selmon, *m;
	const char *appid = client_get_appid(c);
	const char *title = client_get_title(c);
	uint32_t newtags = 0;
	int isfloating = 0;
	size_t i;

	c->isfloating = 0;
	c->tags = 0;

	for (r = rules; r < END(rules); r++) {
		if ((!r->title || strstr(title, r->title))
				&& (!r->id || strstr(appid, r->id))) {
			isfloating = r->isfloating;
			newtags |= r->tags;
			i = 0;
			wl_list_for_each(m, &mons, link) {
				if (r->monitor == (int)i++)
					mon = m;
			}
		}
	}

	isfloating |= client_is_float_type(c);
	setmon(c, mon, newtags);
	setfloating(c, isfloating);
}

/* Handles surface commit events from clients */
void
commitnotify(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, commit);

	if (c->surface.xdg->initial_commit) {
		client_set_tiled(c, WLR_EDGE_NONE);
		setfullscreen(c, client_wants_fullscreen(c));
		return;
	}

	/* Update client geometry if requested */
	if (c->geom.width != c->surface.xdg->geometry.width
			|| c->geom.height != c->surface.xdg->geometry.height) {
		resize(c, c->geom, 0);
	}
}

/* Handles decoration creation for clients */
void
createdecoration(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_toplevel_decoration_v1 *dec = data;
	Client *c = dec->toplevel->base->data;
	if (!c)
		return;

	c->decoration = dec;
	LISTEN(&dec->events.request_mode, &c->set_decoration_mode, setdecorationmode);
	LISTEN(&dec->events.destroy, &c->destroy_decoration, destroydecoration);

	setdecorationmode(&c->set_decoration_mode, dec);
}

/* Destroys client decoration object */
void
destroydecoration(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, destroy_decoration);
	c->decoration = NULL;
}

/* Sets server-side decoration mode */
void
setdecorationmode(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, set_decoration_mode);
	wlr_xdg_toplevel_decoration_v1_set_mode(c->decoration,
			WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

/* Handles creation of new XDG popups */
void
createpopup(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_popup *popup = data;
	LISTEN_STATIC(&popup->base->surface->events.commit, commitpopup);
}

/* Handles surface commit events on XDG popups */
void
commitpopup(struct wl_listener *listener, void *data)
{
	struct wlr_surface *surface = data;
	struct wlr_xdg_popup *popup = wlr_xdg_popup_try_from_wlr_surface(surface);
	LayerSurface *l = NULL;
	Client *c = NULL;
	struct wlr_box box;
	int type = -1;

	if (!popup->base->initial_commit)
		return;

	type = toplevel_from_wlr_surface(popup->base->surface, &c, &l);
	if (!popup->parent || type < 0)
		return;
	popup->base->surface->data = wlr_scene_xdg_surface_create(
			popup->parent->data, popup->base);
	if ((l && !l->mon) || (c && !c->mon)) {
		wlr_xdg_popup_destroy(popup);
		return;
	}
	box = type == LayerShell ? l->mon->m : c->mon->w;
	box.x -= (type == LayerShell ? l->scene->node.x : c->geom.x);
	box.y -= (type == LayerShell ? l->scene->node.y : c->geom.y);
	wlr_xdg_popup_unconstrain_from_box(popup, &box);
	wl_list_remove(&listener->link);
	free(listener);
}

/* Handles creation and mapping of new XDG top-level application surfaces */
void
createnotify(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_toplevel *toplevel = data;
	Client *c;

	c = toplevel->base->data = ecalloc(1, sizeof(*c));
	c->type = XDGShell;
	c->surface.xdg = toplevel->base;
	c->bw = borderpx;

	LISTEN(&toplevel->base->surface->events.commit, &c->commit, commitnotify);
	LISTEN(&toplevel->base->surface->events.map, &c->map, mapnotify);
	LISTEN(&toplevel->base->surface->events.unmap, &c->unmap, unmapnotify);
	LISTEN(&toplevel->base->events.destroy, &c->destroy, destroyclient);
	LISTEN(&toplevel->events.request_fullscreen, &c->fullscreen, fullscreennotify);
	LISTEN(&toplevel->events.request_maximize, &c->maximize, maximizenotify);
	LISTEN(&toplevel->events.set_title, &c->set_title, updatetitle);
}

/* Destroys client tracking structures upon surface destruction */
void
destroyclient(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, destroy);
	wl_list_remove(&c->commit.link);
	wl_list_remove(&c->map.link);
	wl_list_remove(&c->unmap.link);
	wl_list_remove(&c->destroy.link);
	wl_list_remove(&c->fullscreen.link);
	wl_list_remove(&c->maximize.link);
	wl_list_remove(&c->set_title.link);

	if (c->decoration) {
		wl_list_remove(&c->set_decoration_mode.link);
		wl_list_remove(&c->destroy_decoration.link);
	}

	free(c);
}

/* Focuses a client window and updates borders and input focus */
void
focusclient(Client *c, int lift)
{
	struct wlr_surface *old_surface = seat->keyboard_state.focused_surface;
	int is_same = (c && client_surface(c) == old_surface);
	Client *tmp;

	if (locked)
		return;

	if (c && !VISIBLEON(c, c->mon))
		c = NULL;

	if (c) {
		/* Move to front of focus stack */
		wl_list_remove(&c->flink);
		wl_list_insert(&fstack, &c->flink);

		if (lift) {
			wlr_scene_node_raise_to_top(&c->scene->node);
		}

		/* Deactivate previously focused surface */
		if (old_surface && old_surface != client_surface(c))
			client_activate_surface(old_surface, 0);

		/* Activate newly focused client surface */
		client_activate_surface(client_surface(c), 1);
		client_notify_enter(client_surface(c), wlr_seat_get_keyboard(seat));

		/* Update border color */
		client_set_border_color(c, focuscolor);
	} else {
		if (old_surface)
			client_activate_surface(old_surface, 0);
		wlr_seat_keyboard_clear_focus(seat);
	}

	/* Update unfocused client border colors */
	wl_list_for_each(tmp, &clients, link) {
		if (tmp != c && client_surface(tmp) != old_surface)
			client_set_border_color(tmp, tmp->isurgent ? urgentcolor : bordercolor);
	}

	if (!is_same)
		printstatus();
}

/* Returns top-most visible client on specified monitor */
Client *
focustop(Monitor *m)
{
	Client *c;
	wl_list_for_each(c, &fstack, flink) {
		if (VISIBLEON(c, m))
			return c;
	}
	return NULL;
}

/* Closes currently focused window */
void
killclient(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if (sel)
		client_send_close(sel);
}

/* Handles window map events when client becomes visible */
void
mapnotify(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, map);

	/* Create scene graph node for client */
	c->scene = wlr_scene_tree_create(layers[LyrTile]);
	c->scene->node.data = c;
	c->border = wlr_scene_rect_create(c->scene, 0, 0, bordercolor);
	c->border->node.data = c;

	c->scene_surface = wlr_scene_xdg_surface_create(c->scene, c->surface.xdg);
	c->scene_surface->node.data = c;

	wl_list_insert(&clients, &c->link);
	wl_list_insert(&fstack, &c->flink);

	applyrules(c);
	focusclient(c, 1);
	arrange(c->mon);
	printstatus();
}

/* Handles window unmap events when client surface is hidden */
void
unmapnotify(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, unmap);
	if (c == grabc) {
		cursor_mode = CurNormal;
		grabc = NULL;
	}

	if (client_is_unmanaged(c)) {
		if (c == exclusive_focus) {
			exclusive_focus = NULL;
			focusclient(focustop(selmon), 1);
		}
	} else {
		wl_list_remove(&c->link);
		setmon(c, NULL, 0);
		wl_list_remove(&c->flink);
	}

	wlr_scene_node_destroy(&c->scene->node);
	printstatus();
	motionnotify(0, NULL, 0, 0, 0, 0);
}

/* Reassigns client window to specified monitor */
void
setmon(Client *c, Monitor *m, uint32_t newtags)
{
	Monitor *oldmon = c->mon;

	if (oldmon == m)
		return;
	c->mon = m;
	c->prev = c->geom;

	if (oldmon)
		arrange(oldmon);
	if (m) {
		resize(c, c->geom, 0);
		c->tags = newtags ? newtags : m->tagset[m->seltags];
		setfullscreen(c, c->isfullscreen);
		setfloating(c, c->isfloating);
	}
	focusclient(focustop(selmon), 1);
}

/* Sets client floating state */
void
setfloating(Client *c, int floating)
{
	c->isfloating = floating;
	wlr_scene_node_reparent(&c->scene->node, layers[c->isfullscreen
			? LyrFS : c->isfloating ? LyrFloat : LyrTile]);
	arrange(c->mon);
	printstatus();
}

/* Sets client fullscreen state */
void
setfullscreen(Client *c, int fullscreen)
{
	c->isfullscreen = fullscreen;
	if (!c->mon || !client_surface(c)->mapped)
		return;
	c->bw = fullscreen ? 0 : borderpx;
	client_set_fullscreen(c, fullscreen);
	wlr_scene_node_reparent(&c->scene->node, layers[c->isfullscreen
			? LyrFS : c->isfloating ? LyrFloat : LyrTile]);

	if (fullscreen) {
		c->prev = c->geom;
		resize(c, c->mon->m, 0);
	} else {
		resize(c, c->prev, 0);
	}
	arrange(c->mon);
	printstatus();
}

/* Handles client fullscreen request events */
void
fullscreennotify(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, fullscreen);
	setfullscreen(c, client_wants_fullscreen(c));
}

/* Handles client maximize requests (converted to fullscreen or floating) */
void
maximizenotify(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, maximize);
	if (c->surface.xdg->toplevel->requested.maximized)
		setfullscreen(c, 1);
}

/* Moves current client to specified tag */
void
tag(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if (!sel || (arg->ui & TAGMASK) == 0)
		return;

	sel->tags = arg->ui & TAGMASK;
	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}

/* Toggles specified tag for focused client */
void
toggletag(const Arg *arg)
{
	uint32_t newtags;
	Client *sel = focustop(selmon);
	if (!sel || !(newtags = sel->tags ^ (arg->ui & TAGMASK)))
		return;

	sel->tags = newtags;
	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}

/* Switches visible tags on current monitor */
void
view(const Arg *arg)
{
	if (!selmon || (arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1;
	if (arg->ui & TAGMASK)
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}

/* Toggles visibility of specified tag on current monitor */
void
toggleview(const Arg *arg)
{
	uint32_t newtagset;
	if (!(newtagset = selmon ? selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK) : 0))
		return;

	selmon->tagset[selmon->seltags] = newtagset;
	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}

/* Sets window urgency state */
void
urgent(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_activation_v1_request_activate_event *event = data;
	Client *c = NULL;
	toplevel_from_wlr_surface(event->surface, &c, NULL);
	if (!c || c == focustop(selmon))
		return;

	c->isurgent = 1;
	printstatus();

	if (client_surface(c)->mapped)
		client_set_border_color(c, urgentcolor);
}

/* Updates status bar when client window title changes */
void
updatetitle(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, set_title);
	if (c == focustop(c->mon))
		printstatus();
}
