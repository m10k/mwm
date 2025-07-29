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

	struct {
		struct client *current;
		struct client *next;
		int changed;
	} focus;

	struct {
		struct monitor *current;
		struct monitor *next;
		int changed;
	} viewer;

	int number;
	int needs_redraw;
};

int workspace_new(const int number, struct workspace **workspace)
{
	struct workspace *wspace;

	if(!workspace) {
		return(-EINVAL);
	}

	if (!(wspace = calloc(1, sizeof(*wspace)))) {
		return -ENOMEM;
	}

	wspace->number = number;

	*workspace = wspace;

	return(0);
}

int workspace_free(struct workspace **workspace)
{
	if (!workspace) {
		return -EINVAL;
	}

	if (!*workspace) {
		return -EALREADY;
	}

	free(*workspace);
	*workspace = NULL;

	return 0;
}

int workspace_get_number(struct workspace *workspace)
{
	if (!workspace) {
		return -EINVAL;
	}

	return workspace->number;
}

int workspace_attach_client(struct workspace *workspace, struct client *client)
{
	if (!workspace || !client) {
		return -EINVAL;
	}

	if (loop_append(&workspace->clients, client) < 0) {
		return -ENOMEM;
	}

	client_set_workspace(client, workspace);

	if (!workspace_get_focused_client(workspace)) {
		workspace_focus_client(workspace, client);
	}

	workspace_needs_redraw(workspace);

	return 0;
}


int workspace_detach_client(struct workspace *workspace, struct client *client)
{
#if MWM_DEBUG
	fprintf(stderr, "%s(%p, %p)\n", __func__, (void*)workspace, (void*)client);
#endif /* MWM_DEBUG */

	if (!workspace || !client) {
		return -EINVAL;
	}

	if (workspace->focus.next == client) {
		/* don't change the focus if this client was supposed to be focused */
		workspace->focus.next = NULL;
		workspace->focus.changed = 0;
	}

	if (workspace->focus.current == client) {
		struct client *next;
		int err;

		/* if the client was focused, figure out who gets the focus next */

		err = loop_get_next(&workspace->clients, client, (void**)&next);

		if (err < 0) {
			/*
			 * At the *very least* we should have gotten a pointer to client itself.
			 * If we didn't even get that, client wasn't on this workspace and we've
			 * encountered a bug.
			 */
			fprintf(stderr, "%s: BUG: Could not determine next client\n", __func__);
			fprintf(stderr, "%s: loop_get_next: %s\n", __func__, strerror(-err));

			return err;
		}

		err = loop_remove(&workspace->clients, client);

		if (err < 0) {
			/*
			 * If client wasn't on this workspace, the branch above should have been
			 * visited. So if we get here, something even nastier is going on.
			 */
			fprintf(stderr, "%s: BUG: Client %p was not on this workspace.\n",
			        __func__, (void*)client);
			fprintf(stderr, "%s: loop_remove: %s\n", __func__, strerror(-err));

			return err;
		}

		if (next == client) {
			/* nothing left to focus on */
			next = NULL;
		}

		workspace_focus_client(workspace, next);
	}

	workspace_needs_redraw(workspace);

	return 0;
}

int workspace_find_client(struct workspace *workspace,
			  int (*cmp)(struct client*, void*),
			  void *data, struct client **client)
{
	if (!workspace) {
		return -EINVAL;
	}

	return loop_find(&workspace->clients, (int(*)(void*,void*))cmp,
			 data, (void**)client);
}

int workspace_set_viewer(struct workspace *workspace, struct monitor *viewer)
{
	if (!workspace) {
		return -EINVAL;
	}

	if (workspace->viewer.current == viewer) {
		return -EALREADY;
	}

	workspace->viewer.next = viewer;
	workspace->viewer.changed = 1;

	return 0;
}

struct monitor* workspace_get_viewer(struct workspace *workspace)
{
	return workspace->viewer.current;
}

int workspace_focus_client(struct workspace *workspace, struct client *client)
{
	if (!workspace) {
		return -EINVAL;
	}

	if (workspace->focus.current == client) {
		return -EALREADY;
	}

	workspace->focus.next = client;
	workspace->focus.changed = 1;
	workspace_needs_redraw(workspace);

	return 0;
}

