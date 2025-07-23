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

	struct {
		struct geom current;
		struct geom next;
		int changed;
	} geom;

	struct geom pointer;
	int needs_redraw;

	client_flags_t flags;
	struct workspace *workspace;

	char *hint;
};

int client_new(Window window, XWindowAttributes *attrs, struct client **client)
{
	struct client *cl;

	if(!client) {
		return(-EINVAL);
	}

	if (!(cl = calloc(1, sizeof(*cl)))) {
		return -ENOMEM;
	}

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
	if (!client || !geom) {
		return -EINVAL;
	}

	if (memcmp(&client->geom.current, geom, sizeof(*geom)) == 0) {
		return -EALREADY;
	}

	memcpy(&client->geom.next, geom, sizeof(*geom));
	client->geom.changed = 1;

	if (client_is_visible(client)) {
		client_needs_redraw(client);
	}

	return 0;
}

int client_get_geometry(struct client *client, struct geom *geom)
{
	if (!client || !geom) {
		return -EINVAL;
	}

	memcpy(geom, &client->geom.current, sizeof(*geom));
	return 0;
}

Window client_get_window(struct client *client)
{
	return(client->window);
}

int client_redraw(struct client *client)
{
	if (!client) {
		return -EINVAL;
	}

	if (client->geom.changed) {
		memcpy(&client->geom.current, &client->geom.next, sizeof(client->geom.next));
		memset(&client->geom.next, 0, sizeof(client->geom.next));
	}

	/*
	 * If the client should be visible, map it to the display and move/resize
	 * it as needed. Otherwise, move them outside of the visible area.
	 */
	if (client_is_visible(client)) {
		XMapRaised(mwm_get_display(), client->window);
		XMoveResizeWindow(mwm_get_display(), client->window,
		                  client->geom.current.x, client->geom.current.y,
		                  client->geom.current.w, client->geom.current.h);
	} else {
		XMoveWindow(mwm_get_display(), client->window,
		            client->geom.current.w * -2, client->geom.current.y);
	}

	client->geom.changed = 0;
	client->needs_redraw = 0;

	return 0;
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

	if (!client) {
		return -EINVAL;
	}

	display = mwm_get_display();

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

	extents.x = client->geom.current.x - 1;
	extents.y = client->geom.current.y - 1;
	extents.w = client->geom.current.x + client->geom.current.w + 1;
	extents.h = client->geom.current.y + client->geom.current.h + 1;

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

	display = mwm_get_display();

	XQueryPointer(display, client->window, &dontcare_w, &dontcare_w,
	              &client->pointer.x, &client->pointer.y, &dontcare_i,
	              &dontcare_i, &dontcare_u);

	client->pointer.x -= client->geom.current.x;
	client->pointer.y -= client->geom.current.y;
	client->pointer.w = client->geom.current.w;
	client->pointer.h = client->geom.current.h;

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

	w_scale = (double)client->geom.current.w / (double)client->pointer.w;
	h_scale = (double)client->geom.current.h / (double)client->pointer.h;

	new_x = (double)client->pointer.x * w_scale;
	new_y = (double)client->pointer.y * h_scale;

#ifdef MWM_DEBUG
	fprintf(stderr, "Scaling pointer (%d, %d) -> (%d, %d)\n",
	        client->pointer.x, client->pointer.y,
	        (int)new_x, (int)new_y);
#endif /* MWM_DEBUG */

	client->pointer.x = (int)new_x;
	client->pointer.y = (int)new_y;
	client->pointer.w = client->geom.current.w;
	client->pointer.h = client->geom.current.h;

	return;
}

int client_restore_pointer(struct client *client)
{
	Display *display;

	display = mwm_get_display();

#ifdef MWM_DEBUG
	fprintf(stderr, "Restoring pointer (%d, %d), %dx%d\n",
	        client->pointer.x, client->pointer.y,
	        client->pointer.w, client->pointer.y);
#endif /* MWM_DEBUG */

	if (client->pointer.x < 0 || client->pointer.y < 0) {
		kbptr_move(client, KBPTR_CENTER);
	} else {
		/* scale the pointer position if the client was resized */
		if (client->geom.current.w != client->pointer.w ||
		    client->geom.current.h != client->pointer.h) {
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

	if(mwm_get_atom_by_name("WM_STATE", (Atom*)&wm_state) < 0) {
		return(-EIO);
	}

        XChangeProperty(mwm_get_display(), client->window,
			wm_state, wm_state, 32,
			PropModeReplace, (unsigned char*)data, 2);
        return(0);
}

static void _client_update_wm_hints(struct client *client)
{
	XWMHints *hints;

	hints = XGetWMHints(mwm_get_display(),
			    client->window);

	if (hints) {
		if (hints->flags & XUrgencyHint) {
			hints->flags &= ~XUrgencyHint;
			XSetWMHints(mwm_get_display(),
				    client->window, hints);
		}

		XFree(hints);
	}
}

static int _client_update_mwm_hint(struct client *client, XPropertyEvent *event)
{
	char *hint;
	Atom MWM_HINT;

	hint = NULL;

	if (mwm_get_atom(MWM_ATOM_HINT, &MWM_HINT) < 0) {
		return -EIO;
	}

	if (event->atom != MWM_HINT) {
		return 0;
	}

	if (mwm_get_text_property(client->window, MWM_HINT,
	                          &hint) < 0) {
		return -EIO;
	}

	free(client->hint);
	client->hint = hint;
	client_needs_redraw(client);

	if (client->workspace) {
		workspace_needs_redraw(client->workspace);
	}

	return 0;
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
		_client_update_mwm_hint(client, event);
		break;
	}
}

const char* client_get_hint(struct client *client)
{
	return client->hint;
}
