#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "loop.h"

struct loop {
        struct loop *prev;
        struct loop *next;
        void *data;
};

int loop_append(struct loop **loop, void *data)
{
	struct loop *new;

	if(!loop) {
		return(-EINVAL);
	}

	new = malloc(sizeof(*new));

	if(!new) {
		return(-ENOMEM);
	}

	new->data = data;
	new->prev = new;
	new->next = new;

        if(*loop) {
		new->prev = (*loop)->prev;
		(*loop)->prev = new;
		new->prev->next = new;
		new->next = *loop;
	} else {
		*loop = new;
	}

	return(0);
}

int loop_prepend(struct loop **loop, void *data)
{
	struct loop *new;

	if(!loop) {
		return(-EINVAL);
	}

	new = malloc(sizeof(*new));

	if(!new) {
		return(-ENOMEM);
	}

	new->data = data;
	new->prev = new;
	new->next = new;

	if(*loop) {
		new->prev = (*loop)->prev;
		(*loop)->prev = new;
		new->prev->next = new;
		new->next = *loop;
	}

	*loop = new;

	return(0);
}

int loop_remove(struct loop **loop, void *data)
{
	struct loop *elem;

	if(!loop) {
		return(-EINVAL);
	}

	if(loop_find(loop, NULL, data, (void**)&elem) < 0) {
		return(-ENOENT);
	}

	elem->next->prev = elem->prev;
	elem->prev->next = elem->next;

	if(*loop == elem) {
	        if(elem == elem->prev) {
			*loop = NULL;
		} else {
			*loop = elem->prev;
		}
	}

	free(elem);

	return(0);
}

int _cmp_ptr(void *left, void *right)
{
	return(left == right);
}

int loop_find(struct loop **loop, int (*cmp)(void*, void*), void *data, void **dst)
{
	struct loop *cur;

	if(!loop) {
		return(-EINVAL);
	}

	if(!*loop) {
		return(-ENOENT);
	}

	if(!cmp) {
		cmp = _cmp_ptr;
	}
	cur = *loop;

        do {
                if(cmp(cur->data, data) == 0) {
			*dst = data;
			return(0);
                }

                cur = cur->next;
        } while(cur != *loop);

	return(-ENOENT);
}

int loop_free(struct loop **loop)
{
	if(!loop) {
		return(-EINVAL);
	}

	while(*loop) {
		struct loop *next;

		next = (*loop)->next;
		free(*loop);

		*loop = (*loop == next) ? NULL : next;
	}

	return(0);
}
