/* Taken from https://github.com/djpohly/dwl/issues/466 */
#define COLOR(hex)    { ((hex >> 24) & 0xFF) / 255.0f, \
                        ((hex >> 16) & 0xFF) / 255.0f, \
                        ((hex >> 8) & 0xFF) / 255.0f, \
                        (hex & 0xFF) / 255.0f }

/* appearance */
static const int sloppyfocus               = 1;  /* focus follows mouse */
static const int warpcursor                = 1;  /* warp cursor to center of window when focused via binds */
static const int bypass_surface_visibility = 0;  /* 1 means idle inhibitors will disable idle tracking even if it's surface isn't visible  */
static const unsigned int borderpx         = 2;  /* border pixel of windows */
static const unsigned int gappx            = 8;  /* margin/gap pixel around/between windows */
static const unsigned int min_width        = 100; /* minimum width of windows */
static const unsigned int min_height       = 100; /* minimum height of windows */
static const unsigned int corner_radius    = 16;  /* rounded corner radius of windows */
static const float rootcolor[]             = COLOR(0x19181dff);
static const float bordercolor[]           = COLOR(0x32303aff);
static const float focuscolor[]            = COLOR(0xa9b1d6ff);
static const float urgentcolor[]           = COLOR(0xf7768eff);
/* This conforms to the xdg-protocol. Set the alpha to zero to restore the old behavior */
static const float fullscreen_bg[]         = {0.0f, 0.0f, 0.0f, 1.0f};

/* Snap overlay colors */
static const float snap_border_center[]    = COLOR(0xd93333bf);
static const float snap_bg_center[]        = COLOR(0x660d0d80);
static const float snap_border_edge[]      = COLOR(0xbf2626b3);
static const float snap_bg_edge[]          = COLOR(0x800d0d80);

/* Jump label characters for overview mode (defaults to left-hand keys, excluding hjkl) */
static const char overview_labels[]        = "ASDFGQWERTZXCVB";

static const Rule rules[] = {
	/* app_id             title       isfloating   monitor */
	{ "Gimp_EXAMPLE",     NULL,       1,           -1 },
};

/* layout(s) */
static const Layout layouts[] = {
	/* symbol     arrange function */
  { "[\\]",     dwindle },
	{ "RT",       tree_layout },
	{ "[]=",      tile },
	{ "><>",      NULL },
	{ "[M]",      monocle },
	{ "(@)",      spiral },
};

/* monitors */
static const MonitorRule monrules[] = {
	{ NULL,       0.55f, 1,      1,    &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL,   -1,  -1 },
};

/* keyboard */
static const struct xkb_rule_names xkb_rules = {
	.options = NULL,
};

static const int repeat_rate = 35;
static const int repeat_delay = 200;

/* Trackpad & Mouse */
static const int tap_to_click = 1;
static const int tap_and_drag = 1;
static const int drag_lock = 1;
static const int natural_scrolling = 1;
static const int mouse_natural_scrolling = 0;
static const int disable_while_typing = 1;
static const int left_handed = 0;
static const int middle_button_emulation = 0;
static const enum libinput_config_scroll_method scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
static const enum libinput_config_click_method click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
static const uint32_t send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
static const enum libinput_config_accel_profile accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
static const double accel_speed = 0.0;
static const enum libinput_config_tap_button_map button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;

/* Key modifier definition */
#define MODKEY WLR_MODIFIER_LOGO

/* helper for spawning shell commands */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static const char *termcmd[] = { "foot", NULL };
static const char *vicinaecmd[] = { "sh", "-c", "vicinae toggle", NULL };
static const char *cmdcentercmd[] = { "quickshell", "ipc", "call", "command-center", "toggle", NULL };
static const char *lockcmd[] = { "quickshell", "ipc", "call", "lockscreen", "lock", NULL };
static const char *yazicmd[] = { "foot", "-e", "yazi", NULL };

/* Screenshots */
static const char *ss_crop[] = { "sh", "-c", "pgrep -x slurp >/dev/null && exit 0; GEOM=$(slurp); [ -n \"$GEOM\" ] && grim -g \"$GEOM\" - | tee \"$HOME/Pictures/Screenshot_$(date +'%Y-%m-%d_%H-%M-%S').png\" | wl-copy -t image/png", NULL };
static const char *ss_ocr[] = { "sh", "-c", "pgrep -x slurp >/dev/null && exit 0; GEOM=$(slurp); [ -n \"$GEOM\" ] && grim -g \"$GEOM\" - | tesseract stdin stdout -l eng 2>/dev/null | wl-copy && notify-send 'OCR Complete' 'Text copied to clipboard.'", NULL };

