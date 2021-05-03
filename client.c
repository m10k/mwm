#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <X11/Xlib.h>
#include "common.h"
#include "client.h"
#include "workspace.h"

struct client {
	struct geom geom;
	Window window;
	client_flags_t flags;
	struct workspace *workspace;
};

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
