#ifndef MONITOR_H
#define MONITOR_H 1

#include "common.h"
#include "client.h"
#include "xrandr.h"
#include <X11/Xlib.h>

typedef long monitor_t;

struct layout;
struct workspace;

monitor_t monitor_new(const xrandr_crtc_t crtc, const struct geom geom);
int monitor_free(const monitor_t monitor_id);

Display* monitor_get_display(const monitor_t monitor_id);
int monitor_set_layout(const monitor_t monitor_id, struct layout *layout);
int monitor_get_layout(const monitor_t monitor_id, struct layout **layout);

int monitor_get_crtc(const monitor_t monitor_id, xrandr_crtc_t *crtc);

int monitor_get_geometry(const monitor_t monitor_id, struct geom *geom);
int monitor_set_geometry(const monitor_t monitor_id, struct geom *geom);
int monitor_get_usable_area(const monitor_t monitor_id, struct geom *geom);

int monitor_set_workspace(const monitor_t monitor_id, struct workspace *workspace);
int monitor_get_workspace(const monitor_t monitor_id, struct workspace **workspace);

int monitor_get_focused_client(const monitor_t monitor_id, client_t *client);

int monitor_arrange_clients(const monitor_t monitor_id);
int monitor_needs_redraw(const monitor_t monitor_id);
int monitor_redraw(const monitor_t monitor_id);
int monitor_is_focused(const monitor_t monitor_id);

int monitor_foreach(int(*func)(const monitor_t, void*), void *data);

monitor_t monitor_of_window(const Window window);
monitor_t monitor_of_crtc(const xrandr_crtc_t crtc);
monitor_t monitor_at(const struct geom pos);
monitor_t monitor_at_xy(const int x, const int y);

#if MWM_DEBUG
int monitor_dump(const monitor_t monitor_id);
#endif /* MWM_DEBUG */

#endif /* MONITOR_H */