/* Hardware keys */
static const char *volup[] = { "wpctl", "set-volume", "-l", "1.0", "@DEFAULT_AUDIO_SINK@", "5%+", NULL };
static const char *voldown[] = { "wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", "5%-", NULL };
static const char *volmute[] = { "wpctl", "set-mute", "@DEFAULT_AUDIO_SINK@", "toggle", NULL };
static const char *micmute[] = { "wpctl", "set-mute", "@DEFAULT_AUDIO_SOURCE@", "toggle", NULL };

static const char *mediaplay[] = { "playerctl", "play-pause", NULL };
static const char *medianext[] = { "playerctl", "next", NULL };
static const char *mediaprev[] = { "playerctl", "previous", NULL };
static const char *mediastop[] = { "playerctl", "stop", NULL };

static const char *brightup[] = { "brightnessctl", "set", "+5%", NULL };
static const char *brightdown[] = { "brightnessctl", "set", "5%-", NULL };

static const Key keys[] = {
	/* Application Launchers */
	{ MODKEY,                    XKB_KEY_Return,      spawn,            {.v = termcmd} },
	{ MODKEY,                    XKB_KEY_space,       spawn,            {.v = vicinaecmd} },
	{ MODKEY,                    XKB_KEY_d,           spawn,            {.v = cmdcentercmd} },
	{ MODKEY|WLR_MODIFIER_ALT,   XKB_KEY_l,           spawn,            {.v = lockcmd} },
	{ MODKEY,                    XKB_KEY_e,           spawn,            {.v = yazicmd} },

	/* Window Management */
	{ MODKEY,                    XKB_KEY_q,           killclient,       {0} },
	{ MODKEY,                    XKB_KEY_f,           togglefullscreen, {0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_f,           togglefullscreen, {0} },
	{ MODKEY,                    XKB_KEY_v,           togglefloating,   {0} },
	{ MODKEY,                    XKB_KEY_c,           togglefloating,   {0} },
	{ MODKEY,                    XKB_KEY_g,           togglefloating,   {0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_r,           quit,             {0} },

	/* Directional Focus (Vim & Arrow keys) - Spatial focusdir */
	{ MODKEY,                    XKB_KEY_h,           focusdir,         {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY,                    XKB_KEY_l,           focusdir,         {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY,                    XKB_KEY_k,           focusdir,         {.i = WLR_DIRECTION_UP} },
	{ MODKEY,                    XKB_KEY_j,           focusdir,         {.i = WLR_DIRECTION_DOWN} },
	{ MODKEY,                    XKB_KEY_Left,        focusdir,         {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY,                    XKB_KEY_Right,       focusdir,         {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY,                    XKB_KEY_Up,          focusdir,         {.i = WLR_DIRECTION_UP} },
	{ MODKEY,                    XKB_KEY_Down,        focusdir,         {.i = WLR_DIRECTION_DOWN} },

	/* Move / Swap Windows - Tree Swap */
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_h,           tree_swap_dir,     {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_l,           tree_swap_dir,     {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_k,           tree_swap_dir,     {.i = WLR_DIRECTION_UP} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_j,           tree_swap_dir,     {.i = WLR_DIRECTION_DOWN} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Left,        tree_swap_dir,     {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Right,       tree_swap_dir,     {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Up,          tree_swap_dir,     {.i = WLR_DIRECTION_UP} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Down,        tree_swap_dir,     {.i = WLR_DIRECTION_DOWN} },

	/* Window Resizing - Tree Resizing */
	{ MODKEY|WLR_MODIFIER_CTRL,  XKB_KEY_h,           tree_resize_dir,     {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY|WLR_MODIFIER_CTRL,  XKB_KEY_l,           tree_resize_dir,     {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY|WLR_MODIFIER_CTRL,  XKB_KEY_k,           tree_resize_dir,     {.i = WLR_DIRECTION_UP} },
	{ MODKEY|WLR_MODIFIER_CTRL,  XKB_KEY_j,           tree_resize_dir,     {.i = WLR_DIRECTION_DOWN} },
	{ MODKEY|WLR_MODIFIER_CTRL,  XKB_KEY_Left,        tree_resize_dir,     {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY|WLR_MODIFIER_CTRL,  XKB_KEY_Right,       tree_resize_dir,     {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY|WLR_MODIFIER_CTRL,  XKB_KEY_Up,          tree_resize_dir,     {.i = WLR_DIRECTION_UP} },
	{ MODKEY|WLR_MODIFIER_CTRL,  XKB_KEY_Down,        tree_resize_dir,     {.i = WLR_DIRECTION_DOWN} },
	{ MODKEY,                    XKB_KEY_equal,       tree_resize_active,  {.f = +0.15f} },
	{ MODKEY,                    XKB_KEY_minus,       tree_resize_active,  {.f = -0.15f} },

	/* Tree Split Type & Equalization */
	{ MODKEY,                    XKB_KEY_b,           tree_set_split_type, {.i = SPLIT_HORIZONTAL} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_b,           tree_set_split_type, {.i = SPLIT_VERTICAL} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_B,           tree_set_split_type, {.i = SPLIT_VERTICAL} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_v,           tree_set_split_type, {.i = SPLIT_VERTICAL} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_V,           tree_set_split_type, {.i = SPLIT_VERTICAL} },
	{ MODKEY|WLR_MODIFIER_ALT,   XKB_KEY_b,           tree_set_split_type, {.i = SPLIT_HORIZONTAL} },
	{ MODKEY|WLR_MODIFIER_ALT,   XKB_KEY_v,           tree_set_split_type, {.i = SPLIT_VERTICAL} },
	{ MODKEY|WLR_MODIFIER_ALT,   XKB_KEY_e,           tree_equalize_active,  {0} },

	/* Workspace Navigation */
	{ MODKEY,                    XKB_KEY_1,           view_workspace,    {.i = 1} },
	{ MODKEY,                    XKB_KEY_2,           view_workspace,    {.i = 2} },
	{ MODKEY,                    XKB_KEY_3,           view_workspace,    {.i = 3} },
	{ MODKEY,                    XKB_KEY_4,           view_workspace,    {.i = 4} },
	{ MODKEY,                    XKB_KEY_5,           view_workspace,    {.i = 5} },
	{ MODKEY,                    XKB_KEY_6,           view_workspace,    {.i = 6} },
	{ MODKEY,                    XKB_KEY_7,           view_workspace,    {.i = 7} },
	{ MODKEY,                    XKB_KEY_8,           view_workspace,    {.i = 8} },
	{ MODKEY,                    XKB_KEY_9,           view_workspace,    {.i = 9} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_1,           move_to_workspace, {.i = 1} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_2,           move_to_workspace, {.i = 2} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_3,           move_to_workspace, {.i = 3} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_4,           move_to_workspace, {.i = 4} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_5,           move_to_workspace, {.i = 5} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_6,           move_to_workspace, {.i = 6} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_7,           move_to_workspace, {.i = 7} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_8,           move_to_workspace, {.i = 8} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_9,           move_to_workspace, {.i = 9} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_exclam,      move_to_workspace, {.i = 1} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_at,          move_to_workspace, {.i = 2} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_numbersign,  move_to_workspace, {.i = 3} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_dollar,      move_to_workspace, {.i = 4} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_percent,     move_to_workspace, {.i = 5} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_asciicircum, move_to_workspace, {.i = 6} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_ampersand,   move_to_workspace, {.i = 7} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_asterisk,    move_to_workspace, {.i = 8} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_parenleft,   move_to_workspace, {.i = 9} },

	/* Scratchpad Controls */
	{ MODKEY,                    XKB_KEY_grave,       togglescratchpad_view,   {0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_asciitilde,  togglescratchpad_client, {0} },
	{ MODKEY,                    XKB_KEY_p,           togglescratchpad_view,   {0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_p,           togglescratchpad_client, {0} },

	/* Monitor Focus (Vim & Arrow keys) */
	{ MODKEY|WLR_MODIFIER_ALT,   XKB_KEY_h,           focusmon,         {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY|WLR_MODIFIER_ALT,   XKB_KEY_l,           focusmon,         {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY|WLR_MODIFIER_ALT,   XKB_KEY_k,           focusmon,         {.i = WLR_DIRECTION_UP} },
	{ MODKEY|WLR_MODIFIER_ALT,   XKB_KEY_j,           focusmon,         {.i = WLR_DIRECTION_DOWN} },
	{ MODKEY|WLR_MODIFIER_ALT,   XKB_KEY_Left,        focusmon,         {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY|WLR_MODIFIER_ALT,   XKB_KEY_Right,       focusmon,         {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY|WLR_MODIFIER_ALT,   XKB_KEY_Up,          focusmon,         {.i = WLR_DIRECTION_UP} },
	{ MODKEY|WLR_MODIFIER_ALT,   XKB_KEY_Down,        focusmon,         {.i = WLR_DIRECTION_DOWN} },

	/* Move Window to Monitor (Vim & Arrow keys) */
	{ MODKEY|WLR_MODIFIER_SHIFT|WLR_MODIFIER_CTRL, XKB_KEY_h,     tagmon, {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY|WLR_MODIFIER_SHIFT|WLR_MODIFIER_CTRL, XKB_KEY_l,     tagmon, {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY|WLR_MODIFIER_SHIFT|WLR_MODIFIER_CTRL, XKB_KEY_k,     tagmon, {.i = WLR_DIRECTION_UP} },
	{ MODKEY|WLR_MODIFIER_SHIFT|WLR_MODIFIER_CTRL, XKB_KEY_j,     tagmon, {.i = WLR_DIRECTION_DOWN} },
	{ MODKEY|WLR_MODIFIER_SHIFT|WLR_MODIFIER_CTRL, XKB_KEY_Left,  tagmon, {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY|WLR_MODIFIER_SHIFT|WLR_MODIFIER_CTRL, XKB_KEY_Right, tagmon, {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY|WLR_MODIFIER_SHIFT|WLR_MODIFIER_CTRL, XKB_KEY_Up,    tagmon, {.i = WLR_DIRECTION_UP} },
	{ MODKEY|WLR_MODIFIER_SHIFT|WLR_MODIFIER_CTRL, XKB_KEY_Down,  tagmon, {.i = WLR_DIRECTION_DOWN} },

	/* Layout Controls */
	{ MODKEY,                    XKB_KEY_o,           toggleoverview,   {0} },
  { MODKEY,                    XKB_KEY_r,           setlayout,        {.v = &layouts[0]} },
	{ MODKEY,                    XKB_KEY_t,           setlayout,        {.v = &layouts[1]} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_t,           setlayout,        {.v = &layouts[2]} },
	{ MODKEY,                    XKB_KEY_m,           setlayout,        {.v = &layouts[4]} },
	{ MODKEY,                    XKB_KEY_s,           setlayout,        {.v = &layouts[5]} },

	/* Screenshots */
	{ 0,                         XKB_KEY_Print,       spawn,            {.v = ss_crop} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_s,           spawn,            {.v = ss_crop} },
	{ MODKEY|WLR_MODIFIER_CTRL,  XKB_KEY_s,           spawn,            {.v = ss_ocr} },

	/* Volume Controls */
	{ 0,                         XKB_KEY_XF86AudioRaiseVolume, spawn,    {.v = volup} },
	{ 0,                         XKB_KEY_XF86AudioLowerVolume, spawn,    {.v = voldown} },
	{ 0,                         XKB_KEY_XF86AudioMute,        spawn,    {.v = volmute} },
	{ 0,                         XKB_KEY_XF86AudioMicMute,     spawn,    {.v = micmute} },

	/* Media Controls */
	{ 0,                         XKB_KEY_XF86AudioPlay,        spawn,    {.v = mediaplay} },
	{ 0,                         XKB_KEY_XF86AudioNext,        spawn,    {.v = medianext} },
	{ 0,                         XKB_KEY_XF86AudioPrev,        spawn,    {.v = mediaprev} },
	{ 0,                         XKB_KEY_XF86AudioStop,        spawn,    {.v = mediastop} },

	/* Display Brightness Controls */
	{ 0,                         XKB_KEY_XF86MonBrightnessUp,   spawn,   {.v = brightup} },
	{ 0,                         XKB_KEY_XF86MonBrightnessDown, spawn,   {.v = brightdown} },

	/* Ctrl-Alt-Backspace & VT switching */
	{ WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT, XKB_KEY_Terminate_Server, quit, {0} },
#define CHVT(n) { WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT,XKB_KEY_XF86Switch_VT_##n, chvt, {.ui = (n)} }
	CHVT(1), CHVT(2), CHVT(3), CHVT(4), CHVT(5), CHVT(6),
	CHVT(7), CHVT(8), CHVT(9), CHVT(10), CHVT(11), CHVT(12),
};

static const Button buttons[] = {
	{ MODKEY, BTN_LEFT,   moveresize,     {.ui = CurMove} },
	{ MODKEY, BTN_MIDDLE, togglefloating, {0} },
	{ MODKEY, BTN_RIGHT,  moveresize,     {.ui = CurResize} },
};
