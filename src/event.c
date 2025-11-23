#include "event.h"
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>

#define QLEN  64
#define QMASK (QLEN - 1)

static struct event *eventq[64];
static struct {
	int nq;
	int dq;
} idx = { 0, 0 };

int event_new(struct event **event, event_type_t type)
{
	struct event *ev;
	int err;

	if (!event) {
		return -EINVAL;
	}

	if (!(ev = calloc(1, sizeof(*ev)))) {
		return -ENOMEM;
	}

	if (gettimeofday(&ev->time, NULL) < 0) {
		err = -errno;
		free(ev);
	} else {
		err = 0;
		ev->type = type;
		*event = ev;
	}

	return err;
}

int event_free(struct event **event)
{
	if (!event) {
		return -EINVAL;
	}

	if (!*event) {
		return -EALREADY;
	}

	free(*event);
	*event = NULL;
	return 0;
}

int event_nq(struct event *event)
{
	if (!event) {
		return -EINVAL;
	}

	if (eventq[idx.nq]) {
		return -EBUSY;
	}

	eventq[idx.nq] = event;
	idx.nq = (idx.nq + 1) & QMASK;

	return 0;
}

int event_dq(struct event **event)
{
	if (!event) {
		return -EINVAL;
	}

	if (!eventq[idx.dq]) {
		return -ENOENT;
	}

	*event = eventq[idx.dq];
	eventq[idx.dq] = NULL;
	idx.dq = (idx.dq + 1) & QMASK;

	return 0;
}
