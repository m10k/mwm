#define _DEFAULT_SOURCE
#include <X11/Xlib.h>
#include <unistd.h>
#include "mwm.h"
#include "client.h"
#include "kbptr.h"
#include "common.h"

struct kbptr {
	int x;
	int y;
	int hstride;
	int vstride;
	struct client *last_client;
};

static struct kbptr kbptr = {
	.last_client = NULL
};

static void do_button(struct mwm *mwm,
		      struct client *client,
		      const unsigned int button,
		      const unsigned int pressrelease)
{
	XButtonEvent event;
	unsigned int mask;
	unsigned int state;
	Display *display;
	Window window;

	display = mwm_get_display(mwm);
	window = client_get_window(client);

	switch(pressrelease) {
	case ButtonPress:
		mask = ButtonPressMask;
		state = 0;
		break;

	case ButtonReleaseMask:
		/*
		 * The button must have been set if it was released,
		 * hence the button's state mask must be set.
		 */
		mask = ButtonReleaseMask;
		state = button << 8;
		break;

	default:
		return;
	}

	if(XQueryPointer(display, window,
			 &event.root, &event.window,
			 &event.x_root, &event.y_root,
			 &event.x, &event.y,
			 &event.state) < 0) {
		return;
	}

	event.type = pressrelease;
	event.display = display;
	event.window = window;
	event.subwindow = None;
	event.time = CurrentTime;

	event.state = state;
	event.button = button;
	event.same_screen = True;

	if(XSendEvent(display, window, True, mask, (XEvent*)&event) >= 0) {
		XFlush(display);
	}

	return;
}

void kbptr_move(struct mwm *mwm, struct client *client, long direction)
{
	struct geom client_geom;
	unsigned int dir;
        unsigned int stepsize;

	client_get_geometry(client, &client_geom);

	if(client != kbptr.last_client) {
		kbptr.x = client_geom.w / 2;
		kbptr.y = client_geom.h / 2;
		kbptr.hstride = kbptr.x / 2;
		kbptr.vstride = kbptr.y / 2;
		kbptr.last_client = client;
	}

        dir = direction & KBPTR_DMASK;
        stepsize = direction & KBPTR_HALFSTEP;

        switch(dir) {
        case KBPTR_CENTER:
                kbptr.x = client_geom.w / 2;
                kbptr.y = client_geom.h / 2;
                kbptr.hstride = kbptr.x / 2;
                kbptr.vstride = kbptr.y / 2;
                break;

        case KBPTR_NORTH:
                kbptr.vstride >>= stepsize;
                kbptr.y -= kbptr.vstride;
                kbptr.vstride /= 2;
                break;

        case KBPTR_EAST:
                kbptr.hstride >>= stepsize;
                kbptr.x += kbptr.hstride;
                kbptr.hstride /= 2;
                break;

        case KBPTR_SOUTH:
                kbptr.vstride >>= stepsize;
                kbptr.y += kbptr.vstride;
                kbptr.vstride /= 2;
                break;

        case KBPTR_WEST:
                kbptr.hstride >>= stepsize;
                kbptr.x -= kbptr.hstride;
                kbptr.hstride /= 2;
                break;

        default:
                return;
        }

        XWarpPointer(mwm_get_display(mwm), None, client_get_window(client),
                     0, 0, 0, 0,
                     kbptr.x, kbptr.y);

        return;
}

void kbptr_click(struct mwm *mwm, struct client *client, long button)
{
	do_button(mwm, client, button, ButtonPress);
	usleep(100000);
	do_button(mwm, client, button, ButtonRelease);

	return;
}
