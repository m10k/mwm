#ifndef SET_H
#define SET_H

struct set;

int set_new(struct set **set);
int set_free(struct set **set);

int set_real_index(struct set *set, const int index);

/* array operations */
int set_set(struct set *set, const int idx, void *data);
int set_unset(struct set *set, const int idx, void **data);
int set_get(struct set *set, const int idx, void **data);

/* queue operations */
int set_nq(struct set *set, void *data);
int set_dq(struct set *set, void **data);

/* stack operations */
#define set_push set_nq
int set_pop(struct set *set, void **data);

int set_foreach(struct set *set, int (*func)(void*, const int, void*), void *data);
int set_search(struct set *set, int (*cmp)(void*, void*), void *data);

#if MWM_DEBUG
void set_dump(struct set *set);
#endif /* MWM_DEBUG */

#endif /* SET_H */
