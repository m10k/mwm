#include "set.h"
#include <errno.h>
#include <limits.h>
#if MWM_DEBUG
#include <stdio.h>
#endif /* MWM_DEBUG */
#include <stdlib.h>
#include <string.h>

#define SET_INIT_SIZE 8

struct set {
	void **data;
	int size;
	int len;
};

int set_new(struct set **set)
{
	struct set *s;

	if (!set) {
		return -EINVAL;
	}

	if (*set) {
		return -EALREADY;
	}

	if (!(s = calloc(1, sizeof(*s)))) {
		return -ENOMEM;
	}

	s->size = SET_INIT_SIZE;
	s->len = 0;

	if (!(s->data = calloc(s->size, sizeof(*s->data)))) {
		free(s);
		return -ENOMEM;
	}

	*set = s;
	return 0;
}

/* double the size of the set */
static int _set_grow(struct set *set)
{
	int new_size;
	void **new_data;

	if (!set) {
		return -EINVAL;
	}

	if (set->size > (INT_MAX / 2)) {
		return -EOVERFLOW;
	}

	new_size = set->size * 2;
	new_data = realloc(set->data, new_size * sizeof(*set->data));

	if (!new_data) {
		return -ENOMEM;
	}

	memset(new_data + set->size, 0, sizeof(*new_data) * (new_size - set->size));

	set->data = new_data;
	set->size = new_size;

	return 0;
}

/* make the set half-size */
static int _set_shrink(struct set *set)
{
	int new_size;
	void **new_data;

	if (!set) {
		return -EINVAL;
	}

	new_size = set->size / 2;

	if (new_size < set->len) {
		/* set is more than half full */
		return -EBUSY;
	}

	new_data = realloc(set->data, new_size * sizeof(*set->data));

	if (!new_data) {
		return -ENOMEM;
	}

	memset(new_data + set->len, 0, sizeof(*new_data) * (new_size - set->len));

	set->data = new_data;
	set->size = new_size;

	return 0;
}

int set_free(struct set **set)
{
	if (!set) {
		return -EINVAL;
	}

	if (!*set) {
		return -EALREADY;
	}

	free((*set)->data);
	(*set)->data = NULL;

	free(*set);
	*set = NULL;

	return 0;
}

static int _normalize_idx(struct set *set, const int idx)
{
	int normalized;

	if (!set) {
		return -EINVAL;
	}

	if (!set->len) {
		return -ENOENT;
	}

	normalized = idx % set->len;

	while (normalized < 0) {
		normalized += set->len;
	}

	return normalized;
}

int set_real_index(struct set *set, const int index)
{
	return _normalize_idx(set, index);
}

int set_set(struct set *set, const int idx, void *data)
{
	if (!set) {
		return -EINVAL;
	}

	while (idx >= set->size) {
		int err;

		err = _set_grow(set);

		if (err < 0) {
			return err;
		}
	}

	set->data[idx] = data;
	if (set->len < (idx + 1)) {
		set->len = idx + 1;
	}

	return 0;
}

int set_unset(struct set *set, const int idx, void **data)
{
	int nidx;

	if (!set) {
		return -EINVAL;
	}

	nidx = _normalize_idx(set, idx);
	if (nidx < 0) {
		return nidx;
	}

	if (data) {
		*data = set->data[nidx];
	}
	set->data[nidx] = NULL;

	if (nidx == set->len - 1) {
		set->len--;
	}

	return 0;
}

int set_get(struct set *set, const int idx, void **data)
{
	int nidx;

	if (!set || !data) {
		return -EINVAL;
	}

	nidx = _normalize_idx(set, idx);
	if (nidx < 0) {
		return nidx;
	}

	*data = set->data[nidx];
	return 0;
}

int set_nq(struct set *set, void *data)
{
	int idx;

	if (!set) {
		return -EINVAL;
	}

	if (set->len == set->size &&
	    _set_grow(set) < 0) {
		return -ENOMEM;
	}

	idx = set->len++;
	set->data[idx] = data;
	return idx;
}

int set_dq(struct set *set, void **data)
{
	if (!set) {
		return -EINVAL;
	}

	if (set->len < 1) {
		return -ENOENT;
	}

	/* caller might pass NULL if they're not interested in the data */
	if (data) {
		*data = set->data[0];
	}

	set->len--;
	memmove(set->data, set->data + 1, sizeof(*set->data) * set->len);

	/* we might be able to release some memory */
	_set_shrink(set);

	return 0;
}

int set_pop(struct set *set, void **data)
{
	if (!set) {
		return -EINVAL;
	}

	if (set->len < 1) {
		return -ENOENT;
	}

	set->len--;

	if (data) {
		*data = set->data[set->len];
	}
	set->data[set->len] = NULL;

	/* we might be able to release some memory */
	_set_shrink(set);

	return 0;
}

int set_foreach(struct set *set, int (*func)(void*, const int, void*), void *data)
{
	int err;
	int i;

	if (!set || !func) {
		return -EINVAL;
	}

	for (err = -ENOENT, i = 0; i < set->len; i++) {
		if (!set->data[i]) {
			continue;
		}

		err = func(set->data[i], i, data);

		if (err < 0) {
			break;
		}
	}

	return err;
}

int set_search(struct set *set, int (*cmp)(void*, void*), void *data)
{
	int i;

	if (!set || !cmp) {
		return -EINVAL;
	}

	for (i = 0; i < set->len; i++) {
		if (!set->data[i]) {
			continue;
		}

		if (cmp(set->data[i], data) == 0) {
			return i;
		}
	}

	return -ENOENT;
}

#if MWM_DEBUG
void set_dump(struct set *set)
{
	fprintf(stderr,
	        "set %p\n"
	        "  data = %p\n"
	        "  size = %d\n"
	        "  len  = %d\n",
	        (void*)set, (void*)set->data, set->size, set->len);
}
#endif /* MWM_DEBUG */