struct client* workspace_get_focused_client(struct workspace *workspace)
{
	return workspace->focus.current;
}

struct workspace_foreach_client_args {
	int (*func)(struct workspace*, struct client*, void*);
	struct workspace *workspace;
	void *data;
};

static int _workspace_foreach_client_call(struct client *client,
                                          struct workspace_foreach_client_args *args)
{
	return args->func(args->workspace, client, args->data);
}

int workspace_foreach_client(struct workspace *workspace,
			     int (*func)(struct workspace*, struct client*, void*),
			     void *data)
{
	struct workspace_foreach_client_args args;

	if (!workspace || !func) {
		return -EINVAL;
	}

	args.func = func;
	args.workspace = workspace;
	args.data = data;

	return loop_foreach_with_data(&workspace->clients,
	                              (int(*)(void*, void*))_workspace_foreach_client_call,
	                              &args);
}

int workspace_redraw(struct workspace *workspace)
{
	if (!workspace) {
		return -EINVAL;
	}

	if (workspace->viewer.changed) {
		workspace->viewer.current = workspace->viewer.next;
	}
	if (workspace->focus.changed) {
		workspace->focus.current = workspace->focus.next;
	}

	if(workspace->needs_redraw) {
		loop_foreach(&workspace->clients, (void(*)(void*))client_redraw);
	}

	workspace->viewer.changed = 0;
	workspace->focus.changed = 0;
	workspace->needs_redraw = 0;

	return 0;
}

int workspace_needs_redraw(struct workspace *workspace)
{
	if (!workspace) {
		return -EINVAL;
	}

	workspace->needs_redraw = 1;

	monitor_needs_redraw(workspace->viewer.current);
	if (workspace->viewer.changed) {
		monitor_needs_redraw(workspace->viewer.next);
	}

	return 0;
}

int workspace_count_clients(struct workspace *workspace)
{
	return loop_get_length(&workspace->clients);
}

int workspace_shift_focus(struct workspace *workspace, int dir)
{
	struct client *old_focus;
	struct client *new_focus;

	if (!workspace || dir == 0) {
		return -EINVAL;
	}

	if (dir > 0) {
		if (loop_get_next(&workspace->clients, workspace->focus.current,
				 (void**)&new_focus) < 0) {
			return -EFAULT;
		}
	} else {
		if (loop_get_prev(&workspace->clients, workspace->focus.current,
				 (void**)&new_focus) < 0) {
			return -EFAULT;
		}
	}

	if ((old_focus = workspace_get_focused_client(workspace))) {
		client_save_pointer(old_focus);
	}

	workspace_focus_client(workspace, new_focus);
	workspace_needs_redraw(workspace);

	return 0;
}

int workspace_shift_client(struct workspace *workspace, struct client *client, int dir)
{
	struct client *shift;
	int err;

#if MWM_DEBUG
	printf("%s(%p, %p, %d)\n", __func__, (void*)workspace, (void*)client, dir);
#endif /* MWM_DEBUG */

	if (!workspace || dir == 0) {
		return -EINVAL;
	}

	shift = client ? client : workspace->focus.current;

	if (!shift) {
		return -ENOENT;
	}

	err = (dir > 0) ? loop_shift_forwards(&workspace->clients, shift) :
		loop_shift_backwards(&workspace->clients, shift);

	if (!err) {
		workspace_needs_redraw(workspace);
	}

	return 0;
}

#if MWM_DEBUG
void workspace_dump(struct workspace *workspace)
{
	fprintf(stderr,
	        "  Workspace %p\n"
	        "    Number:          %d\n"
	        "    Current focus:   %p\n"
	        "    Next focus:      %p\n"
	        "    Current monitor: %p\n"
	        "    Next monitor:    %p\n"
	        "    Needs redraw:    %d\n",
	        (void*)workspace,
	        workspace->number,
	        (void*)workspace->focus.current,
	        (void*)workspace->focus.next,
	        (void*)workspace->viewer.current,
	        (void*)workspace->viewer.next,
	        workspace->needs_redraw);

	loop_foreach(&workspace->clients, (void(*)(void*))client_dump);
}
#endif /* MWM_DEBUG */
