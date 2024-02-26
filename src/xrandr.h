#ifndef MWM_XRANDR_H
#define MWM_XRANDR_H

#include <X11/Xlib.h>
#include "common.h"

typedef XID xrandr_output_t;
typedef XID xrandr_crtc_t;

typedef enum {
	XRANDR_MONITOR_ATTACHED = 0,
	XRANDR_MONITOR_DETACHED,
	XRANDR_MONITOR_GEOMETRY_CHANGED,
	XRANDR_EVENT_MAX
} xrandr_event_t;

struct xrandr;

typedef void (xrandr_func_t)(struct xrandr*,
                             xrandr_crtc_t,
                             struct geom*,
                             void*);

int xrandr_new(struct xrandr **xrr, Display *display, Window window);
int xrandr_free(struct xrandr **xrr);

void xrandr_handle_event(struct xrandr *xrr, XEvent *event);
void xrandr_update(struct xrandr *xrr);

int xrandr_set_callback(struct xrandr *xrr,
                        xrandr_event_t event,
                        xrandr_func_t *handler,
                        void *userdata);

#endif /* MWM_XRANDR_H */
