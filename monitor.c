/*
 * DWL - Monitor & Display Management Module
 * Handles creation, destruction, output mode configuration, screen area math,
 * and multi-monitor output tracking.
 */

#include "dwl.h"

/* Initializes and configures a newly connected physical or virtual output monitor */
void
createmon(struct wl_listener *listener, void *data)
{
	struct wlr_output *wlr_output = data;
	const MonitorRule *r;
	size_t i;
	struct wlr_output_state state;
	Monitor *m;

	if (!wlr_output_init_render(wlr_output, alloc, drw))
		return;

	m = wlr_output->data = ecalloc(1, sizeof(*m));
	m->wlr_output = wlr_output;

	for (i = 0; i < LENGTH(m->layers); i++)
		wl_list_init(&m->layers[i]);

	wlr_output_state_init(&state);
	/* Initialize monitor state using configured rules */
	m->tagset[0] = m->tagset[1] = 1;
	for (r = monrules; r < END(monrules); r++) {
		if (!r->name || strstr(wlr_output->name, r->name)) {
			m->m.x = r->x;
			m->m.y = r->y;
			m->mfact = r->mfact;
			m->nmaster = r->nmaster;
			m->lt[0] = r->lt;
			m->lt[1] = &layouts[LENGTH(layouts) > 1 && r->lt != &layouts[1]];
			strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, LENGTH(m->ltsymbol));
			wlr_output_state_set_scale(&state, r->scale);
			wlr_output_state_set_transform(&state, r->rr);
			break;
		}
	}

	wlr_output_state_set_mode(&state, wlr_output_preferred_mode(wlr_output));

	/* Set up event listeners */
	LISTEN(&wlr_output->events.frame, &m->frame, rendermon);
	LISTEN(&wlr_output->events.destroy, &m->destroy, cleanupmon);
	LISTEN(&wlr_output->events.request_state, &m->request_state, requestmonstate);

	wlr_output_state_set_enabled(&state, 1);
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	wl_list_insert(&mons, &m->link);
	printstatus();

	m->fullscreen_bg = wlr_scene_rect_create(layers[LyrFS], 0, 0, fullscreen_bg);
	wlr_scene_node_set_enabled(&m->fullscreen_bg->node, 0);

	m->scene_output = wlr_scene_output_create(scene, wlr_output);
	if (m->m.x == -1 && m->m.y == -1)
		wlr_output_layout_add_auto(output_layout, wlr_output);
	else
		wlr_output_layout_add(output_layout, wlr_output, m->m.x, m->m.y);
}

/* Cleans up monitor resources upon disconnect */
void
cleanupmon(struct wl_listener *listener, void *data)
{
	Monitor *m = wl_container_of(listener, m, destroy);
	LayerSurface *l, *tmp;
	size_t i;

	/* Reassign remaining windows to another monitor */
	closemon(m);
	wl_list_remove(&m->destroy.link);
	wl_list_remove(&m->frame.link);
	wl_list_remove(&m->request_state.link);

	for (i = 0; i < LENGTH(m->layers); i++) {
		wl_list_for_each_safe(l, tmp, &m->layers[i], link)
			wlr_layer_surface_v1_destroy(l->layer_surface);
	}

	wl_list_remove(&m->link);
	wlr_scene_node_destroy(&m->fullscreen_bg->node);
	free(m);

	printstatus();
}

/* Reassigns clients from a monitor being closed */
void
closemon(Monitor *m)
{
	Client *c;
	Monitor *target = NULL, *tmp;

	wl_list_for_each(tmp, &mons, link) {
		if (tmp != m) {
			target = tmp;
			break;
		}
	}

	wl_list_for_each(c, &clients, link) {
		if (c->mon == m)
			setmon(c, target, 0);
	}

	if (selmon == m)
		selmon = target;
}

/* Returns neighboring monitor in requested direction */
Monitor *
dirtomon(enum wlr_direction dir)
{
	struct wlr_output *next;
	if (!selmon)
		return NULL;
	next = wlr_output_layout_adjacent_output(output_layout, dir, selmon->wlr_output,
			selmon->m.x, selmon->m.y);

	return next ? next->data : selmon;
}

