#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include "common.h"
#include "workspace.h"
#include "monitor.h"
#include "client.h"
#include "loop.h"
#include "set.h"

struct workspace {
	client_t *clients;
	int num_clients;

	struct {
	        int current;
	        int next;
		int changed;
	} focus;

	struct {
	        monitor_t current;
	        monitor_t next;
		int changed;
	} viewer;

	int number;
	int needs_redraw;
	workspace_t id;
};

static struct set *_workspaces = NULL;

static inline int __init(void)
{
	return _workspaces ? 0 : set_new(&_workspaces);
}

static inline int __get_workspace(struct workspace **workspace, const workspace_t workspace_id)
{
	int err;

	if (workspace_id < 0) {
		return -EINVAL;
	}

	if ((err = set_get(_workspaces, workspace_id, (void**)workspace)) < 0) {
		return err;
	}

	if (!*workspace) {
		return -EBADF;
	}

	return 0;
}

static int _get_client_idx(struct workspace *workspace, const client_t client)
{
	int idx;

	if (!workspace || client < 0) {
		return -EINVAL;
	}

	for (idx = 0; idx < workspace->num_clients; idx++) {
		if (workspace->clients[idx] == client) {
			return idx;
		}
	}

	return -ENOENT;
}

workspace_t workspace_new(const int number)
{
	struct workspace *wspace;
	int err;

	if ((err = __init()) < 0) {
		return err;
	}

	if (!(wspace = calloc(1, sizeof(*wspace)))) {
		return -ENOMEM;
	}

	wspace->focus.current = -1;
	wspace->focus.next = -1;
	wspace->viewer.current = -1;
	wspace->viewer.next = -1;
	wspace->number = number;

	if ((err = set_nq(_workspaces, wspace)) < 0) {
		free(wspace);
	} else {
	        wspace->id = err;
	}

	return err;
}

int workspace_free(const workspace_t workspace_id)
{
	struct workspace *workspace;
	int err;

	if ((err = __get_workspace(&workspace, workspace_id)) < 0) {
		return err;
	}

	free(workspace->clients);
	free(workspace);

	return 0;
}

int workspace_get_number(workspace_t workspace_id, int *number)
{
	struct workspace *workspace;
	int err;

	if (!number) {
		return -EINVAL;
	}

	if ((err = __get_workspace(&workspace, workspace_id)) < 0) {
		return err;
	}

	*number = workspace->number;
	return 0;
}

static int _client_array_insert(struct workspace *workspace, const client_t client)
{
	client_t *new_clients;
	int new_num_clients;

	if (workspace->num_clients == INT_MAX) {
		return -EOVERFLOW;
	}

	new_num_clients = workspace->num_clients + 1;

	if ((SIZE_MAX / sizeof(client_t)) < new_num_clients) {
		return -EOVERFLOW;
	}

	if (!(new_clients = realloc(workspace->clients,
	                            sizeof(client_t) * new_num_clients))) {
		return -ENOMEM;
	}

	/*
	 * TODO: Add configuration for insertion position
	 *  - Insertion could be at the end or behind the current focus
	 *  - In the latter case, the focus needs to be updated, since
	 *    it is an index into `clients`
	 */
	new_clients[new_num_clients - 1] = client;

	workspace->clients = new_clients;
	workspace->num_clients = new_num_clients;

	return 0;
}

