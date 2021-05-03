#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
#include "mwm.h"
#include "workspace.h"
#include "monitor.h"
#include "array.h"
#include "loop.h"
#include "x.h"
#include "common.h"

typedef void (_mwm_xhandler_t)(struct mwm *, XEvent *);

struct mwm {
	Display *display;
	int screen;
	Window root;
	struct geom root_geom;

	int running;
	struct loop *monitors;
	struct loop *workspaces;

	_mwm_xhandler_t *xhandler[LASTEvent];
	mwm_handler_t *handler[MWM_EVENT_LAST];
};

static void _mwm_button_press(struct mwm *mwm, XEvent *event)
{
	return;
}

static void _mwm_client_message(struct mwm *mwm, XEvent *event)
{
	return;
}

static void _mwm_configure_request(struct mwm *mwm, XEvent *event)
{
	return;
}

static int _cmp_monitor_id(struct monitor *mon, int *id)
{
	return(monitor_get_id(mon) == *id);
}

static void _mwm_configure_notify(struct mwm *mwm, XEvent *event)
{
	XConfigureEvent *cevent;
	XineramaScreenInfo *screen_info;
	int num_monitors;
	int i;

	cevent = &event->xconfigure;

	if(cevent->window != mwm->root) {
		return;
	}

	if(x_get_geom(mwm->display, mwm->root, &mwm->root_geom) < 0) {
		mwm_stop(mwm);
		return;
	}

	screen_info = XineramaQueryScreens(mwm->display, &num_monitors);

	for(i = 0; i < num_monitors; i++) {
		struct monitor *mon;

		printf("Screen Info %d/%d: %d, %d, %d, %d\n",
		       i,
		       screen_info[i].screen_number,
		       screen_info[i].x_org, screen_info[i].y_org,
		       screen_info[i].width, screen_info[i].height);

		if(loop_find(&mwm->monitors, (int(*)(void*,void*))_cmp_monitor_id,
			     &screen_info[i].screen_number, (void**)&mon) < 0) {
			if(monitor_new(screen_info[i].screen_number,
				       screen_info[i].x_org, screen_info[i].y_org,
				       screen_info[i].width, screen_info[i].height,
				       &mon) < 0) {
				/* TODO: Let the user know */
				return;
			}

			if(mwm_attach_monitor(mwm, mon) < 0) {
				monitor_free(&mon);
				/* TODO: Again, let the user know */
				return;
			}
		} else {
			/* update geometry */

			printf("Old monitor\n");

			if(monitor_set_geometry(mon, screen_info[i].x_org,
						screen_info[i].y_org,
						screen_info[i].width,
						screen_info[i].height) < 0) {
				/* TODO: Let the user know */
			}
		}
	}

	/* TODO: check if monitors have been removed */

	if(screen_info) {
		XFree(screen_info);
	}

	return;
}

static void _mwm_destroy_notify(struct mwm *mwm, XEvent *event)
{
	return;
}

static void _mwm_enter_notify(struct mwm *mwm, XEvent *event)
{
	return;
}

static void _mwm_expose(struct mwm *mwm, XEvent *event)
{
	return;
}

static void _mwm_focus_in(struct mwm *mwm, XEvent *event)
{
	return;
}

static void _mwm_key_press(struct mwm *mwm, XEvent *event)
{
	return;
}

static void _mwm_mapping_notify(struct mwm *mwm, XEvent *event)
{
	return;
}

static void _mwm_map_request(struct mwm *mwm, XEvent *event)
{
	return;
}

static void _mwm_motion_notify(struct mwm *mwm, XEvent *event)
{
	return;
}

static void _mwm_property_notify(struct mwm *mwm, XEvent *event)
{
	return;
}

static void _mwm_unmap_notify(struct mwm *mwm, XEvent *event)
{
	return;
}

static void _mwm_notify(struct mwm *mwm, mwm_event_t event, void *data)
{
	if(!mwm || event >= MWM_EVENT_LAST) {
		return;
	}

	if(mwm->handler[event]) {
		mwm->handler[event](mwm, event, data);
	}

	return;
}

