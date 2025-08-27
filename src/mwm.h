#ifndef MWM_H
#define MWM_H 1

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include "theme.h"
#include "common.h"
#include "client.h"

struct mwm;
struct monitor;
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
	MWM_CMD_KBPTR_MOVE,
	MWM_CMD_KBPTR_CLICK,
	MWM_CMD_MAX
} mwm_cmd_t;

typedef enum {
	MWM_ATOM_HINT = 0,
	MWM_ATOM_UTF8,
	MWM_ATOM_MAX
} mwm_atom_t;

int mwm_init(void);
int mwm_cleanup(void);
int mwm_run(void);
int mwm_stop(void);

int mwm_needs_redraw(void);
int mwm_redraw(void);

Display* mwm_get_display(void);
Window mwm_get_root_window(void);

int mwm_attach_monitor(struct monitor *mon);
int mwm_detach_monitor(struct monitor *mon);
int mwm_focus_monitor(struct monitor *mon);
struct monitor* mwm_get_focused_monitor(void);
int mwm_find_monitor(int (*cmp)(struct monitor*, void*),
                     void*, struct monitor**);

int mwm_attach_client(const client_t client);
int mwm_detach_client(const client_t client);
int mwm_focus_client(const client_t client);
client_t mwm_get_focused_client(void);

int mwm_attach_workspace(struct workspace *workspace);
int mwm_detach_workspace(struct workspace *workspace);
int mwm_focus_workspace(struct workspace *workspace);
struct workspace* mwm_get_focused_workspace(void);
int mwm_find_workspace(int (*cmp)(struct workspace*, void*),
                       void*, struct workspace**);
int mwm_foreach_workspace(int (*func)(struct workspace*, void*),
                          void *data);

Window mwm_create_window(const int x, const int y, const int w, const int h);
GC mwm_create_gc(void);
XftDraw* mwm_create_xft_context(Drawable drawable);
Drawable mwm_create_pixmap(Window window, const int width, const int height);
void mwm_free_pixmap(Drawable drawable);
int mwm_render_text(XftDraw *drawable,
                    mwm_palette_t palette, const char *text,
                    const int x, const int y,
                    const int w, const int h);
int mwm_render_text_vertical(XftDraw *drawable,
                             mwm_palette_t palette, const char *text,
                             const int x, const int y,
                             const int w, const int h);
int mwm_get_font_height(void);
int mwm_get_text_width(const char *text);
int mwm_get_text_property(Window window, Atom atom, char **dst);
unsigned long mwm_get_color(mwm_palette_t palette, mwm_color_t color);

int mwm_get_pointer(struct geom *pointer);
int mwm_get_status(char **dst);
int mwm_grab_keys(void);
int mwm_cmd(mwm_cmd_t, void *data);
int mwm_get_atom_by_name(const char *name, Atom *dst);
int mwm_get_atom(mwm_atom_t atom_id, Atom *dst);

#endif /* MWM_H */
