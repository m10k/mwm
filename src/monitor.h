#ifndef MONITOR_H
#define MONITOR_H 1

#include <X11/Xlib.h>

struct mwm;
struct monitor;
struct layout;
struct workspace;
struct geom;

int monitor_new(struct mwm *mwm, int id, int x, int y, int w, int h,
		struct monitor **monitor);
int monitor_free(struct monitor **monitor);

Display* monitor_get_display(struct monitor *monitor);
int monitor_set_layout(struct monitor *monitor,
		       struct layout *layout);
struct layout* monitor_get_layout(struct monitor *monitor);

int monitor_get_id(struct monitor *monitor);

int monitor_get_geometry(struct monitor *monitor, struct geom *geom);
int monitor_set_geometry(struct monitor *monitor, struct geom *geom);
int monitor_get_usable_area(struct monitor *monitor, struct geom *geom);

int monitor_set_workspace(struct monitor *monitor, struct workspace *workspace);

struct workspace* monitor_get_workspace(struct monitor *monitor);
struct client* monitor_get_focused_client(struct monitor *monitor);

int monitor_arrange_clients(struct monitor *monitor);
int monitor_needs_redraw(struct monitor *monitor);
int monitor_redraw(struct monitor *monitor);
int monitor_is_floating(struct monitor *monitor);
int monitor_is_dirty(struct monitor *monitor);
int monitor_is_focused(struct monitor *monitor);

#endif /* MONITOR_H */
