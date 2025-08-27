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
	int (*arrange)(const client_t, struct geom*, int, int);
	layout_orientation_t orientation;
};

struct layout_args {
	struct layout *layout;
	int total_clients;
	int arranged_clients;
	struct geom usable_area;
};

static int bookshelf(const client_t client, struct geom *unallocated,
		     int arranged_clients, int total_clients);
static int bookstack(const client_t client, struct geom *unallocated,
		     int arranged_clients, int total_clients);
static int sink(const client_t client, struct geom *unallocated,
		int arranged_clients, int total_clients);
static int geom_bookshelf(const client_t client, struct geom *unallocated,
                          int arranged_clients, int total_clients);
static int geom_bookstack(const client_t client, struct geom *unallocated,
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

static struct layout layout_sink = {
	.name = "渦",
	.arrange = sink,
	.orientation = LAYOUT_HORIZONTAL | LAYOUT_VERTICAL
};

static struct layout layout_geom_bookshelf = {
	.name = "幾縦",
	.arrange = geom_bookshelf,
	.orientation = LAYOUT_HORIZONTAL
};

static struct layout layout_geom_bookstack = {
	.name = "幾横",
	.arrange = geom_bookstack,
	.orientation = LAYOUT_VERTICAL
};

struct layout *layouts[] = {
	&layout_bookshelf,
	&layout_bookstack,
	&layout_sink,
	&layout_geom_bookshelf,
	&layout_geom_bookstack,
	NULL
};

static int bookshelf(const client_t client, struct geom *unallocated,
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

static int bookstack(const client_t client, struct geom *unallocated,
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

static int sink(const client_t client, struct geom *unallocated,
                int arranged_clients, int total_clients)
{
	struct geom geom;
	struct geom old_unallocated;
	int gravity;
	int split;

#define GRAVITY_LEFT   0
#define GRAVITY_TOP    1
#define GRAVITY_RIGHT  2
#define GRAVITY_BOTTOM 3
#define MIN_WIDTH 128
#define MIN_HEIGHT 128

	/* sink: split the unallocated area in half and use the first half
	 *
	 * Which one is the first half depends on the number of clients that are
	 * aleady arranged. If this is the first client, split vertically and use
	 * the left half; if this is the second client, split horizontally and
	 * use the top half; if this is the third client, split vertically and use
	 * the right half, and so on.
	 *
	 * +------------+------------+
	 * |            |            |
	 * |            |      1     |
	 * |            |            |
	 * |     0      |------------+
	 * |            | 4 | 5|     |
	 * |            |------+  2  |
	 * |            |  3   |     |
	 * +------------+------+-----+
	 *
	 * gravity = {left, top, right, bottom}[n % 4]
	 */

	gravity = arranged_clients % 4;
	split = (arranged_clients + 1 == total_clients) ? 1 : 2;

	if (unallocated->w < MIN_WIDTH ||
	    unallocated->h < MIN_HEIGHT) {
		return -1;
	}

	memcpy(&old_unallocated, unallocated, sizeof(old_unallocated));

	switch (gravity) {
	case GRAVITY_LEFT:
		geom.x = unallocated->x;
		geom.y = unallocated->y;
		geom.w = unallocated->w / split;
		geom.h = unallocated->h;

		unallocated->x += geom.w;
		unallocated->w -= geom.w;
		break;

	case GRAVITY_TOP:
		geom.x = unallocated->x;
		geom.y = unallocated->y;
		geom.w = unallocated->w;
		geom.h = unallocated->h / split;

		unallocated->y += geom.h;
		unallocated->h -= geom.h;
		break;

	case GRAVITY_RIGHT:
		geom.w = unallocated->w;
		geom.h = unallocated->h;
		geom.x = unallocated->x;
		geom.y = unallocated->y;

		if (split == 2) {
			geom.w /= 2;
			geom.x += geom.w;
		}

		unallocated->w -= geom.w;
		break;

	case GRAVITY_BOTTOM:
		geom.h = unallocated->h;
		geom.w = unallocated->w;
		geom.x = unallocated->x;
		geom.y = unallocated->y;

		if (split == 2) {
			geom.h /= 2;
			geom.y += geom.h;
		}

		unallocated->h -= geom.h;
		break;

	default:
		return 0;
	}

	if (geom.w <= 2 * PADDING ||
	    geom.h <= 2 * PADDING) {
		/* don't map client if there isn't enough space */
		return -1;
	}

	if (unallocated->w < MIN_WIDTH ||
	    unallocated->h < MIN_HEIGHT) {
		/*
		 * Remaining area is too small for another client.
		 * Use up the entire area for this client.
		 */

		memcpy(&geom, &old_unallocated, sizeof(geom));
		unallocated->w = 0;
		unallocated->h = 0;
	}

	geom.x += PADDING;
	geom.y += PADDING;
	geom.w -= 2 * PADDING;
	geom.h -= 2 * PADDING;

#undef GRAVITY_LEFT
#undef GRAVITY_TOP
#undef GRAVITY_RIGHT
#undef GRAVITY_BOTTOM
#undef MIN_WIDTH
#undef MIN_HEIGHT

	return client_set_geometry(client, &geom);
}

static int geom_bookshelf(const client_t client, struct geom *unallocated,
                          int arranged_clients, int total_clients)
{
	struct geom geom;
	int w;
	int last_client;

	last_client = arranged_clients + 1 == total_clients;
	w = last_client ? unallocated->w : unallocated->w / 2;

	geom.x = unallocated->x + PADDING;
	geom.y = unallocated->y;
	geom.w = w - PADDING - (last_client ? PADDING : 0);
	geom.h = unallocated->h - PADDING;

	unallocated->x += w;
	unallocated->w -= w;

	return(client_set_geometry(client, &geom));
}

static int geom_bookstack(const client_t client, struct geom *unallocated,
                          int arranged_clients, int total_clients)
{
	struct geom geom;
	int h;
	int last_client;

	last_client = arranged_clients + 1 == total_clients;
	h = last_client ? unallocated->h : unallocated->h / 2;

	geom.x = unallocated->x + PADDING;
	geom.y = unallocated->y + PADDING;
	geom.w = unallocated->w - PADDING;
	geom.h = h - PADDING - (last_client ? PADDING : 0);

	unallocated->y += h;
	unallocated->h -= h;

	return(client_set_geometry(client, &geom));
}

int _arrange_workspace(struct workspace *workspace, const client_t client,
		       void *data)
{
	struct layout_args *args;

	if (!workspace || client < 0 || !data) {
		return -EINVAL;
	}

	args = (struct layout_args*)data;

	args->layout->arrange(client, &args->usable_area,
			      args->arranged_clients,
			      args->total_clients);
	args->arranged_clients++;

	return 0;
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
	args.total_clients = workspace_count_clients(workspace);
	args.arranged_clients = 0;
	memcpy(&args.usable_area, usable_area, sizeof(*usable_area));

	workspace_foreach_client(workspace, _arrange_workspace, &args);

	return(0);
}

layout_orientation_t layout_get_orientation(struct layout *layout)
{
	return(layout->orientation);
}
