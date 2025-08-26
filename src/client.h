#ifndef MWM_CLIENT_H
#define MWM_CLIENT_H 1

#include <X11/Xlib.h>

typedef long client_t;

struct workspace;
struct geom;

client_t client_new(Window window);
int client_free(const client_t client_id);

int client_get_window(const client_t client_id, Window *window);
int client_redraw(const client_t client_id);
int client_is_visible(const client_t client_id);

int client_get_geometry(const client_t client_id, struct geom *geom);
int client_set_geometry(const client_t client_id, struct geom *geom);

int client_set_workspace(const client_t client_id, struct workspace *workspace);
int client_get_workspace(const client_t client_id, struct workspace **workspace);

int client_needs_redraw(const client_t client_id);
int client_focus(const client_t client_id);

int client_save_pointer(const client_t client_id);
int client_restore_pointer(const client_t client_id);

int client_set_state(const client_t client_id, long state);
int client_property_notify(const client_t client_id, XPropertyEvent *event);

int client_get_hint(const client_t client_id, const char **hint);

#if MWM_DEBUG
int client_dump(const client_t client_id);
#endif /* MWM_DEBUG */

#endif /* MWM_CLIENT_H */
