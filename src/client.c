#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "common.h"
#include "client.h"
#include "workspace.h"
#include "mwm.h"
#include "monitor.h"
#include "kbptr.h"

struct client {
	Window window;
	struct geom geom;
	struct geom pointer;
	int needs_redraw;

	int border_width;
	client_flags_t flags;
	struct workspace *workspace;
};

extern struct mwm *__mwm;

int client_new(Window window, XWindowAttributes *attrs, struct client **client)
{
	struct client *cl;

	if(!client) {
		return(-EINVAL);
	}

	cl = malloc(sizeof(*cl));

	if(!cl) {
		return(-ENOMEM);
	}

	memset(cl, 0, sizeof(*cl));

	cl->window = window;
	cl->pointer.x = -1;
	cl->pointer.y = -1;
	cl->pointer.w = 1;
	cl->pointer.h = 1;
	*client = cl;

	return(0);
}

int client_free(struct client **client)
{
	if(!client) {
		return(-EINVAL);
	}

	if(!*client) {
		return(-EALREADY);
	}

	free(*client);
	*client = NULL;

	return(0);
}

int client_set_geometry(struct client *client, struct geom *geom)
{
	if(!client || !geom) {
		return(-EINVAL);
	}

	if(memcmp(&client->geom, geom, sizeof(*geom)) == 0) {
		return(-EALREADY);
	}

	memcpy(&client->geom, geom, sizeof(*geom));

	if(client_is_visible(client)) {
		client_needs_redraw(client);
	}

	return(0);
}

int client_get_geometry(struct client *client, struct geom *geom)
{
	if(!client || !geom) {
		return(-EINVAL);
	}

	memcpy(geom, &client->geom, sizeof(*geom));
	return(0);
}

Window client_get_window(struct client *client)
{
	return(client->window);
}

int client_configure(struct client *client)
{
	XConfigureEvent configure_event;
	Display *display;
	Window window;

	display = mwm_get_display(__mwm);
	window = client->window;

	configure_event.type = ConfigureNotify;
	configure_event.display = display;
	configure_event.event = window;
	configure_event.window = window;
	configure_event.x = client->geom.x;
	configure_event.y = client->geom.y;
	configure_event.width = client->geom.w;
	configure_event.height = client->geom.h;
	configure_event.border_width = client->border_width;
	configure_event.above = None;
	configure_event.override_redirect = False;

	if(!XSendEvent(display, window, False, StructureNotifyMask,
		       (XEvent*)&configure_event)) {
		return(-EIO);
	}

	return(0);
}

int client_redraw(struct client *client)
{
	if(!client) {
		return(-EINVAL);
	}

	if(client_is_visible(client)) {
		XMapRaised(mwm_get_display(__mwm), client->window);
		XMoveWindow(mwm_get_display(__mwm), client->window,
			    client->geom.x, client->geom.y);
	} else {
		XMoveWindow(mwm_get_display(__mwm), client->window,
			    client->geom.w * -2, client->geom.y);
	}

	client->needs_redraw = 0;

	return(0);
}

int client_get_border(struct client *client)
{
	return(client->border_width);
}

void client_set_border(struct client *client, int border)
{
	client->border_width = border;
	return;
}

struct monitor* client_get_viewer(struct client *client)
{
	if(client->workspace) {
		return(workspace_get_viewer(client->workspace));
	}

	return(NULL);
}

int client_is_floating(struct client *client)
{
	struct monitor *viewer;

	if(!client) {
		return(FALSE);
	}

	if(client->flags & CLIENT_FLOATING) {
		return(TRUE);
	}

	viewer = client_get_viewer(client);

	return(viewer ? monitor_is_floating(viewer) : FALSE);
}

int client_set_workspace(struct client *client, struct workspace *workspace)
{
	if(!client || !workspace) {
		return(-EINVAL);
	}

	client->workspace = workspace;
	return(0);
}

struct workspace* client_get_workspace(struct client *client)
{
	return(client->workspace);
}

int client_is_visible(struct client *client)
{
	struct workspace *workspace;

	workspace = client_get_workspace(client);

	if(!workspace) {
		return(FALSE);
	}

	return(workspace_get_viewer(workspace) != NULL);
}

int client_is_tiled(struct client *client)
{
	if(!client) {
		return(FALSE);
	}

	if(client->flags & (CLIENT_FLOATING |
			    CLIENT_FULLSCREEN |
			    CLIENT_FIXED)) {
		return(FALSE);
	}

	return(TRUE);
}

int client_change_geometry(struct client *client, struct geom *geom)
{
	if(!client || !geom) {
		return(-EINVAL);
	}

	/* If the client is floating, we'll allow it */
	if(client_is_floating(client)) {
		client_set_geometry(client, geom);
	}

	return(0);
}

int client_show(struct client *client)
{
#if MWM_DEBUG
	printf("XMoveResizeWindow(%p, %ld, %d, %d, %d, %d)\n",
	       (void*)client, client->window,
	       client->geom.x, client->geom.y,
	       client->geom.w, client->geom.h);
#endif /* MWM_DEBUG */

	XMoveResizeWindow(mwm_get_display(__mwm), client->window,
			  client->geom.x, client->geom.y,
			  client->geom.w, client->geom.h);
	XMapRaised(mwm_get_display(__mwm), client->window);

	return(0);
}

