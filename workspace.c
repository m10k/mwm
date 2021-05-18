#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "workspace.h"
#include "monitor.h"
#include "client.h"
#include "loop.h"
#include "common.h"

struct monitor;

struct workspace {
	struct loop *clients;
	struct client *focused;
	struct monitor *viewer;

	int number;
	int needs_redraw;
};

int workspace_new(const int number, struct workspace **workspace)
{
	struct workspace *wspace;

	if(!workspace) {
		return(-EINVAL);
	}

	wspace = malloc(sizeof(*wspace));

	if(!wspace) {
		return(-ENOMEM);
	}

	wspace->number = number;

	*workspace = wspace;

	return(0);
}

int workspace_free(struct workspace **workspace)
{
	if(!workspace) {
		return(-EINVAL);
	}

	if(!*workspace) {
		return(-EALREADY);
	}

	free(*workspace);
	*workspace = NULL;

	return(0);
}

int workspace_get_number(struct workspace *workspace)
{
	if(!workspace) {
		return(-EINVAL);
	}

	return(workspace->number);
}

int workspace_attach_client(struct workspace *workspace, struct client *client)
{
	if(!workspace || !client) {
		return(-EINVAL);
	}

	if(loop_append(&workspace->clients, client) < 0) {
		return(-ENOMEM);
	}

	client_set_workspace(client, workspace);

	if(!workspace_get_focused_client(workspace)) {
		workspace_focus_client(workspace, client);
	}

	workspace_needs_redraw(workspace);

	return(0);
}


int workspace_detach_client(struct workspace *workspace, struct client *client)
{
	int err;

#if MWM_DEBUG
	printf("%s(%p, %p)\n", __func__, (void*)workspace, (void*)client);
#endif /* MWM_DEBUG */

	if(!workspace || !client) {
		return(-EINVAL);
	}

	err = loop_remove(&workspace->clients, client);

	if(err < 0) {
		return(err);
	}

	if(workspace_get_focused_client(workspace) == client) {
		struct client *first;

		if(loop_get_first(&workspace->clients, (void**)&first) < 0) {
			first = NULL;
		}

		workspace_focus_client(workspace, first);
	}

	workspace_needs_redraw(workspace);

	return(0);
}

int workspace_find_client(struct workspace *workspace,
			  int (*cmp)(struct client*, void*),
			  void *data, struct client **client)
{
	if(!workspace) {
		return(-EINVAL);
	}

	return(loop_find(&workspace->clients, (int(*)(void*,void*))cmp,
			 data, (void**)client));
}

int workspace_set_viewer(struct workspace *workspace, struct monitor *viewer)
{
	if(!workspace) {
		return(-EINVAL);
	}

	workspace->viewer = viewer;
	return(0);
}

struct monitor* workspace_get_viewer(struct workspace *workspace)
{
	return(workspace->viewer);
}

int workspace_focus_client(struct workspace *workspace, struct client *client)
{
	if(!workspace) {
		return(-EINVAL);
	}

	workspace->focused = client;
	return(0);
}

struct client* workspace_get_focused_client(struct workspace *workspace)
{
	return(workspace->focused);
}

int workspace_foreach_client(struct workspace *workspace,
			     int (*func)(struct workspace*, struct client*, void*),
			     void *data)
{
	loop_iter_t first;
	loop_iter_t cur;

	if(!workspace || !func) {
		return(-EINVAL);
	}

	first = loop_get_iter(&workspace->clients);

	if(!first) {
		return(0);
	}

	cur = first;

	do {
		struct client *client;

		client = (struct client*)loop_iter_get_data(cur);

		if(func(workspace, client, data) < 0) {
			break;
		}

		cur = loop_iter_get_next(cur);
	} while(cur != first);

	return(0);
}

int workspace_redraw(struct workspace *workspace)
{
	loop_iter_t first;
	loop_iter_t cur;

	if(!workspace) {
		return(-EINVAL);
	}

	first = loop_get_iter(&workspace->clients);

	if(first) {
		cur = first;

		do {
			struct client *client;

			client = (struct client*)loop_iter_get_data(cur);
			client_redraw(client);

			cur = loop_iter_get_next(cur);
		} while(cur != first);
	}

	workspace->needs_redraw = 0;

	return(0);
}

int workspace_needs_redraw(struct workspace *workspace)
{
	if(!workspace) {
		return(-EINVAL);
	}

	workspace->needs_redraw = 1;

	if(workspace->viewer) {
		monitor_needs_redraw(workspace->viewer);
	}

	return(0);
}

int workspace_arrange(struct workspace *workspace,
		      int (*arrange)(struct workspace*, struct client*, int, void*),
		      void *data)
{
	int unarranged_clients;
	loop_iter_t first;
	loop_iter_t cur;

	unarranged_clients = loop_get_length(&workspace->clients);
	first = loop_get_iter(&workspace->clients);

	if(!first) {
		return(0);
	}

	cur = first;

	do {
		struct client *client;

		client = (struct client*)loop_iter_get_data(cur);

		arrange(workspace, client, unarranged_clients, data);

		unarranged_clients--;
		loop_iter_inc(cur);
	} while(cur != first);

	return(0);
}

int _increase_if_tiled(struct workspace *workspace, struct client *client, void *data)
{
	int *count;

	if(!workspace || !client || !data) {
		return(-EINVAL);
	}

	count = (int*)data;

	if(client_is_tiled(client)) {
		(*count)++;
	}

	return(0);
}

int workspace_count_tiled_clients(struct workspace *workspace)
{
	int count;

	count = 0;

	workspace_foreach_client(workspace, _increase_if_tiled, &count);

	return(count);
}

int workspace_shift_focus(struct workspace *workspace, int dir)
{
	struct client *new_focus;

	if(!workspace || dir == 0) {
		return(-EINVAL);
	}

	if(dir > 0) {
		if(loop_get_next(&workspace->clients, workspace->focused,
				 (void**)&new_focus) < 0) {
			return(-EFAULT);
		}
	} else {
		if(loop_get_prev(&workspace->clients, workspace->focused,
				 (void**)&new_focus) < 0) {
			return(-EFAULT);
		}
	}

	workspace_focus_client(workspace, new_focus);
	workspace_needs_redraw(workspace);

	return(0);
}

int workspace_shift_client(struct workspace *workspace, struct client *client, int dir)
{
	struct client *shift;
	int err;

#if MWM_DEBUG
	printf("%s(%p, %p, %d)\n", __func__, (void*)workspace, (void*)client, dir);
#endif /* MWM_DEBUG */

	if(!workspace || dir == 0) {
		return(-EINVAL);
	}

	shift = client ? client : workspace->focused;

	if(!shift) {
		return(-ENOENT);
	}

	err = (dir > 0) ? loop_shift_forwards(&workspace->clients, shift) :
		loop_shift_backwards(&workspace->clients, shift);

	if(!err) {
		workspace_needs_redraw(workspace);
	}

	return(0);
}
