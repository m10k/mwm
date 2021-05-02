#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "array.h"

#define ARRAY_GROW_SIZE   8
#define ARRAY_SHRINK_SIZE 8

struct array {
        int size;
	void **data;
	char *flags;
};

static int _array_grow(struct array *array)
{
	int new_size;
	void **new_data;
	char *new_flags;

	if(!array) {
		return(-EINVAL);
	}

	new_size = array->size + ARRAY_GROW_SIZE;
	new_data = malloc(sizeof(*array->data) * new_size);

	if(!new_data) {
		return(-ENOMEM);
	}

	new_flags = malloc(sizeof(*array->flags) * new_size);

	if(!new_flags) {
		free(new_data);
		return(-ENOMEM);
	}

	if(array->data) {
		memcpy(new_data, array->data,
		       array->size * sizeof(*new_data));
		free(array->data);
	}
	memset(new_data + array->size, 0,
	       ARRAY_GROW_SIZE * sizeof(*new_data));

	if(array->flags) {
		memcpy(new_flags, array->flags,
		       array->size * sizeof(*new_flags));
		free(array->flags);
	}
	memset(new_flags + array->size, 0,
	       ARRAY_GROW_SIZE * sizeof(*new_flags));

	array->data = new_data;
	array->flags = new_flags;
	array->size = new_size;

	return(0);
}

static int _array_last_used(struct array *array)
{
	int last;

	if(!array) {
		return(-EINVAL);
	}

	for(last = array->size - 1; last >= 0; last--) {
		if(array->flags[last]) {
			break;
		}
	}

	return(last);
}

static int _array_shrink(struct array *array)
{
	int unused_tail;
	int new_size;
	void **new_data;
	char *new_flags;

	if(!array) {
		return(-EINVAL);
	}

	new_data = NULL;
	new_flags = NULL;

	unused_tail = array->size - _array_last_used(array);

	if(unused_tail < ARRAY_SHRINK_SIZE) {
		return(0);
	}

	unused_tail -= unused_tail % ARRAY_SHRINK_SIZE;
	new_size = array->size - unused_tail;

	if(new_size > 0) {
		new_data = malloc(new_size * sizeof(*new_data));

		if(!new_data) {
			return(-ENOMEM);
		}

		new_flags = malloc(new_size * sizeof(*new_flags));

		if(!new_flags) {
			free(new_data);
			return(-ENOMEM);
		}

		memcpy(new_data, array->data,
		       new_size * sizeof(*new_data));
		memcpy(new_flags, array->flags,
		       new_size * sizeof(*new_flags));
	}

	free(array->data);
	free(array->flags);

	array->data = new_data;
	array->flags = new_flags;
	array->size = new_size;

	return(0);
}

int array_new(struct array **array)
{
	struct array *a;

	if(!array) {
		return(-EINVAL);
	}

	a = malloc(sizeof(*a));

	if(!a) {
		return(-ENOMEM);
	}

	memset(a, 0, sizeof(*a));

	if(_array_grow(a) < 0) {
		free(a);
		return(-ENOMEM);
	}

	*array = a;
	return(0);
}

int array_free(struct array **array)
{
	if(!array) {
		return(-EINVAL);
	}

	if(!*array) {
		return(-EALREADY);
	}

	if((*array)->data) {
		free((*array)->data);
	}
	if((*array)->flags) {
		free((*array)->flags);
	}

	free(*array);
	*array = NULL;

	return(0);
}

int array_set(struct array *array, int idx, void *data)
{
	if(!array) {
		return(-EINVAL);
	}

	while(idx > array->size) {
		if(_array_grow(array) < 0) {
			return(-ENOMEM);
		}
	}

	array->data[idx] = data;
	array->flags[idx] = 1;

	return(0);
}

int array_get(struct array *array, int idx, void **data)
{
	int real_idx;

	if(!array || !data) {
		return(-EINVAL);
	}

	if(array->size == 0) {
		return(-ENODATA);
	}

	real_idx = idx % array->size;
	*data = array->data[real_idx];

	return(0);
}

int array_take(struct array *array, int idx, void **data)
{
	int real_idx;

	if(!array) {
		return(-EINVAL);
	}

	if(array->size == 0) {
		return(-ENODATA);
	}

	real_idx = idx % array->size;

	if(data) {
		*data = array->data[real_idx];
	}

	array->flags[real_idx] = 0;
	_array_shrink(array);

	return(0);
}
