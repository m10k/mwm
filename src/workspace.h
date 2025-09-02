#ifndef MWM_WORKSPACE_H
#define MWM_WORKSPACE_H 1

#include "common.h"
#include "client.h"
#include "monitor.h"

workspace_t workspace_new(const int number);
int workspace_free(const workspace_t workspace_id);
int workspace_get_number(const workspace_t workspace_id, int *number);

int workspace_attach_client(const workspace_t workspace_id,
                            const client_t client);
int workspace_detach_client(const workspace_t workspace_id,
                            const client_t client);

int workspace_find_client(const workspace_t workspace_id,
                          int (*cmp)(client_t, void*),
                          void *data, client_t *client);

int workspace_focus_client(const workspace_t workspace_id,
                           const client_t client);
int workspace_get_focused_client(const workspace_t workspace_id, client_t *client);

int workspace_set_viewer(const workspace_t workspace_id, const monitor_t monitor);
int workspace_get_viewer(const workspace_t workspace_id, monitor_t *viewer);

int workspace_count_clients(const workspace_t workspace_id);
int workspace_foreach_client(const workspace_t workspace_id,
                             int (*func)(const workspace_t, const client_t, void*),
                             void *data);

int workspace_needs_redraw(const workspace_t workspace_id);
int workspace_redraw(const workspace_t workspace_id);

int workspace_shift_focus(const workspace_t workspace_id, int dir);
int workspace_shift_client(const workspace_t workspace_id, const client_t client, int dir);

int workspace_foreach(int(*func)(const workspace_t, void*), void *data);

workspace_t workspace_number(const int number);
workspace_t workspace_unviewed(void);

#if MWM_DEBUG
int workspace_dump(const workspace_t workspace_id);
#endif /* MWM_DEBUG */

#endif /* MWM_WORKSPACE_H */