static int _client_array_remove(struct workspace *workspace, const client_t client)
{
	client_t *new_clients;
	int new_num_clients;
	int src_idx;
	int dst_idx;
	int rem_idx;

	if (workspace->num_clients <= 0) {
		return -ENOENT;
	}

	new_num_clients = workspace->num_clients - 1;

	if (!(new_clients = malloc(new_num_clients * sizeof(client_t)))) {
		return -ENOMEM;
	}

	for (src_idx = dst_idx = 0, rem_idx = -1; src_idx < workspace->num_clients; src_idx++) {
		if (workspace->clients[src_idx] == client) {
			rem_idx = src_idx;
			continue;
		}

		new_clients[dst_idx] = workspace->clients[src_idx];
		dst_idx++;
	}

	if (rem_idx < 0) {
		free(new_clients);
		return -ENOENT;
	}

	/*
	 * Case 1: The focus is an index into the client array. If we removed a client in
	 * front of the focused client, we need to adjust the focus.
	 *
	 *           +---+---+---+---+                               +---+---+---+
	 * clients = | 6 | 0 | 4 | 1 |  (Remove client 6)  clients = | 0 | 4 | 1 |
	 *           +---+---+---+---+  ================>            +---+---+---+
	 *                 ^                                           ^
	 *                 |                                           |
	 * focus (1) ------+                               focus (0) --+
	 *
	 * Case 2: If the focused client is removed, the next client "automatically" receives
	 * the focus.
	 *
	 *           +---+---+---+                               +---+---+
	 * clients = | 0 | 4 | 1 |  (Remove client 0)  clients = | 4 | 1 |
	 *           +---+---+---+  ================>            +---+---+
	 *             ^                                           ^
	 *             |                                           |
	 * focus (0) --+                               focus (0) --+
	 *
	 * Case 3: If the client at the end is removed, we do have to adjust the focus.
	 *
	 *           +---+---+---+                               +---+---+
	 * clients = | 0 | 4 | 1 |  (Remove client 1)  clients = | 0 | 4 |
	 *           +---+---+---+  ================>            +---+---+
	 *                     ^                                       ^
	 *                     |                                       |
	 * focus (2) ----------+                       focus (1) ------+
	 *
	 */

#define IS_LAST(index, array_size)                     ((index + 1) == array_size)
#define IS_CASE1(focused_idx, removed_idx)             (focused_idx > removed_idx)
#define IS_CASE3(focused_idx, removed_idx, array_size) (focused_idx == removed_idx && \
                                                        IS_LAST(removed_idx, array_size))

	if (IS_CASE1(workspace->focus.current, rem_idx) ||
	    IS_CASE3(workspace->focus.current, rem_idx, workspace->num_clients)) {
		workspace->focus.current--;
	}
	if (IS_CASE1(workspace->focus.next, rem_idx)) {
		workspace->focus.next--;
	} else if (workspace->focus.next == rem_idx) {
		/* if the to-be-focused client was removed, don't change the focus */
		workspace->focus.next = -1;
		workspace->focus.changed = 0;
	}

	free(workspace->clients);
	workspace->clients = new_clients;
	workspace->num_clients = new_num_clients;

#undef IS_LAST
#undef IS_CASE1
#undef IS_CASE3

	return 0;
}

int workspace_attach_client(const workspace_t workspace_id, const client_t client)
{
	struct workspace *workspace;
	client_t focused;
	int err;

	if (client < 0) {
		return -EINVAL;
	}

	if ((err = __get_workspace(&workspace, workspace_id)) < 0) {
		return err;
	}

	if ((err = _client_array_insert(workspace, client)) < 0) {
		return err;
	}

	client_set_workspace(client, workspace_id);

	if (workspace_get_focused_client(workspace_id, &focused) < 0 ||
	    focused < 0) {
		workspace_focus_client(workspace_id, client);
	}

	workspace_needs_redraw(workspace_id);

	return 0;
}


int workspace_detach_client(const workspace_t workspace_id, const client_t client)
{
	struct workspace *workspace;
	int err;

#if MWM_DEBUG
	fprintf(stderr, "%s(%ld, %ld)\n", __func__, workspace_id, client);
#endif /* MWM_DEBUG */

	if (client < 0) {
		return -EINVAL;
	}

	if ((err = __get_workspace(&workspace, workspace_id)) < 0) {
		return err;
	}

	if ((err = _client_array_remove(workspace, client)) < 0) {
		return err;
	}

	workspace_needs_redraw(workspace_id);

	return 0;
}

int workspace_find_client(const workspace_t workspace_id,
			  int (*cmp)(const client_t, void*),
			  void *data, client_t *client)
{
	struct workspace *workspace;
	int err;
	int i;

	if (!cmp) {
		return -EINVAL;
	}

	if ((err = __get_workspace(&workspace, workspace_id)) < 0) {
		return err;
	}

	for (i = 0; i < workspace->num_clients; i++) {
		if (cmp(workspace->clients[i], data) == 0) {
			if (client) {
				*client = workspace->clients[i];
			}
			return 0;
		}
	}

	return -ENOENT;
}

int workspace_set_viewer(const workspace_t workspace_id, const monitor_t viewer)
{
	struct workspace *workspace;
	int err;

	if ((err = __get_workspace(&workspace, workspace_id)) < 0) {
		return err;
	}

	if (workspace->viewer.current == viewer) {
		return -EALREADY;
	}

	workspace->viewer.next = viewer;
	workspace->viewer.changed = 1;

	return 0;
}

