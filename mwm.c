#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <X11/Xlib.h>
#include "mwm.h"
#include "monitor.h"
#include "array.h"

struct mwm {
	Display *display;

	struct array *monitors;
};

int mwm_new(struct mwm **dst)
{
	struct mwm *mwm;

	if(!dst) {
		return(-EINVAL);
	}

	mwm = malloc(sizeof(*mwm));

	if(!mwm) {
		return(-ENOMEM);
	}

	memset(mwm, 0, sizeof(*mwm));

	if(array_new(&(mwm->monitors)) < 0) {
		free(mwm);
		return(-ENOMEM);
	}

	*dst = mwm;

	return(0);
}

int mwm_free(struct mwm **mwm)
{
	if(!mwm) {
		return(-EINVAL);
	}

	if(!*mwm) {
		return(-EALREADY);
	}

	free(*mwm);
	*mwm = NULL;

	return(0);
}

int mwm_init(struct mwm *mwm)
{
	return(-ENOSYS);
}

int mwm_run(struct mwm *mwm)
{
	return(-ENOSYS);
}

int mwm_attach_monitor(struct mwm *mwm, struct monitor *mon)
{
	int idx;

	if(!mwm || !mon) {
		return(-EINVAL);
	}

	idx = monitor_get_id(mon);

	if(array_set(mwm->monitors, idx, mon) < 0) {
		return(-ENOMEM);
	}

	/* register monitor */

	return(0);
}

int mwm_detach_monitor(struct mwm *mwm, struct monitor *mon)
{
	int idx;

	if(!mwm || !mon) {
		return(-EINVAL);
	}

	idx = monitor_get_id(mon);

	if(array_take(mwm->monitors, idx, NULL) < 0) {
		return(-ENODEV);
	}

	/* unregister monitor */

	return(0);
}
