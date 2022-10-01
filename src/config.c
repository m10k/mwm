#include <X11/keysym.h>
#include "theme.h"
#include "keys.h"
#include "mwm.h"
#include "kbptr.h"

#define MODKEY Mod4Mask

static const char *_menucmd[] = {
	"dmenu_run", NULL
};

static const char *_termcmd[] = {
	"xterm", "-e", "/bin/bash", "--login", NULL
};

struct theme config_theme = {
	.active = {
		.named = {
			.focused          = "#bb2323",
			.visible          = "#d2d2d2",
			.text             = "#2d2d2d",
			.background       = "#f8f8f8",
			.indicator_fill   = "#c25676",
			.indicator_border = "#bb2323",
			.client_indicator = "#d2d2d2"
		}
	},
	.inactive = {
		.named = {
			.focused          = "#486c9c",
			.visible          = "#d2d2d2",
			.text             = "#2d2d2d",
			.background       = "#f8f8f8",
			.indicator_fill   = "#486c9c",
			.indicator_border = "#708fc9",
			.client_indicator = "#d2d2d2",
		}
	},

	.statusbar_font = "青柳衡山フォントT 14"
};

struct key_binding config_keybindings[] = {
	{ MODKEY | ControlMask, XK_BackSpace,   MWM_CMD_QUIT,                NULL },
	{ MODKEY,               XK_p,           MWM_CMD_SPAWN,               (void*)_menucmd },
	{ MODKEY | ShiftMask,   XK_Return,      MWM_CMD_SPAWN,               (void*)_termcmd },

	{ MODKEY,               XK_1,           MWM_CMD_SHOW_WORKSPACE,      (void*)0 },
	{ MODKEY,               XK_2,           MWM_CMD_SHOW_WORKSPACE,      (void*)1 },
	{ MODKEY,               XK_3,           MWM_CMD_SHOW_WORKSPACE,      (void*)2 },
	{ MODKEY,               XK_4,           MWM_CMD_SHOW_WORKSPACE,      (void*)3 },
	{ MODKEY,               XK_5,           MWM_CMD_SHOW_WORKSPACE,      (void*)4 },
	{ MODKEY,               XK_6,           MWM_CMD_SHOW_WORKSPACE,      (void*)5 },
	{ MODKEY,               XK_7,           MWM_CMD_SHOW_WORKSPACE,      (void*)6 },
	{ MODKEY,               XK_8,           MWM_CMD_SHOW_WORKSPACE,      (void*)7 },
	{ MODKEY,               XK_9,           MWM_CMD_SHOW_WORKSPACE,      (void*)8 },
	{ MODKEY,               XK_0,           MWM_CMD_SHOW_WORKSPACE,      (void*)9 },
	{ MODKEY,               XK_minus,       MWM_CMD_SHOW_WORKSPACE,      (void*)10 },
	{ MODKEY,               XK_asciicircum, MWM_CMD_SHOW_WORKSPACE,      (void*)11 },
	{ MODKEY | ShiftMask,   XK_1,           MWM_CMD_MOVE_TO_WORKSPACE,   (void*)0 },
	{ MODKEY | ShiftMask,   XK_2,           MWM_CMD_MOVE_TO_WORKSPACE,   (void*)1 },
	{ MODKEY | ShiftMask,   XK_3,           MWM_CMD_MOVE_TO_WORKSPACE,   (void*)2 },
	{ MODKEY | ShiftMask,   XK_4,           MWM_CMD_MOVE_TO_WORKSPACE,   (void*)3 },
	{ MODKEY | ShiftMask,   XK_5,           MWM_CMD_MOVE_TO_WORKSPACE,   (void*)4 },
	{ MODKEY | ShiftMask,   XK_6,           MWM_CMD_MOVE_TO_WORKSPACE,   (void*)5 },
	{ MODKEY | ShiftMask,   XK_7,           MWM_CMD_MOVE_TO_WORKSPACE,   (void*)6 },
	{ MODKEY | ShiftMask,   XK_8,           MWM_CMD_MOVE_TO_WORKSPACE,   (void*)7 },
	{ MODKEY | ShiftMask,   XK_9,           MWM_CMD_MOVE_TO_WORKSPACE,   (void*)8 },
	{ MODKEY | ShiftMask,   XK_0,           MWM_CMD_MOVE_TO_WORKSPACE,   (void*)9 },
	{ MODKEY | ShiftMask,   XK_minus,       MWM_CMD_MOVE_TO_WORKSPACE,   (void*)10 },
	{ MODKEY | ShiftMask,   XK_asciicircum, MWM_CMD_MOVE_TO_WORKSPACE,   (void*)11 },
	{ MODKEY,               XK_t,           MWM_CMD_SET_LAYOUT,          (void*)0 },
	{ MODKEY,               XK_y,           MWM_CMD_SET_LAYOUT,          (void*)1 },

