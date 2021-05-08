#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "loop.h"

struct loop {
        struct loop *prev;
        struct loop *next;
        void *data;
};

static int _cmp_ptr(void *left, void *right)
{
	return(left == right ? 0 : 1);
}

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

int __loop_find(struct loop **loop,
		int (*cmp)(void*, void*),
		void *data,
		struct loop **iter)
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
			if(iter) {
				*iter = cur;
			}
			return(0);
		}

		cur = cur->next;
	} while(cur != *loop);

	return(-ENOENT);
}

int loop_remove(struct loop **loop, void *data)
{
	struct loop *elem;

	if(!loop) {
		return(-EINVAL);
	}

	if(__loop_find(loop, NULL, data, &elem) < 0) {
		return(-ENOENT);
	}

	elem->next->prev = elem->prev;
	elem->prev->next = elem->next;

	if(*loop == elem) {
	        if(elem == elem->next) {
			*loop = NULL;
		} else {
			*loop = elem->next;
		}
	}

	free(elem);

	return(0);
}

int loop_find(struct loop **loop, int (*cmp)(void*, void*),
	      void *data, void **dst)
{
	struct loop *elem;
	int err;

	err = __loop_find(loop, cmp, data, &elem);

	if(!err) {
		if(dst) {
			*dst = elem->data;
		}
	}

	return(err);
}

int loop_foreach(struct loop **loop, int(*func)(void*, void*),
		 void *data)
{
	struct loop *cur;

	if(!loop) {
		return(-EINVAL);
	}

	cur = *loop;

	while(cur) {
		if(func(cur->data, data) < 0) {
			break;
		}

		cur = cur->next;

		if(cur == *loop) {
			break;
		}
	}

	return(0);
}

int loop_free(struct loop **loop)
{
	if(!loop) {
		return(-EINVAL);
	}

	while(*loop) {
		struct loop *next;

		(*loop)->prev->next = NULL;
		next = (*loop)->next;

		free(*loop);

		*loop = next;
	}

	return(0);
}

int loop_get_first(struct loop **loop, void **data)
{
	if(!loop || !data) {
		return(-EINVAL);
	}

	if(!*loop) {
		return(-ENOENT);
	}

	*data = (*loop)->data;
	return(0);
}

int loop_get_last(struct loop **loop, void **data)
{
	if(!loop || !data) {
		return(-EINVAL);
	}

	if(!*loop) {
		return(-ENOENT);
	}

	*data = (*loop)->prev->data;
	return(0);
}

loop_iter_t loop_get_iter(struct loop **loop)
{
	if(!loop) {
		return(NULL);
	}

	return(*loop);
}

loop_iter_t loop_iter_get_next(loop_iter_t iter)
{
	return(iter->next);
}

loop_iter_t loop_iter_get_prev(loop_iter_t iter)
{
	return(iter->prev);
}

void* loop_iter_get_data(loop_iter_t iter)
{
	return(iter->data);
}

int loop_get_length(struct loop **loop)
{
	struct loop *cur;

	int length;

	if(!loop) {
		return(-EINVAL);
	}

	if(!*loop) {
		return(0);
	}

	length = 0;
	cur = *loop;

	do {
		length++;
		cur = cur->next;
	} while(cur != *loop);

	return(length);
}
