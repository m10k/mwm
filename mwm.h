#ifndef MWM_H
#define MWM_H 1

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include "theme.h"

struct mwm;
struct monitor;
struct client;
struct workspace;

typedef enum {
	MWM_CMD_QUIT = 0,
	MWM_CMD_SPAWN,
	MWM_CMD_SHOW_WORKSPACE,
	MWM_CMD_MOVE_TO_WORKSPACE,
	MWM_CMD_SET_LAYOUT,
	MWM_CMD_SHIFT_FOCUS,
	MWM_CMD_SHIFT_CLIENT,
	MWM_CMD_SHIFT_WORKSPACE,
	MWM_CMD_SHIFT_MONITOR_FOCUS,
	MWM_CMD_MAX
} mwm_cmd_t;

int mwm_new(struct mwm **mwm);
int mwm_free(struct mwm **mwm);

int mwm_init(struct mwm *mwm);
int mwm_run(struct mwm *mwm);
int mwm_stop(struct mwm *mwm);

int mwm_needs_redraw(struct mwm *mwm);
int mwm_redraw(struct mwm *mwm);

Display* mwm_get_display(struct mwm *mwm);
Window mwm_get_root_window(struct mwm *mwm);

int mwm_attach_monitor(struct mwm *mwm, struct monitor *mon);
int mwm_detach_monitor(struct mwm *mwm, struct monitor *mon);
int mwm_focus_monitor(struct mwm *mwm, struct monitor *mon);
struct monitor* mwm_get_focused_monitor(struct mwm *mwm);
int mwm_find_monitor(struct mwm *mwm, int (*cmp)(struct monitor*, void*),
		     void*, struct monitor**);

int mwm_attach_client(struct mwm *mwm, struct client *client);
int mwm_detach_client(struct mwm *mwm, struct client *client);
int mwm_focus_client(struct mwm *mwm, struct client *client);
struct client* mwm_get_focused_client(struct mwm *mwm);
int mwm_find_client(struct mwm *mwm, int (*cmp)(struct client*, void*),
		    void*, struct client**);

int mwm_attach_workspace(struct mwm *mwm, struct workspace *workspace);
int mwm_detach_workspace(struct mwm *mwm, struct workspace *workspace);
int mwm_focus_workspace(struct mwm *mwm, struct workspace *workspace);
struct workspace* mwm_get_focused_workspace(struct mwm *mwm);
int mwm_find_workspace(struct mwm *mwm, int (*cmp)(struct workspace*, void*),
		       void*, struct workspace**);
int mwm_foreach_workspace(struct mwm *mwm,
			  int (*func)(struct mwm*, struct workspace*, void*),
			  void *data);

Window mwm_create_window(struct mwm *mwm, const int x, const int y, const int w, const int h);
GC mwm_create_gc(struct mwm *mwm);
XftDraw* mwm_create_xft_context(struct mwm *mwm, Drawable drawable);

int mwm_render_text(struct mwm *mwm, XftDraw *drawable,
		    mwm_palette_t palette, const char *text,
		    const int x, const int y);
int mwm_get_font_height(struct mwm *mwm);
int mwm_get_text_width(struct mwm *mwm, const char *text);
unsigned long mwm_get_color(struct mwm *mwm, mwm_palette_t palette, mwm_color_t color);

int mwm_get_status(struct mwm *mwm, char *buffer, const size_t buffer_size);
int mwm_grab_keys(struct mwm *mwm);
int mwm_cmd(struct mwm *mwm, mwm_cmd_t, void *data);
int mwm_get_atom(struct mwm *mwm, const char *name, long *dst);

#endif /* MWM_H */
