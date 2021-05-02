#ifndef ARRAY_H
#define ARRAY_H

struct array;

int array_new(struct array **array);
int array_free(struct array **array);

int array_set(struct array*, int, void*);
int array_get(struct array*, int, void**);
int array_take(struct array*, int, void**);

#endif /* ARRAY_H */