/* Finds monitor containing coordinate point (x, y) */
Monitor *
xytomon(double x, double y)
{
	struct wlr_output *o = wlr_output_layout_output_at(output_layout, x, y);
	return o ? o->data : selmon;
}

/* Switches focus to target monitor specified by directional argument */
void
focusmon(const Arg *arg)
{
	Monitor *m = dirtomon(arg->i);
	if (m && m != selmon) {
		selmon = m;
		focusclient(focustop(selmon), 1);
	}
}

/* Sends focused window to adjacent monitor */
void
tagmon(const Arg *arg)
{
	Client *sel = focustop(selmon);
	Monitor *m = dirtomon(arg->i);

	if (sel && m && m != selmon)
		setmon(sel, m, 0);
}

/* Render loop frame callback for output monitor */
void
rendermon(struct wl_listener *listener, void *data)
{
	Monitor *m = wl_container_of(listener, m, frame);
	wlr_scene_output_commit(m->scene_output, NULL);
}

/* Handles output mode and state requests */
void
requestmonstate(struct wl_listener *listener, void *data)
{
	Monitor *m = wl_container_of(listener, m, request_state);
	struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(m->wlr_output, event->state);
}

/* Applies output management configuration changes */
void
outputmgrapply(struct wl_listener *listener, void *data)
{
	struct wlr_output_configuration_v1 *config = data;
	outputmgrapplyortest(config, 0);
}

/* Helper function to test or apply output manager config */
void
outputmgrapplyortest(struct wlr_output_configuration_v1 *config, int test)
{
	struct wlr_output_configuration_head_v1 *config_head;
	int ok = 1;

	wl_list_for_each(config_head, &config->heads, link) {
		struct wlr_output *wlr_output = config_head->state.output;
		Monitor *m = wlr_output->data;
		struct wlr_output_state state;

		m->asleep = 0;

		wlr_output_state_init(&state);
		wlr_output_state_set_enabled(&state, config_head->state.enabled);
		if (!config_head->state.enabled)
			goto apply_or_test;

		if (config_head->state.mode)
			wlr_output_state_set_mode(&state, config_head->state.mode);
		else
			wlr_output_state_set_custom_mode(&state,
					config_head->state.custom_mode.width,
					config_head->state.custom_mode.height,
					config_head->state.custom_mode.refresh);

		wlr_output_state_set_transform(&state, config_head->state.transform);
		wlr_output_state_set_scale(&state, config_head->state.scale);
		wlr_output_state_set_adaptive_sync_enabled(&state,
				config_head->state.adaptive_sync_enabled);

apply_or_test:
		ok &= test ? wlr_output_test_state(wlr_output, &state)
				: wlr_output_commit_state(wlr_output, &state);

		wlr_output_state_finish(&state);
	}

	if (ok)
		wlr_output_configuration_v1_send_succeeded(config);
	else
		wlr_output_configuration_v1_send_failed(config);
}

/* Tests output manager configuration validity */
void
outputmgrtest(struct wl_listener *listener, void *data)
{
	struct wlr_output_configuration_v1 *config = data;
	outputmgrapplyortest(config, 1);
}

/* Controls DPMS screen power save mode */
void
powermgrsetmode(struct wl_listener *listener, void *data)
{
	struct wlr_output_power_v1_set_mode_event *event = data;
	struct wlr_output_state state = {0};
	Monitor *m = event->output->data;

	if (!m)
		return;

	m->gamma_lut_changed = 1;
	wlr_output_state_set_enabled(&state, event->mode);
	wlr_output_commit_state(m->wlr_output, &state);

	m->asleep = !event->mode;
	updatemons(NULL, NULL);
}

/* Recalculates monitor bounds and output layout */
void
updatemons(struct wl_listener *listener, void *data)
{
	Monitor *m;

	wl_list_for_each(m, &mons, link) {
		struct wlr_box box;
		wlr_output_layout_get_box(output_layout, m->wlr_output, &box);
		m->m = box;
		m->w = box;
		arrangelayers(m);
		arrange(m);
	}
}
