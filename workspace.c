#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "workspace.h"
#include "client.h"
#include "loop.h"

struct workspace {
	struct loop *clients;
	struct client *focused;

	int number;
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

int workspace_add_client(struct workspace *workspace, struct client *client)
{
	if(!workspace || !client) {
		return(-EINVAL);
	}

	return(loop_append(&workspace->clients, client));
}


int workspace_remove_client(struct workspace *workspace, struct client *client)
{
	if(!workspace || !client) {
		return(-EINVAL);
	}

	return(loop_remove(&workspace->clients, client));
}
