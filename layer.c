/*
 * DWL - Layer Shell Surface Management Module
 * Manages desktop environment surfaces (bars, desktop popups, lock screens),
 * surface positioning, and available monitor usable area calculations.
 */

#include "dwl.h"

/* Handles creation of new layer shell surface (e.g. status bar, wallpaper, panel) */
void
createlayersurface(struct wl_listener *listener, void *data)
{
	struct wlr_layer_surface_v1 *wlr_layer_surface = data;
	LayerSurface *layersurface;
	struct wlr_scene_tree *popups;

	if (!wlr_layer_surface->output)
		wlr_layer_surface->output = selmon ? selmon->wlr_output : NULL;

	if (!wlr_layer_surface->output) {
		wlr_layer_surface_v1_destroy(wlr_layer_surface);
		return;
	}

	layersurface = ecalloc(1, sizeof(*layersurface));
	layersurface->type = LayerShell;
	layersurface->layer_surface = wlr_layer_surface;
	layersurface->mon = wlr_layer_surface->output->data;

	wlr_layer_surface->data = layersurface;

	layersurface->scene_layer = wlr_scene_layer_surface_v1_create(
			layers[layermap[wlr_layer_surface->pending.layer]], wlr_layer_surface);
	layersurface->scene = layersurface->scene_layer->tree;
	layersurface->scene->node.data = layersurface;

	popups = wlr_scene_tree_create(layers[LyrTop]);
	layersurface->popups = popups;

	LISTEN(&wlr_layer_surface->surface->events.commit, &layersurface->surface_commit, commitlayersurfacenotify);
	LISTEN(&wlr_layer_surface->surface->events.unmap, &layersurface->unmap, unmaplayersurfacenotify);
	LISTEN(&wlr_layer_surface->events.destroy, &layersurface->destroy, destroylayersurfacenotify);

	wl_list_insert(&layersurface->mon->layers[wlr_layer_surface->pending.layer], &layersurface->link);

	arrangelayers(layersurface->mon);
}

/* Destroys layer shell surface object */
void
destroylayersurfacenotify(struct wl_listener *listener, void *data)
{
	LayerSurface *layersurface = wl_container_of(listener, layersurface, destroy);

	wl_list_remove(&layersurface->surface_commit.link);
	wl_list_remove(&layersurface->unmap.link);
	wl_list_remove(&layersurface->destroy.link);

	if (layersurface->link.next)
		wl_list_remove(&layersurface->link);

	wlr_scene_node_destroy(&layersurface->popups->node);
	free(layersurface);
}

/* Unmaps layer shell surface when hidden */
void
unmaplayersurfacenotify(struct wl_listener *listener, void *data)
{
	LayerSurface *layersurface = wl_container_of(listener, layersurface, unmap);

	layersurface->mapped = 0;

	if (exclusive_focus == layersurface)
		exclusive_focus = NULL;

	arrangelayers(layersurface->mon);
	focusclient(focustop(selmon), 1);
}

/* Handles commit events on layer shell surface */
void
commitlayersurfacenotify(struct wl_listener *listener, void *data)
{
	LayerSurface *layersurface = wl_container_of(listener, layersurface, surface_commit);
	struct wlr_layer_surface_v1 *wlr_layer_surface = layersurface->layer_surface;

	if (wlr_layer_surface->initial_commit)
		wlr_fractional_scale_v1_notify_scale(wlr_layer_surface->surface,
				layersurface->mon->wlr_output->scale);

	if (wlr_layer_surface->current.layer != wlr_layer_surface->pending.layer) {
		wl_list_remove(&layersurface->link);
		wl_list_insert(&layersurface->mon->layers[wlr_layer_surface->pending.layer],
				&layersurface->link);
		wlr_scene_node_reparent(&layersurface->scene->node,
				layers[layermap[wlr_layer_surface->pending.layer]]);
	}

	arrangelayers(layersurface->mon);
}

/* Positions layer surfaces for a monitor and updates usable screen area */
void
arrangelayers(Monitor *m)
{
	int i;
	struct wlr_box usable_area = m->m;

	if (!m->wlr_output->enabled)
		return;

	/* Arrange exclusive layer surfaces first, shrinking usable_area */
	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 1);

	m->w = usable_area;

	/* Arrange non-exclusive surfaces */
	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 0);

	arrange(m);
}

/* Helper function to position individual layer surfaces within usable_area */
void
arrangelayer(Monitor *m, struct wl_list *list, struct wlr_box *usable_area, int exclusive)
{
	LayerSurface *layersurface;

	wl_list_for_each(layersurface, list, link) {
		struct wlr_layer_surface_v1 *wlr_layer_surface = layersurface->layer_surface;

		if (exclusive != (wlr_layer_surface->current.exclusive_zone > 0))
			continue;

		wlr_scene_layer_surface_v1_configure(layersurface->scene_layer,
				usable_area, usable_area);
	}
}
