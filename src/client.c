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
#include "set.h"

struct client {
	client_t id;
	Window window;

	struct {
		struct geom current;
		struct geom next;
		int changed;
	} geom;

	struct geom pointer;
	int needs_redraw;

	struct workspace *workspace;

	char *hint;
};

static struct set *_clients = NULL;

static inline int __init(void)
{
	return _clients ? 0 : set_new(&_clients);
}

static inline int __get_client(struct client **client, const client_t client_id)
{
	int err;

	if (client_id < 0) {
		return -EINVAL;
	}

	if ((err = set_get(_clients, client_id, (void**)client)) < 0) {
		return err;
	}

	if (!*client) {
		return -EBADF;
	}

	return 0;
}

client_t client_new(Window window)
{
	struct client *client;
	int err;

	if ((err = __init()) < 0) {
		return err;
	}

	if (!(client = calloc(1, sizeof(*client)))) {
		return -ENOMEM;
	}

	client->window = window;
	client->pointer.x = -1;
	client->pointer.y = -1;
	client->pointer.w = 1;
	client->pointer.h = 1;

	if ((err = set_nq(_clients, client)) < 0) {
		free(client);
	} else {
		client->id = err;
	}

	return err;
}

int client_free(const client_t client_id)
{
	struct client *client;
	int err;

	if ((err = set_unset(_clients, client_id, (void**)&client)) < 0) {
		return err;
	}

	if(!client) {
		return -EBADF;
	}

	free(client);
	return 0;
}

int client_set_geometry(const client_t client_id, struct geom *geom)
{
	struct client *client;
	int err;

	if (!geom) {
		return -EINVAL;
	}

	if ((err = __get_client(&client, client_id)) < 0) {
		return err;
	}

	if (memcmp(&client->geom.current, geom, sizeof(*geom)) == 0) {
		return -EALREADY;
	}

	memcpy(&client->geom.next, geom, sizeof(*geom));
	client->geom.changed = 1;

	if (client_is_visible(client_id)) {
		client_needs_redraw(client_id);
	}

	return 0;
}

int client_get_geometry(const client_t client_id, struct geom *geom)
{
	struct client *client;
	int err;

	if (!geom) {
		return -EINVAL;
	}

	if ((err = __get_client(&client, client_id)) < 0) {
		return err;
	}

	memcpy(geom, &client->geom.current, sizeof(*geom));
	return 0;
}

int client_get_window(const client_t client_id, Window *window)
{
	struct client *client;
	int err;

	if (!window) {
		return -EINVAL;
	}

	if ((err = __get_client(&client, client_id)) < 0) {
		return err;
	}

	*window = client->window;
	return 0;
}