int mwm_new(struct mwm **dst)
{
	struct mwm *mwm;
	int err;
	int i;

	err = 0;

	if(!dst) {
		return(-EINVAL);
	}

	mwm = malloc(sizeof(*mwm));

	if(!mwm) {
		return(-ENOMEM);
	}

	memset(mwm, 0, sizeof(*mwm));

	for(i = 0; i < 12; i++) {
		struct workspace *workspace;

		if(workspace_new(i, &workspace) < 0 ||
		   loop_append(&mwm->workspaces, workspace) < 0) {
			err = -ENOMEM;
			goto cleanup;
		}
	}

	mwm->xhandler[ButtonPress]      = _mwm_button_press;
	mwm->xhandler[ClientMessage]    = _mwm_client_message;
	mwm->xhandler[ConfigureRequest] = _mwm_configure_request;
	mwm->xhandler[ConfigureNotify]  = _mwm_configure_notify;
	mwm->xhandler[DestroyNotify]    = _mwm_destroy_notify;
	mwm->xhandler[EnterNotify]      = _mwm_enter_notify;
	mwm->xhandler[Expose]           = _mwm_expose;
	mwm->xhandler[FocusIn]          = _mwm_focus_in;
	mwm->xhandler[KeyPress]         = _mwm_key_press;
	mwm->xhandler[MappingNotify]    = _mwm_mapping_notify;
	mwm->xhandler[MapRequest]       = _mwm_map_request;
	mwm->xhandler[MotionNotify]     = _mwm_motion_notify;
	mwm->xhandler[PropertyNotify]   = _mwm_property_notify;
	mwm->xhandler[UnmapNotify]      = _mwm_unmap_notify;

cleanup:
	if(err < 0) {
		mwm_free(&mwm);
	} else {
		*dst = mwm;
	}

	return(err);
}

int mwm_free(struct mwm **mwm)
{
	if(!mwm) {
		return(-EINVAL);
	}

	if(!*mwm) {
		return(-EALREADY);
	}

	loop_free(&(*mwm)->workspaces);
	loop_free(&(*mwm)->monitors);

	if((*mwm)->display) {
		XCloseDisplay((*mwm)->display);
	}

	free(*mwm);
	*mwm = NULL;

	return(0);
}

int mwm_init(struct mwm *mwm)
{
	if(!mwm) {
		return(-EINVAL);
	}

	mwm->display = XOpenDisplay(NULL);

	if(!mwm->display) {
		return(-EIO);
	}

	mwm->screen = DefaultScreen(mwm->display);
	mwm->root = RootWindow(mwm->display, mwm->screen);

	XSelectInput(mwm->display, mwm->root,
		     SubstructureRedirectMask |
		     SubstructureNotifyMask |
		     ButtonPressMask |
		     PointerMotionMask |
		     EnterWindowMask |
		     LeaveWindowMask |
		     StructureNotifyMask |
		     PropertyChangeMask);

	x_configure_notify(mwm->display, mwm->root, NULL, 0);

	return(0);
}

int mwm_run(struct mwm *mwm)
{
	XEvent event;

	XSync(mwm->display, False);
	mwm->running = 1;

	while(mwm->running && !XNextEvent(mwm->display, &event)) {
		if(mwm->xhandler[event.type]) {
			mwm->xhandler[event.type](mwm, &event);
		}
	}

	return(0);
}

int mwm_stop(struct mwm *mwm)
{
	if(!mwm) {
		return(-EINVAL);
	}

	if(!mwm->running) {
		return(-EALREADY);
	}

	mwm->running = 0;

	return(0);
}

int mwm_attach_monitor(struct mwm *mwm, struct monitor *mon)
{
	int idx;

	if(!mwm || !mon) {
		return(-EINVAL);
	}

	idx = monitor_get_id(mon);

	if(loop_append(&mwm->monitors, mon) < 0) {
		return(-ENOMEM);
	}

	_mwm_notify(mwm, MWM_EVENT_MONITOR_ATTACHED, mon);

	printf("Attached monitor %d: %p\n", idx, (void*)mon);

	return(0);
}

int mwm_detach_monitor(struct mwm *mwm, struct monitor *mon)
{
	if(!mwm || !mon) {
		return(-EINVAL);
	}

	if(loop_remove(&mwm->monitors, mon) < 0) {
		return(-ENODEV);
	}

	/* unregister monitor */

	return(0);
}
