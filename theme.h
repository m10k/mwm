#ifndef MWM_THEME_H
#define MWM_THEME_H

typedef enum {
        MWM_PALETTE_ACTIVE = 0,
        MWM_PALETTE_INACTIVE,
        MWM_PALETTE_MAX
} mwm_palette_t;

typedef enum {
        MWM_COLOR_FOCUSED = 0,
        MWM_COLOR_VISIBLE,
        MWM_COLOR_TEXT,
        MWM_COLOR_BACKGROUND,
        MWM_COLOR_INDICATOR_FILL,
        MWM_COLOR_INDICATOR_BORDER,
        MWM_COLOR_CLIENT_INDICATOR,
        MWM_COLOR_MAX
} mwm_color_t;

union colorset {
	struct {
		const char *focused;
		const char *visible;
		const char *text;
		const char *background;
		const char *indicator_fill;
		const char *indicator_border;
		const char *client_indicator;
	} named;

	const char *indexed[MWM_COLOR_MAX];
};

struct theme {
	union colorset active;
	union colorset inactive;

	const char *statusbar_font;
};

#endif /* MWM_THEME_H */
