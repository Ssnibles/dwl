/*
 * See LICENSE file for copyright and license details.
 */
#ifndef COLOR_H
#define COLOR_H

extern float rootcolor[4];
extern float bordercolor[4];
extern float focuscolor[4];
extern float urgentcolor[4];
extern float fullscreen_bg[4];
extern float scratchpad_bg[4];
extern float snap_border_center[4];
extern float snap_bg_center[4];
extern float snap_border_edge[4];
extern float snap_bg_edge[4];

void load_color_config(const char *custom_path);

#endif /* COLOR_H */
