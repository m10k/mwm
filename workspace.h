#ifndef MWM_WORKSPACE_H
#define MWM_WORKSPACE_H 1

struct client;
struct workspace;

int workspace_new(const int number, struct workspace **workspace);
int workspace_free(struct workspace **workspace);

int workspace_add_client(struct workspace *workspace,
			 struct client *client);
int workspace_remove_client(struct workspace *workspace,
			    struct client *client);

#endif /* MWM_WORKSPACE_H */
