#ifndef MONITOR_H
#define MONITOR_H 1

#include "client.h"
#include "xrandr.h"
#include <X11/Xlib.h>

struct monitor;
struct layout;
struct workspace;
struct geom;

int monitor_new(xrandr_crtc_t crtc, int x, int y, int w, int h,
		struct monitor **monitor);
int monitor_free(struct monitor **monitor);

Display* monitor_get_display(struct monitor *monitor);
int monitor_set_layout(struct monitor *monitor,
		       struct layout *layout);
struct layout* monitor_get_layout(struct monitor *monitor);

xrandr_crtc_t monitor_get_crtc(struct monitor *monitor);

int monitor_get_geometry(struct monitor *monitor, struct geom *geom);
int monitor_set_geometry(struct monitor *monitor, struct geom *geom);
int monitor_get_usable_area(struct monitor *monitor, struct geom *geom);

int monitor_set_workspace(struct monitor *monitor, struct workspace *workspace);

struct workspace* monitor_get_workspace(struct monitor *monitor);
client_t monitor_get_focused_client(struct monitor *monitor);

int monitor_arrange_clients(struct monitor *monitor);
int monitor_needs_redraw(struct monitor *monitor);
int monitor_redraw(struct monitor *monitor);
int monitor_is_focused(struct monitor *monitor);

#if MWM_DEBUG
void monitor_dump(struct monitor *monitor);
#endif /* MWM_DEBUG */

#endif /* MONITOR_H */