int client_redraw(const client_t client_id)
{
	struct client *client;
	int err;

	if ((err = __get_client(&client, client_id)) < 0) {
		return err;
	}

	if (client->geom.changed) {
		memcpy(&client->geom.current, &client->geom.next, sizeof(client->geom.next));
		memset(&client->geom.next, 0, sizeof(client->geom.next));
	}

	/*
	 * If the client should be visible, map it to the display and move/resize
	 * it as needed. Otherwise, move them outside of the visible area.
	 */
	if (client_is_visible(client_id)) {
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

int client_set_workspace(const client_t client_id, struct workspace *workspace)
{
	struct client *client;
	int err;

	if ((err = __get_client(&client, client_id)) < 0) {
		return err;
	}

	client->workspace = workspace;
	return 0;
}

int client_get_workspace(const client_t client_id, struct workspace **workspace)
{
	struct client *client;
	int err;

	if (!workspace) {
		return -EINVAL;
	}

	if ((err = __get_client(&client, client_id)) < 0) {
		return err;
	}

	*workspace = client->workspace;
	return 0;
}

int client_is_visible(const client_t client_id)
{
	struct workspace *workspace;

	workspace = NULL;

	return client_get_workspace(client_id, &workspace) == 0 &&
	       workspace != NULL &&
	       workspace_get_viewer(workspace) >= 0;
}

int client_needs_redraw(const client_t client_id)
{
	struct client *client;
	int err;

	if ((err = __get_client(&client, client_id)) < 0) {
		return err;
	}

	client->needs_redraw = 1;
	workspace_needs_redraw(client->workspace);

	return 0;
}

int client_focus(const client_t client_id)
{
	struct client *client;
	Display *display;
	struct geom pointer;
	struct geom extents;
	int err;

	if ((err = __get_client(&client, client_id)) < 0) {
		return err;
	}

	display = mwm_get_display();

	XSetInputFocus(display, client->window, RevertToPointerRoot, CurrentTime);

	if ((err = mwm_get_pointer(&pointer)) < 0) {
		return err;
	}

	/*
	 * If the pointer is not over the focused client, move it over the client.
	 * Because of the border, the window is actually slightly larger than what
	 * the client geometry says, so we need to add some tolerance, otherwise
	 * the pointer would suddenly jump to the center of the window when the
	 * user is moving the pointer over the border.
	 */
	extents.x = client->geom.current.x - 1;
	extents.y = client->geom.current.y - 1;
	extents.w = client->geom.current.w + 1;
	extents.h = client->geom.current.h + 1;

	if (!geom_contains(&extents, &pointer)) {
		client_restore_pointer(client_id);
	}

	return 0;
}

int client_save_pointer(const client_t client_id)
{
	struct client *client;
	int err;

	if ((err = __get_client(&client, client_id)) < 0) {
		return err;
	}

	if ((err = mwm_get_pointer(&client->pointer)) < 0) {
		return err;
	}

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

int client_restore_pointer(const client_t client_id)
{
	struct client *client;
	Display *display;
	int err;

	if ((err = __get_client(&client, client_id)) < 0) {
		return err;
	}

	display = mwm_get_display();

#ifdef MWM_DEBUG
	fprintf(stderr, "Restoring pointer (%d, %d), %dx%d\n",
	        client->pointer.x, client->pointer.y,
	        client->pointer.w, client->pointer.y);
#endif /* MWM_DEBUG */

	if (client->pointer.x < 0 || client->pointer.y < 0) {
		kbptr_move(client_id, KBPTR_CENTER);
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

int client_set_state(const client_t client_id, const long state)
{
	struct client *client;
	long wm_state;
        long data[2];
        int err;

        if ((err = __get_client(&client, client_id)) < 0) {
	        return err;
        }

	data[0] = state;
	data[1] = None;

	if(mwm_get_atom_by_name("WM_STATE", (Atom*)&wm_state) < 0) {
		return -EIO;
	}

        XChangeProperty(mwm_get_display(), client->window,
			wm_state, wm_state, 32,
			PropModeReplace, (unsigned char*)data, 2);
        return 0;
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
	client_needs_redraw(client->id);

	if (client->workspace) {
		workspace_needs_redraw(client->workspace);
	}

	return 0;
}

int client_property_notify(const client_t client_id, XPropertyEvent *event)
{
	struct client *client;
	int err;

	if ((err = __get_client(&client, client_id)) < 0) {
		return err;
	}

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

	return 0;
}

int client_get_hint(const client_t client_id, const char **hint)
{
	struct client *client;
	int err;

	if ((err = __get_client(&client, client_id)) < 0) {
		return err;
	}

	*hint = client->hint;
	return 0;
}

static int _cmp_client_window(struct client *client, Window *window)
{
	return client->window == *window ? 0 : 1;
}

static int _client_contains_geom(struct client *client, struct geom *geom)
{
	return geom_contains(&client->geom.current, geom) ? 0 : 1;
}

client_t client_of_window(const Window window)
{
	return set_search(_clients, (int(*)(void*, void*))_cmp_client_window, (void*)&window);
}

client_t client_at(const struct geom pos)
{
	return set_search(_clients, (int(*)(void*, void*))_client_contains_geom, (void*)&pos);
}

client_t client_at_xy(const int x, const int y)
{
	struct geom geom;

	geom.x = x;
	geom.y = y;
	geom.w = 0;
	geom.h = 0;

	return client_at(geom);
}

#if MWM_DEBUG
int client_dump(const client_t client_id)
{
	struct client *client;
	int err;

	if ((err = __get_client(&client, client_id)) < 0) {
		fprintf(stderr, "    Client %ld INVALID\n", client_id);
		return err;
	}

	fprintf(stderr,
	        "    Client %ld @ %p\n"
	        "      Window %lx\n"
	        "      Current geometry: %dx%d @ %dx%d\n"
	        "      Next geometry:    %dx%d @ %dx%d\n"
	        "      Geometry changed: %d\n"
	        "      Pointer:          %dx%d [w/h %dx%d]\n"
	        "      Needs redraw:     %d\n"
	        "      Workspace:        %p\n"
	        "      Hint:             %s\n",
	        client_id, (void*)client,
	        client->window,
	        client->geom.current.w, client->geom.current.h, client->geom.current.x, client->geom.current.y,
	        client->geom.next.w, client->geom.next.h, client->geom.next.x, client->geom.next.y,
	        client->geom.changed,
	        client->pointer.x, client->pointer.y, client->pointer.w, client->pointer.h,
	        client->needs_redraw,
	        (void*)client->workspace,
	        client->hint ? client->hint : "(none)");
	return 0;
}
#endif /* MWM_DEBUG */
