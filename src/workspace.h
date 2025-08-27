#ifndef MWM_WORKSPACE_H
#define MWM_WORKSPACE_H 1

#include "client.h"

struct monitor;
struct workspace;

int workspace_new(const int number, struct workspace **workspace);
int workspace_free(struct workspace **workspace);
int workspace_get_number(struct workspace *workspace);

int workspace_attach_client(struct workspace *workspace,
                            const client_t client);
int workspace_detach_client(struct workspace *workspace,
                            const client_t client);

int workspace_find_client(struct workspace *workspace,
                          int (*cmp)(client_t, void*),
                          void *data, client_t *client);

int workspace_focus_client(struct workspace *workspace,
                           const client_t client);
client_t workspace_get_focused_client(struct workspace *workspace);

int workspace_set_viewer(struct workspace *workspace, struct monitor *viewer);
struct monitor* workspace_get_viewer(struct workspace *workspace);

int workspace_count_clients(struct workspace *workspace);
int workspace_foreach_client(struct workspace *workspace,
                             int (*func)(struct workspace*, const client_t, void*),
                             void *data);

int workspace_needs_redraw(struct workspace *workspace);
int workspace_redraw(struct workspace *workspace);

int workspace_shift_focus(struct workspace *workspace, int dir);
int workspace_shift_client(struct workspace *workspace, const client_t client, int dir);

#if MWM_DEBUG
void workspace_dump(struct workspace *workspace);
#endif /* MWM_DEBUG */

#endif /* MWM_WORKSPACE_H */