int workspace_get_viewer(const workspace_t workspace_id, monitor_t *viewer)
{
	struct workspace *workspace;
	int err;

	if (!viewer) {
		return -EINVAL;
	}

	if ((err = __get_workspace(&workspace, workspace_id)) < 0) {
		return err;
	}

	*viewer = workspace->viewer.current;

	return 0;
}

int workspace_focus_client(const workspace_t workspace_id, const client_t client)
{
	struct workspace *workspace;
	int err;
	int client_idx;

	if ((err = __get_workspace(&workspace, workspace_id)) < 0) {
		return err;
	}

	if ((client_idx = _get_client_idx(workspace, client)) < 0) {
		return err;
	}

	if (workspace->focus.current == client_idx) {
		/* we're staying on the current workspace */
		if (workspace->focus.next != workspace->focus.current) {
			workspace->focus.next = -1;
			workspace->focus.changed = 0;
		}

		return -EALREADY;
	}

	workspace->focus.next = client_idx;
	workspace->focus.changed = 1;
	workspace_needs_redraw(workspace_id);

	return 0;
}

int workspace_get_focused_client(const workspace_t workspace_id, client_t *client)
{
	struct workspace *workspace;
	int err;

	if (!client) {
		return -EINVAL;
	}

	if ((err = __get_workspace(&workspace, workspace_id)) < 0) {
		return err;
	}

	if (workspace->focus.current < 0) {
		return -ENOENT;
	}

	if (workspace->focus.current >= workspace->num_clients) {
		return -EBADFD;
	}

	*client = workspace->clients[workspace->focus.current];
	return 0;
}

int workspace_foreach_client(const workspace_t workspace_id,
			     int (*func)(const workspace_t, const client_t, void*),
			     void *data)
{
	struct workspace *workspace;
	int err;
	int i;

	if (!func) {
		return -EINVAL;
	}

	if ((err = __get_workspace(&workspace, workspace_id)) < 0) {
		return err;
	}

	for (i = 0; i < workspace->num_clients; i++) {
		if (workspace->clients[i] < 0) {
			continue;
		}

		err = func(workspace_id, workspace->clients[i], data);

		if (err < 0) {
			break;
		}
	}

	return err;
}

int workspace_redraw(const workspace_t workspace_id)
{
	struct workspace *workspace;
	int err;

	if ((err = __get_workspace(&workspace, workspace_id)) < 0) {
		return err;
	}

	if (workspace->viewer.changed) {
		workspace->viewer.current = workspace->viewer.next;
	}
	if (workspace->focus.changed) {
		workspace->focus.current = workspace->focus.next;
	}

	if(workspace->needs_redraw) {
		int i;

		for (i = 0; i < workspace->num_clients; i++) {
			client_redraw(workspace->clients[i]);
		}
	}

	workspace->viewer.changed = 0;
	workspace->focus.changed = 0;
	workspace->needs_redraw = 0;

	return 0;
}

int workspace_needs_redraw(const workspace_t workspace_id)
{
	struct workspace *workspace;
	int err;

	if ((err = __get_workspace(&workspace, workspace_id)) < 0) {
		return err;
	}

	workspace->needs_redraw = 1;

	monitor_needs_redraw(workspace->viewer.current);
	if (workspace->viewer.changed) {
		monitor_needs_redraw(workspace->viewer.next);
	}

	return 0;
}

int workspace_count_clients(const workspace_t workspace_id)
{
	struct workspace *workspace;
	int err;

	if ((err = __get_workspace(&workspace, workspace_id)) < 0) {
		return err;
	}

	return workspace->num_clients;
}

int workspace_shift_focus(const workspace_t workspace_id, int dir)
{
	struct workspace *workspace;
	int err;
	int old_focus;
	int new_focus;

	if ((err = __get_workspace(&workspace, workspace_id)) < 0) {
		return err;
	}

	if (workspace->num_clients < 1) {
		return -EBADFD;
	}

	old_focus = workspace->focus.current;
	new_focus = (old_focus + dir) % workspace->num_clients;
	while (new_focus < 0) {
		new_focus += workspace->num_clients;
	}

	/* FIXME: Check if pointer is over the client */
	client_save_pointer(workspace->clients[old_focus]);

	workspace_focus_client(workspace_id, workspace->clients[new_focus]);
	workspace_needs_redraw(workspace_id);

	return 0;
}

