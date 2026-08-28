/*
 * See LICENSE file for copyright and license details.
 */
#include <string.h>

#include "dwl.h"
#include "rules.h"
#include "layout.h"
#include "client.h"
#include "config.h"

void
applyrules(Client *c)
{
	/* rule matching */
	const char *appid = client_get_appid(c);
	const char *title = client_get_title(c);
	Monitor *mon = selmon;
	int isfloating = 0;

	for (const Rule *r = rules; r < END(rules); r++) {
		if ((!r->title || strstr(title, r->title))
				&& (!r->id || strstr(appid, r->id))) {
			isfloating = r->isfloating;
			int i = 0;
			Monitor *m;
			wl_list_for_each(m, &mons, link) {
				if (r->monitor == i++)
					mon = m;
			}
		}
	}

	isfloating |= client_is_float_type(c);
	setmon(c, mon);
	setfloating(c, isfloating);
}
