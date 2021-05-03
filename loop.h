#ifndef MWM_LOOP_H
#define MWM_LOOP_H 1

struct loop;

int loop_append(struct loop **loop, void *data);
int loop_prepend(struct loop **loop, void *data);
int loop_remove(struct loop **loop, void *data);
int loop_find(struct loop **loop, int(*cmp)(void*, void*), void *data, void **dst);
int loop_free(struct loop **loop);

#endif /* MWM_LOOP_H */
