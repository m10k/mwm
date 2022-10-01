#include <X11/Xlib.h>
#include <string.h>
#include <errno.h>
#include "common.h"

void x_configure_notify(Display *display, Window window,
			struct geom *geom, unsigned int border)
{
	XConfigureEvent event;

	memset(&event, 0, sizeof(event));

	event.type = ConfigureNotify;
	event.display = display;
	event.event = window;
	event.window = window;

	if(geom) {
		event.x = geom->x;
		event.y = geom->y;
		event.width = geom->w;
		event.height = geom->h;
	}

	event.border_width = border;
	event.above = None;
	event.override_redirect = False;

	XSendEvent(display, window, False, StructureNotifyMask, (XEvent*)&event);

	return;
}

int x_get_geom(Display *display, Window window, struct geom *geom)
{
	Window root;
	unsigned int border;
	unsigned int depth;

	if(XGetGeometry(display, window, &root, &geom->x, &geom->y,
			&geom->w, &geom->h, &border, &depth) == False) {
		return(-EIO);
	}

	return(0);
}
