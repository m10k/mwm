#ifndef MWM_LOOP_H
#define MWM_LOOP_H 1

struct loop;
typedef struct loop* loop_iter_t;

int loop_append(struct loop **loop, void *data);
int loop_prepend(struct loop **loop, void *data);
int loop_remove(struct loop **loop, void *data);
int loop_find(struct loop **loop, int(*cmp)(void*, void*), void *data, void **dst);
int loop_foreach(struct loop **loop, void(*func)(void*));
int loop_foreach_with_data(struct loop **loop, int(*func)(void*, void*), void*);
int loop_free(struct loop **loop);
int loop_get_first(struct loop **loop, void**);
int loop_get_last(struct loop **loop, void**);
int loop_get_length(struct loop **loop);
int loop_get_next(struct loop **loop, void *data, void **next);
int loop_get_prev(struct loop **loop, void *data, void **next);
int loop_shift_backwards(struct loop **loop, void *data);
int loop_shift_forwards(struct loop **loop, void *data);

loop_iter_t loop_get_iter(struct loop **loop);
loop_iter_t loop_iter_get_next(loop_iter_t iter);
loop_iter_t loop_iter_get_prev(loop_iter_t iter);
void* loop_iter_get_data(loop_iter_t iter);

#define loop_iter_inc(iter) ((iter) = loop_iter_get_next((iter)))
#define loop_iter_dec(iter) ((iter) = loop_iter_get_prev((iter)))

#endif /* MWM_LOOP_H */