int workspace_shift_client(const workspace_t workspace_id, const client_t client, int dir)
{
	struct workspace *workspace;
	client_t swap;
	int shift_src;
	int shift_dst;
	int err;

#if MWM_DEBUG
	printf("%s(%ld, %ld, %d)\n", __func__, workspace_id, client, dir);
#endif /* MWM_DEBUG */

	if ((err = __get_workspace(&workspace, workspace_id)) < 0) {
		return err;
	}

	if (workspace->num_clients < 1) {
		return -ENOENT;
	}

	if (client >= 0) {
		shift_src = _get_client_idx(workspace, client);
	} else {
		shift_src = workspace->focus.current;
	}
	fprintf(stderr, "%s: Shifting client #%d (id %ld)\n", __func__, shift_src, workspace->clients[shift_src]);

	if (shift_src < 0) {
		return shift_src;
	}
	shift_dst = (shift_src + dir) % workspace->num_clients;
	while (shift_dst < 0) {
		shift_dst += workspace->num_clients;
	}

	fprintf(stderr, "%s: Swapping clients #%d and #%d (id %ld and %ld)\n",
	        __func__, shift_src, shift_dst, workspace->clients[shift_src], workspace->clients[shift_dst]);

	swap = workspace->clients[shift_dst];
	workspace->clients[shift_dst] = workspace->clients[shift_src];
	workspace->clients[shift_src] = swap;
	if (shift_src == workspace->focus.current) {
		workspace->focus.current = shift_dst;
		fprintf(stderr, "%s: Updating workspace->focus.current to %d\n", __func__, shift_dst);
	}
	workspace_needs_redraw(workspace_id);

	fprintf(stderr, "%s: workspace->focus\n"
	        "  .current = %d\n"
	        "  .next    = %d\n"
	        "  .changed = %d\n",
	        __func__, workspace->focus.current, workspace->focus.next, workspace->focus.changed);

	return 0;
}

struct _workspace_call_args {
	int (*func)(const workspace_t, void*);
	void *data;
};

int _workspace_call(struct workspace *workspace,
                    const int idx,
                    struct _workspace_call_args *args)
{
	return args->func((workspace_t)idx, args->data);
}

int workspace_foreach(int (*func)(const workspace_t, void*), void *data)
{
	struct _workspace_call_args args;

	args.func = func;
	args.data = data;

	return set_foreach(_workspaces,
	                   (int(*)(void*, const int, void*))_workspace_call,
	                   &args);
}

static int _cmp_workspace_number(const struct workspace *workspace, const int *number)
{
	return workspace->number - *number;
}

static int _cmp_workspace_viewer(const struct workspace *workspace, monitor_t *viewer)
{
	return workspace->viewer.current - *viewer;
}

workspace_t workspace_number(const int number)
{
	return set_search(_workspaces, (int(*)(void*, void*))_cmp_workspace_number, (void*)&number);
}

workspace_t workspace_unviewed(void)
{
	monitor_t none;

	none = -1;

	return set_search(_workspaces, (int(*)(void*, void*))_cmp_workspace_viewer, (void*)&none);
}

#if MWM_DEBUG
int workspace_dump(const workspace_t workspace_id)
{
	struct workspace *workspace;
	int err;
	int i;

	if ((err = __get_workspace(&workspace, workspace_id)) < 0) {
		fprintf(stderr, "  Workspace %ld INVALID: %s\n", workspace_id, strerror(-err));
		return err;
	}

	fprintf(stderr,
	        "  Workspace %ld @ %p\n"
	        "    Number:          %d\n"
	        "    Current focus:   %d [client %ld]\n"
	        "    Next focus:      %d [client %ld]\n"
	        "    Current viewer:  %ld\n"
	        "    Next viewer:     %ld\n"
	        "    Needs redraw:    %d\n",
	        workspace_id, (void*)workspace,
	        workspace->number,
	        workspace->focus.current, workspace->clients[workspace->focus.current],
	        workspace->focus.next, workspace->clients[workspace->focus.next],
	        workspace->viewer.current,
	        workspace->viewer.next,
	        workspace->needs_redraw);

	for (i = 0; i < workspace->num_clients; i++) {
		client_dump(workspace->clients[i]);
	}

	return 0;
}
#endif /* MWM_DEBUG */
