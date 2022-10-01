#ifndef MWM_X_H
#define MWM_X_H 1

#include <X11/Xlib.h>
#include "common.h"

void x_configure_notify(Display *display, Window window,
			struct geom *geom, unsigned int border);
int x_get_geom(Display *display, Window window, struct geom *geom);

#endif /* MWM_X_H */