	{ MODKEY,               XK_a,           MWM_CMD_SHIFT_FOCUS,         (void*)-1 },
	{ MODKEY,               XK_d,           MWM_CMD_SHIFT_FOCUS,         (void*)+1 },
	{ MODKEY | ShiftMask,   XK_a,           MWM_CMD_SHIFT_CLIENT,        (void*)-1 },
	{ MODKEY | ShiftMask,   XK_d,           MWM_CMD_SHIFT_CLIENT,        (void*)+1 },
	{ MODKEY,               XK_w,           MWM_CMD_SHIFT_FOCUS,         (void*)-1 },
	{ MODKEY,               XK_s,           MWM_CMD_SHIFT_FOCUS,         (void*)+1 },
	{ MODKEY | ShiftMask,   XK_w,           MWM_CMD_SHIFT_CLIENT,        (void*)-1 },
	{ MODKEY | ShiftMask,   XK_s,           MWM_CMD_SHIFT_CLIENT,        (void*)+1 },

	{ MODKEY,               XK_q,           MWM_CMD_SHIFT_MONITOR_FOCUS, (void*)-1 },
	{ MODKEY | ShiftMask,   XK_q,           MWM_CMD_SHIFT_WORKSPACE,     (void*)-1 },
	{ MODKEY,               XK_e,           MWM_CMD_SHIFT_MONITOR_FOCUS, (void*)+1 },
	{ MODKEY | ShiftMask,   XK_e,           MWM_CMD_SHIFT_WORKSPACE,     (void*)+1 },

	{ MODKEY,               XK_u,           MWM_CMD_KBPTR_MOVE,          (void*)KBPTR_CENTER },
	{ MODKEY,               XK_i,           MWM_CMD_KBPTR_MOVE,          (void*)KBPTR_NORTH },
	{ MODKEY,               XK_l,           MWM_CMD_KBPTR_MOVE,          (void*)KBPTR_EAST },
	{ MODKEY,               XK_k,           MWM_CMD_KBPTR_MOVE,          (void*)KBPTR_SOUTH },
	{ MODKEY,               XK_j,           MWM_CMD_KBPTR_MOVE,          (void*)KBPTR_WEST },
	{ MODKEY | ShiftMask,   XK_i,           MWM_CMD_KBPTR_MOVE,          (void*)(KBPTR_NORTH | KBPTR_HALFSTEP) },
	{ MODKEY | ShiftMask,   XK_l,           MWM_CMD_KBPTR_MOVE,          (void*)(KBPTR_EAST  | KBPTR_HALFSTEP) },
	{ MODKEY | ShiftMask,   XK_k,           MWM_CMD_KBPTR_MOVE,          (void*)(KBPTR_SOUTH | KBPTR_HALFSTEP) },
	{ MODKEY | ShiftMask,   XK_j,           MWM_CMD_KBPTR_MOVE,          (void*)(KBPTR_WEST  | KBPTR_HALFSTEP) },
	{ MODKEY,               XK_semicolon,   MWM_CMD_KBPTR_CLICK,         (void*)KBPTR_LEFT },
	{ MODKEY | ShiftMask,   XK_semicolon,   MWM_CMD_KBPTR_CLICK,         (void*)KBPTR_RIGHT },
	{ MODKEY | ControlMask, XK_semicolon,   MWM_CMD_KBPTR_CLICK,         (void*)KBPTR_MIDDLE },

	{ 0,                    0,              MWM_CMD_MAX,                 0 }
};
