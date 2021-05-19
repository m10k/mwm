#ifndef MWM_CLIENT_H
#define MWM_CLIENT_H 1

#include <X11/Xlib.h>

typedef enum {
	CLIENT_FIXED      = (1 << 0),
	CLIENT_FLOATING   = (1 << 1),
	CLIENT_URGENT     = (1 << 2),
	CLIENT_NEVERFOCUS = (1 << 3),
	CLIENT_FULLSCREEN = (1 << 4)
} client_flags_t;

struct client;
struct workspace;
struct geom;

int client_new(Window window, XWindowAttributes *attrs, struct client **client);
int client_free(struct client **client);

int client_get_border(struct client *client);
void client_set_border(struct client *client, int border);

Window client_get_window(struct client *client);
int client_owns_window(struct client *client, Window *window);
int client_configure(struct client *client);
int client_redraw(struct client *client);
int client_is_floating(struct client *client);
int client_is_visible(struct client *client);
int client_is_tiled(struct client *client);
int client_get_geometry(struct client *client, struct geom *geom);
int client_set_geometry(struct client *client, struct geom *geom);
int client_change_geometry(struct client *client, struct geom *geom);

int client_set_workspace(struct client *client, struct workspace *workspace);
struct workspace* client_get_workspace(struct client *client);
struct monitor* client_get_viewer(struct client *client);
int client_show(struct client *client);

int client_needs_redraw(struct client *client);
int client_focus(struct client *client);

int client_set_state(struct client *client, long state);

#endif /* MWM_CLIENT_H */