int client_needs_redraw(struct client *client)
{
	if(!client) {
		return(-EINVAL);
	}

	client->needs_redraw = 1;
	workspace_needs_redraw(client->workspace);

	return(0);
}

int client_focus(struct client *client)
{
	Display *display;
	Window dontcare_w;
	int dontcare_i;
	unsigned int dontcare_u;
	struct geom extents;
	int x;
	int y;

	if(!client) {
		return(-EINVAL);
	}

	display = mwm_get_display(__mwm);

	XSetInputFocus(display, client->window, RevertToPointerRoot, CurrentTime);

	/*
	 * If the pointer is not over the focused client, move it over the client.
	 * Because of the border, the window is actually slightly larger than what
	 * the client geometry says, so we need to add some tolerance, otherwise
	 * the pointer would suddenly jump to the center of the window when the
	 * user is moving the pointer over the border.
	 */
	XQueryPointer(display, client->window, &dontcare_w, &dontcare_w,
		      &x, &y, &dontcare_i, &dontcare_i, &dontcare_u);

	extents.x = client->geom.x - 1;
	extents.y = client->geom.y - 1;
	extents.w = client->geom.x + client->geom.w + 1;
	extents.h = client->geom.y + client->geom.h + 1;

	if(!(x >= extents.x && y >= extents.y &&
	     x <= extents.w && y <= extents.h)) {
		client_restore_pointer(client);
	}

	return(0);
}

int client_save_pointer(struct client *client)
{
	Display *display;
	Window dontcare_w;
	int dontcare_i;
	unsigned int dontcare_u;

	display = mwm_get_display(__mwm);

	XQueryPointer(display, client->window, &dontcare_w, &dontcare_w,
	              &client->pointer.x, &client->pointer.y, &dontcare_i,
	              &dontcare_i, &dontcare_u);

	client->pointer.x -= client->geom.x;
	client->pointer.y -= client->geom.y;
	client->pointer.w = client->geom.w;
	client->pointer.h = client->geom.h;

#ifdef MWM_DEBUG
	fprintf(stderr, "Saved pointer: (%d, %d), %dx%d\n",
	        client->pointer.x, client->pointer.y,
	        client->pointer.w, client->pointer.h);
#endif /* MWM_DEBUG */

	return 0;
}

static void _client_scale_pointer(struct client *client)
{
	double w_scale;
	double h_scale;
	double new_x;
	double new_y;

	w_scale = (double)client->geom.w / (double)client->pointer.w;
	h_scale = (double)client->geom.h / (double)client->pointer.h;

	new_x = (double)client->pointer.x * w_scale;
	new_y = (double)client->pointer.y * h_scale;

#ifdef MWM_DEBUG
	fprintf(stderr, "Scaling pointer (%d, %d) -> (%d, %d)\n",
	        client->pointer.x, client->pointer.y,
	        (int)new_x, (int)new_y);
#endif /* MWM_DEBUG */

	client->pointer.x = (int)new_x;
	client->pointer.y = (int)new_y;
	client->pointer.w = client->geom.w;
	client->pointer.h = client->geom.h;

	return;
}

int client_restore_pointer(struct client *client)
{
	Display *display;

	display = mwm_get_display(__mwm);

#ifdef MWM_DEBUG
	fprintf(stderr, "Restoring pointer (%d, %d), %dx%d\n",
	        client->pointer.x, client->pointer.y,
	        client->pointer.w, client->pointer.y);
#endif /* MWM_DEBUG */

	if (client->pointer.x < 0 || client->pointer.y < 0) {
		kbptr_move(__mwm, client, KBPTR_CENTER);
	} else {
		/* scale the pointer position if the client was resized */
		if (client->geom.w != client->pointer.w ||
		    client->geom.h != client->pointer.h) {
			_client_scale_pointer(client);
		}

		XWarpPointer(display, None, client->window, 0, 0, 0, 0,
		             client->pointer.x, client->pointer.y);
	}

	return 0;
}

int client_set_state(struct client *client, const long state)
{
	long wm_state;
        long data[2];

	data[0] = state;
	data[1] = None;

	if(mwm_get_atom(__mwm, "WM_STATE", &wm_state) < 0) {
		return(-EIO);
	}

        XChangeProperty(mwm_get_display(__mwm), client->window,
			wm_state, wm_state, 32,
			PropModeReplace, (unsigned char*)data, 2);
        return(0);
}

static void _client_update_wm_hints(struct client *client)
{
	XWMHints *hints;

	hints = XGetWMHints(mwm_get_display(__mwm),
			    client->window);

	if (hints) {
		if (hints->flags & XUrgencyHint) {
			hints->flags &= ~XUrgencyHint;
			XSetWMHints(mwm_get_display(__mwm),
				    client->window, hints);
		}

		XFree(hints);
	}
}

void client_property_notify(struct client *client, XPropertyEvent *event)
{
	switch (event->atom) {
	case XA_WM_TRANSIENT_FOR:
		if (client->workspace) {
			workspace_needs_redraw(client->workspace);
		}
		break;

	case XA_WM_NORMAL_HINTS:
		/* ignore size hints */
		break;

	case XA_WM_HINTS:
		_client_update_wm_hints(client);
		break;

	default:
		break;
	}
}
