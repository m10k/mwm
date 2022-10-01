#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "monitor.h"
#include "client.h"
#include "workspace.h"
#include "layout.h"
#include "common.h"

#define PADDING 8

struct layout {
	char *name;
	int (*arrange)(struct client*, struct geom*, int, int);
	layout_orientation_t orientation;
};

struct layout_args {
	struct layout *layout;
	int total_clients;
	int arranged_clients;
	struct geom usable_area;
};

static int bookshelf(struct client *client, struct geom *unallocated,
		     int arranged_clients, int total_clients);
static int bookstack(struct client *client, struct geom *unallocated,
		     int arranged_clients, int total_clients);

static struct layout layout_bookshelf = {
	.name = "縦",
	.arrange = bookshelf,
	.orientation = LAYOUT_HORIZONTAL
};

static struct layout layout_bookstack = {
	.name = "横",
	.arrange = bookstack,
	.orientation = LAYOUT_VERTICAL
};

struct layout *layouts[] = {
	&layout_bookshelf,
	&layout_bookstack,
	NULL
};

static int bookshelf(struct client *client, struct geom *unallocated,
		     int arranged_clients, int total_clients)
{
	struct geom geom;
	int w;

	w = unallocated->w / (total_clients - arranged_clients);

	geom.x = unallocated->x + PADDING;
	geom.y = unallocated->y;
	geom.w = w - PADDING - ((arranged_clients + 1 == total_clients) ? PADDING : 0);
	geom.h = unallocated->h - PADDING;

	unallocated->x += w;
	unallocated->w -= w;

	return(client_set_geometry(client, &geom));
}

static int bookstack(struct client *client, struct geom *unallocated,
		     int arranged_clients, int total_clients)
{
	struct geom geom;
	int h;

	h = unallocated->h / (total_clients - arranged_clients);

	geom.x = unallocated->x + PADDING;
	geom.y = unallocated->y + PADDING;
	geom.w = unallocated->w - PADDING;
	geom.h = h - PADDING - ((arranged_clients + 1 == total_clients) ? PADDING : 0);

	unallocated->y += h;
	unallocated->h -= h;

	return(client_set_geometry(client, &geom));
}

int _arrange_workspace(struct workspace *workspace, struct client *client,
		       void *data)
{
	struct layout_args *args;

	if(!workspace || !client || !data) {
		return(-EINVAL);
	}

	args = (struct layout_args*)data;

	args->layout->arrange(client, &args->usable_area,
			      args->arranged_clients,
			      args->total_clients);
	args->arranged_clients++;

	return(0);
}

int layout_arrange(struct layout *layout,
		   struct workspace *workspace,
		   struct geom *usable_area)
{
	struct layout_args args;

	if(!layout || !workspace || !usable_area) {
		return(-EINVAL);
	}

	args.layout = layout;
	args.total_clients = workspace_count_tiled_clients(workspace);
	args.arranged_clients = 0;
	memcpy(&args.usable_area, usable_area, sizeof(*usable_area));

	workspace_foreach_client(workspace, _arrange_workspace, &args);

	return(0);
}

layout_orientation_t layout_get_orientation(struct layout *layout)
{
	return(layout->orientation);
}
