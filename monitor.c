#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "monitor.h"

struct monitor {
	int id;
	int x;
	int y;
	int w;
	int h;
};

int monitor_new(int id, int x, int y, int w, int h,
		struct monitor **monitor)
{
	struct monitor *mon;

	if(!monitor) {
		return(-EINVAL);
	}

	mon = malloc(sizeof(*mon));

	if(!mon) {
		return(-ENOMEM);
	}

	mon->id = id;
	mon->x = x;
	mon->y = y;
	mon->w = w;
	mon->h = h;

	*monitor = mon;

	return(0);
}

int monitor_free(struct monitor **monitor)
{
	if(!monitor) {
		return(-EINVAL);
	}

	if(!*monitor) {
		return(-EALREADY);
	}

	free(*monitor);
	*monitor = NULL;

	return(0);
}

int monitor_get_id(struct monitor *monitor)
{
	if(!monitor) {
		return(-EINVAL);
	}

	return(monitor->id);
}

int monitor_get_geometry(struct monitor *monitor,
			 int *x, int *y, int *w, int *h)
{
	if(!monitor) {
		return(-EINVAL);
	}

	if(x) {
		*x = monitor->x;
	}
	if(y) {
		*y = monitor->y;
	}
	if(w) {
		*w = monitor->w;
	}
	if(h) {
		*h = monitor->h;
	}

	return(0);
}

int monitor_set_geometry(struct monitor *monitor,
			 int x, int y, int w, int h)
{
	if(!monitor) {
		return(-EINVAL);
	}

	monitor->x = x;
	monitor->y = y;
	monitor->w = w;
	monitor->h = h;

	return(0);
}
