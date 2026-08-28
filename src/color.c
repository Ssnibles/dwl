/*
 * See LICENSE file for copyright and license details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "color.h"

/* Default Tokyo Night color palette RGBA floats */
float rootcolor[4]          = { 0.10196f, 0.10588f, 0.14902f, 1.00000f }; /* #1a1b26ff */
float bordercolor[4]        = { 0.14118f, 0.15686f, 0.23137f, 1.00000f }; /* #24283bff */
float focuscolor[4]         = { 0.47843f, 0.63529f, 0.96863f, 1.00000f }; /* #7aa2f7ff */
float urgentcolor[4]        = { 0.96863f, 0.46275f, 0.55686f, 1.00000f }; /* #f7768eff */
float fullscreen_bg[4]      = { 0.00000f, 0.00000f, 0.00000f, 1.00000f }; /* #000000ff */
float scratchpad_bg[4]      = { 0.05882f, 0.05882f, 0.07843f, 0.70196f }; /* #0f0f14b3 */

float snap_border_center[4] = { 0.85098f, 0.20000f, 0.20000f, 0.74902f }; /* #d93333bf */
float snap_bg_center[4]     = { 0.40000f, 0.05098f, 0.05098f, 0.50196f }; /* #660d0d80 */
float snap_border_edge[4]   = { 0.74902f, 0.14902f, 0.14902f, 0.70196f }; /* #bf2626b3 */
float snap_bg_edge[4]       = { 0.50196f, 0.05098f, 0.05098f, 0.50196f }; /* #800d0d80 */

static void
copy_color(float dest[4], const float src[4])
{
	dest[0] = src[0];
	dest[1] = src[1];
	dest[2] = src[2];
	dest[3] = src[3];
}

static void
init_default_colors(void)
{
	static const float d_rootcolor[4]          = { 0.10196f, 0.10588f, 0.14902f, 1.00000f };
	static const float d_bordercolor[4]        = { 0.14118f, 0.15686f, 0.23137f, 1.00000f };
	static const float d_focuscolor[4]         = { 0.47843f, 0.63529f, 0.96863f, 1.00000f };
	static const float d_urgentcolor[4]        = { 0.96863f, 0.46275f, 0.55686f, 1.00000f };
	static const float d_fullscreen_bg[4]      = { 0.00000f, 0.00000f, 0.00000f, 1.00000f };
	static const float d_scratchpad_bg[4]      = { 0.05882f, 0.05882f, 0.07843f, 0.70196f };
	static const float d_snap_border_center[4] = { 0.85098f, 0.20000f, 0.20000f, 0.74902f };
	static const float d_snap_bg_center[4]     = { 0.40000f, 0.05098f, 0.05098f, 0.50196f };
	static const float d_snap_border_edge[4]   = { 0.74902f, 0.14902f, 0.14902f, 0.70196f };
	static const float d_snap_bg_edge[4]       = { 0.50196f, 0.05098f, 0.05098f, 0.50196f };

	copy_color(rootcolor, d_rootcolor);
	copy_color(bordercolor, d_bordercolor);
	copy_color(focuscolor, d_focuscolor);
	copy_color(urgentcolor, d_urgentcolor);
	copy_color(fullscreen_bg, d_fullscreen_bg);
	copy_color(scratchpad_bg, d_scratchpad_bg);
	copy_color(snap_border_center, d_snap_border_center);
	copy_color(snap_bg_center, d_snap_bg_center);
	copy_color(snap_border_edge, d_snap_border_edge);
	copy_color(snap_bg_edge, d_snap_bg_edge);
}

static int
parse_hex_color(const char *hex_str, float out[4])
{
	if (!hex_str || !out)
		return 0;

	while (isspace((unsigned char)*hex_str))
		hex_str++;
	if (*hex_str == '#')
		hex_str++;
	else if (hex_str[0] == '0' && (hex_str[1] == 'x' || hex_str[1] == 'X'))
		hex_str += 2;

	size_t len = strlen(hex_str);
	while (len > 0 && isspace((unsigned char)hex_str[len - 1]))
		len--;

	unsigned int r = 0, g = 0, b = 0, a = 255;
	if (len == 6) {
		if (sscanf(hex_str, "%02x%02x%02x", &r, &g, &b) != 3)
			return 0;
	} else if (len == 8) {
		if (sscanf(hex_str, "%02x%02x%02x%02x", &r, &g, &b, &a) != 4)
			return 0;
	} else {
		return 0;
	}

	out[0] = (float)r / 255.0f;
	out[1] = (float)g / 255.0f;
	out[2] = (float)b / 255.0f;
	out[3] = (float)a / 255.0f;
	return 1;
}

static void
set_color_key(const char *key, const char *val)
{
	float parsed[4];
	if (!parse_hex_color(val, parsed))
		return;

	if (strcmp(key, "rootcolor") == 0)
		copy_color(rootcolor, parsed);
	else if (strcmp(key, "bordercolor") == 0)
		copy_color(bordercolor, parsed);
	else if (strcmp(key, "focuscolor") == 0)
		copy_color(focuscolor, parsed);
	else if (strcmp(key, "urgentcolor") == 0)
		copy_color(urgentcolor, parsed);
	else if (strcmp(key, "fullscreen_bg") == 0)
		copy_color(fullscreen_bg, parsed);
	else if (strcmp(key, "scratchpad_bg") == 0)
		copy_color(scratchpad_bg, parsed);
	else if (strcmp(key, "snap_border_center") == 0)
		copy_color(snap_border_center, parsed);
	else if (strcmp(key, "snap_bg_center") == 0)
		copy_color(snap_bg_center, parsed);
	else if (strcmp(key, "snap_border_edge") == 0)
		copy_color(snap_border_edge, parsed);
	else if (strcmp(key, "snap_bg_edge") == 0)
		copy_color(snap_bg_edge, parsed);
}

void
load_color_config(const char *custom_path)
{
	FILE *fp = NULL;
	char pathbuf[512];

	init_default_colors();

	if (custom_path) {
		fp = fopen(custom_path, "r");
	}

	if (!fp) {
		const char *xdg = getenv("XDG_CONFIG_HOME");
		const char *home = getenv("HOME");
		if (xdg && *xdg) {
			snprintf(pathbuf, sizeof(pathbuf), "%s/dwl/colors.conf", xdg);
		} else if (home && *home) {
			snprintf(pathbuf, sizeof(pathbuf), "%s/.config/dwl/colors.conf", home);
		} else {
			pathbuf[0] = '\0';
		}

		if (pathbuf[0])
			fp = fopen(pathbuf, "r");
	}

	if (!fp)
		fp = fopen("/etc/dwl/colors.conf", "r");

	if (!fp)
		return;

	char line[256];
	while (fgets(line, sizeof(line), fp)) {
		char *p = line;
		while (isspace((unsigned char)*p))
			p++;

		if (*p == '\0' || *p == '#' || *p == ';')
			continue;

		char *eq = strchr(p, '=');
		if (!eq)
			continue;

		*eq = '\0';
		char *key = p;
		char *val = eq + 1;

		char *end = key + strlen(key) - 1;
		while (end >= key && isspace((unsigned char)*end)) {
			*end = '\0';
			end--;
		}

		while (isspace((unsigned char)*val))
			val++;

		end = val + strlen(val) - 1;
		while (end >= val && isspace((unsigned char)*end)) {
			*end = '\0';
			end--;
		}

		set_color_key(key, val);
	}

	fclose(fp);
}
