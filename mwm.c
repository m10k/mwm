#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xproto.h>
#include <X11/extensions/Xinerama.h>
#include <X11/Xft/Xft.h>
#include <X11/XKBlib.h>
#include <pango/pango.h>
#include <pango/pangoxft.h>
#include "mwm.h"
#include "keys.h"
#include "workspace.h"
#include "monitor.h"
#include "loop.h"
#include "x.h"
#include "common.h"
#include "client.h"
#include "theme.h"
#include "kbptr.h"

typedef void (_mwm_xhandler_t)(struct mwm*, XEvent*);

#define FIND_MONITOR_BY_ID       ((int(*)(void*, void*))_cmp_monitor_id)
#define FIND_CLIENT_BY_WINDOW    ((int(*)(struct client*, void*))_cmp_client_window)
#define FIND_MONITOR_BY_WINDOW   ((int(*)(void*, void*))_cmp_monitor_contains_window)
#define FIND_MONITOR_BY_GEOM     ((int(*)(void*, void*))_cmp_monitor_contains_geom)
#define FIND_WORKSPACE_BY_VIEWER ((int(*)(void*, void*))_cmp_workspace_viewer)
#define FIND_WORKSPACE_BY_NUMBER ((int(*)(void*, void*))_cmp_workspace_number)

struct palette {
	unsigned long color[MWM_COLOR_MAX];
	XftColor xcolor[MWM_COLOR_MAX];
};

struct mwm {
	Display *display;
	int screen;
	Window root;
	struct geom root_geom;

	int running;
	int needs_redraw;
	struct loop *monitors;
	struct loop *workspaces;
	struct monitor *current_monitor;
	struct client *focused_client;

	_mwm_xhandler_t *xhandler[LASTEvent];

	struct {
		PangoLayout *layout;
		PangoLayout *vlayout;
		int ascent;
		int descent;
		int height;
	} font;

	struct palette palette[MWM_PALETTE_MAX];

	void (*commands[MWM_CMD_MAX])(struct mwm*, void*);

	int (*xerror_default_handler)(Display*, XErrorEvent*);
};

extern struct mwm *__mwm;

static int _xerror_startup(Display *display, XErrorEvent *event);
static int _xerror_handle(Display *display, XErrorEvent *event);
static int _xerror_nop(Display *display, XErrorEvent *event);

static int _cmp_client_window(struct client *client, Window *window)
{
	return(client_get_window(client) == *window ? 0 : 1);
}

static int _cmp_monitor_id(struct monitor *mon, int *id)
{
	return(monitor_get_id(mon) == *id ? 0 : 1);
}

static int _cmp_monitor_contains_window(struct monitor *monitor, Window *window)
{
	struct geom window_geom;
	struct geom monitor_geom;

	if(x_get_geom(monitor_get_display(monitor), *window, &window_geom) < 0 ||
	   monitor_get_geometry(monitor, &monitor_geom) < 0) {
		return(-EFAULT);
	}

	return(geom_intersects(&monitor_geom, &window_geom) > 0 ? 0 : 1);
}

static int _cmp_monitor_contains_geom(struct monitor *monitor, struct geom *geom)
{
	struct geom monitor_geom;

	if(monitor_get_geometry(monitor, &monitor_geom) < 0) {
		return(-EFAULT);
	}

	return(geom_intersects(&monitor_geom, geom) > 0 ? 0 : 1);
}

static int _cmp_workspace_viewer(struct workspace *workspace, struct monitor *monitor)
{
	return(workspace_get_viewer(workspace) == monitor ? 0 : 1);
}

static int _cmp_workspace_number(struct workspace *workspace, int *number)
{
	return(workspace_get_number(workspace) == *number ? 0 : 1);
}

static void _mwm_button_press(struct mwm *mwm, XEvent *event)
{
	XButtonPressedEvent *button_pressed;
	struct monitor *event_monitor;
	struct client *event_client;

#if MWM_DEBUG
	fprintf(stderr, "%s(%p, %p)\n", __func__, (void*)mwm, (void*)event);
#endif /* MWM_DEBUG */

	button_pressed = &event->xbutton;

	if(loop_find(&mwm->monitors, FIND_MONITOR_BY_WINDOW,
		     &button_pressed->window, (void**)&event_monitor) == 0) {
		mwm_focus_monitor(mwm, event_monitor);
	}

	/*
	 * TODO: Handle the event
	 */

	if(mwm_find_client(mwm, FIND_CLIENT_BY_WINDOW,
                           &button_pressed->window, &event_client) == 0) {
		mwm_focus_client(mwm, event_client);
	}

	return;
}

static void _mwm_client_message(struct mwm *mwm, XEvent *event)
{
	/* TODO: fullscreen toggle */
#if MWM_DEBUG
	fprintf(stderr, "%s(%p, %p)\n", __func__, (void*)mwm, (void*)event);
#endif /* MWM_DEBUG */

	return;
}

static void _mwm_configure_request(struct mwm *mwm, XEvent *event)
{
	XConfigureRequestEvent *configure_request;
	struct client *client;

	/*
	 * This event is generated whenever the client attempts to resize itself.
	 * If the client is floating, or the viewer's layout is floating, we will
	 * accept the resize request.
	 * Otherwise we will override the request with the values that we have
	 * stored in the client structure.
	 */
#if MWM_DEBUG
	fprintf(stderr, "%s(%p, %p)\n", __func__, (void*)mwm, (void*)event);
#endif /* MWM_DEBUG */

	configure_request = &event->xconfigurerequest;

	if(mwm_find_client(mwm, FIND_CLIENT_BY_WINDOW,
			   &configure_request->window, &client) < 0) {
		XWindowChanges changes;
		unsigned int value_mask;

		/* no client associated with that window */

		changes.x = configure_request->x;
		changes.y = configure_request->y;
		changes.width = configure_request->width;
		changes.height = configure_request->height;
		changes.border_width = 0; /* TODO: make border width configurable */
		changes.sibling = configure_request->above;
		changes.stack_mode = configure_request->detail;
		value_mask = configure_request->value_mask | CWBorderWidth;

		XConfigureWindow(mwm->display, configure_request->window,
				 value_mask, &changes);
	} else {
		struct geom requested_geom;

		/*
		 * We have a client for that window. Let's see what it is
		 * that the client requested, and let client_change_geometry()
		 * do the deciding.
		 */

		if(configure_request->value_mask & CWBorderWidth) {
			client_set_border(client, configure_request->border_width);
		}

		client_get_geometry(client, &requested_geom);

		if(configure_request->value_mask & CWX) {
			requested_geom.x = configure_request->x;
		}
		if(configure_request->value_mask & CWY) {
			requested_geom.y = configure_request->y;
		}
		if(configure_request->value_mask & CWWidth) {
			requested_geom.w = configure_request->width;
		}
		if(configure_request->value_mask & CWHeight) {
			requested_geom.h = configure_request->height;
		}

		client_change_geometry(client, &requested_geom);
		client_set_state(client, NormalState);
	}

	XSync(mwm->display, False);

	return;
}

static void _mwm_configure_notify(struct mwm *mwm, XEvent *event)
{
	XConfigureEvent *cevent;
	XineramaScreenInfo *screen_info;
	int num_monitors;
	int i;

#if MWM_DEBUG
	fprintf(stderr, "%s(%p, %p)\n", __func__, (void*)mwm, (void*)event);
#endif /* MWM_DEBUG */

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

#if MWM_DEBUG
		fprintf(stderr, "Screen Info %d/%d: %d, %d, %d, %d\n",
			i,
			screen_info[i].screen_number,
			screen_info[i].x_org, screen_info[i].y_org,
			screen_info[i].width, screen_info[i].height);
#endif /* MWM_DEBUG */

		if(loop_find(&mwm->monitors, FIND_MONITOR_BY_ID,
			     &screen_info[i].screen_number, (void**)&mon) < 0) {
			if(monitor_new(mwm, screen_info[i].screen_number,
				       screen_info[i].x_org, screen_info[i].y_org,
				       screen_info[i].width, screen_info[i].height,
				       &mon) < 0) {
				fprintf(stderr, "Could not allocate monitor\n");
				/* TODO: Let the user know */
				return;
			}

			if(mwm_attach_monitor(mwm, mon) < 0) {
				monitor_free(&mon);
				/* TODO: Again, let the user know */
				fprintf(stderr, "Could not attach monitor\n");
				return;
			}
		} else {
			struct geom new_geom;

			/* update geometry */
			new_geom.x = screen_info[i].x_org;
			new_geom.y = screen_info[i].y_org;
			new_geom.w = screen_info[i].width;
			new_geom.h = screen_info[i].height;

			if(monitor_set_geometry(mon, &new_geom) < 0) {
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

static void _mwm_destroy_notify(struct mwm *mwm, XDestroyWindowEvent *event)
{
	struct client *client;

	/* get the client and detach it */
#if MWM_DEBUG
	fprintf(stderr, "%s(%p, %p)\n", __func__, (void*)mwm, (void*)event);
#endif /* MWM_DEBUG */

	if(mwm_find_client(mwm, FIND_CLIENT_BY_WINDOW,
			   &event->window, &client) < 0) {
		fprintf(stderr, "Couldn't find client\n");
		return;
	}

	if(mwm_detach_client(mwm, client) < 0) {
		fprintf(stderr, "Couldn't detach client\n");
		return;
	}

	client_free(&client);
	return;
}

static void _mwm_enter_notify(struct mwm *mwm, XCrossingEvent *event)
{
	struct client *client;
	struct monitor *monitor;

	client = NULL;
	monitor = NULL;

	/* pointer has entered a window - move focus, if it makes sense */
#if MWM_DEBUG
	fprintf(stderr, "%s(%p, %p)\n", __func__, (void*)mwm, (void*)event);
#endif /* MWM_DEBUG */

	if((event->mode != NotifyNormal || event->detail == NotifyInferior) &&
	   event->window == mwm->root) {
		return;
	}

	if(mwm_find_client(mwm, FIND_CLIENT_BY_WINDOW,
			   &event->window, &client) == 0) {
		mwm_focus_client(mwm, client);
	}

	if(loop_find(&mwm->monitors, FIND_MONITOR_BY_WINDOW,
		     &event->window, (void**)&monitor) == 0) {
		mwm_focus_monitor(mwm, monitor);
	}

	return;
}

static void _mwm_expose(struct mwm *mwm, XExposeEvent *event)
{
	struct monitor *monitor;

#if MWM_DEBUG
	fprintf(stderr, "%s(%p, %p) W=0x%lx\n", __func__, (void*)mwm, (void*)event, event->window);
#endif /* MWM_DEBUG */

	/* redraw the status bar, if we have one */

	if(event->count > 0) {
		return;
	}

	if(loop_find(&mwm->monitors, FIND_MONITOR_BY_WINDOW,
		     &event->window, (void**)&monitor) == 0) {
		monitor_needs_redraw(monitor);
	}

	return;
}

static void _mwm_focus_in(struct mwm *mwm, XFocusInEvent *event)
{
	struct client *client;

	/* move focus to the client referenced by the event */
#if MWM_DEBUG
	fprintf(stderr, "%s(%p, %p)\n", __func__, (void*)mwm, (void*)event);
#endif /* MWM_DEBUG */

	if(mwm_find_client(mwm, FIND_CLIENT_BY_WINDOW,
			   &event->window, &client) < 0) {
		return;
	}

	mwm_focus_client(mwm, client);

	return;
}

static void _mwm_key_press(struct mwm *mwm, XKeyEvent *event)
{
	extern struct key_binding config_keybindings[];
	struct key_binding *binding;
	KeySym keysym;
	unsigned int mask;

#if MWM_DEBUG
	fprintf(stderr, "%s(%p, %p)\n", __func__, (void*)mwm, (void*)event);
#endif /* MWM_DEBUG */

#define BUTTONMASK              (ButtonPressMask | ButtonReleaseMask)
#define ALLMODMASK              (Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask)
#define ALLMASK                 (ShiftMask | ControlMask | ALLMODMASK)
#define CLEANMASK(mask)         (mask & ~LockMask & ALLMASK)

	/* handle keyboard shortcuts */

	keysym = XkbKeycodeToKeysym(mwm->display, event->keycode, 0, 0);
	mask = CLEANMASK(event->state);

	for(binding = config_keybindings; binding->cmd < MWM_CMD_MAX; binding++) {
		if(keysym == binding->key && mask == CLEANMASK(binding->mod)) {
			mwm_cmd(mwm, binding->cmd, binding->arg);
		}
	}

#undef BUTTONMASK
#undef ALLMODMASK
#undef ALLMASK
#undef CLEANMASK
	return;
}

static void _mwm_mapping_notify(struct mwm *mwm, XMappingEvent *event)
{
#if MWM_DEBUG
	fprintf(stderr, "%s(%p, %p)\n", __func__, (void*)mwm, (void*)event);
#endif /* MWM_DEBUG */

	XRefreshKeyboardMapping(event);

	if(event->request == MappingKeyboard) {
		mwm_grab_keys(mwm);
	}

	return;
}

static void _mwm_map_request(struct mwm *mwm, XMapRequestEvent *event)
{
	XWindowAttributes attrs;

#if MWM_DEBUG
	fprintf(stderr, "%s(%p, %p)\n", __func__, (void*)mwm, (void*)event);
#endif /* MWM_DEBUG */

	if(!XGetWindowAttributes(mwm->display, event->window, &attrs)) {
		return;
	}

	if(attrs.override_redirect) {
		return;
	}

	if(mwm_find_client(mwm, FIND_CLIENT_BY_WINDOW,
			   &event->window, NULL) < 0) {
		struct client *client;

		if(client_new(event->window, &attrs, &client) < 0) {
			/* ENOMEM */
			return;
		}

		if(mwm_attach_client(mwm, client) < 0) {
			/* ENOMEM */
			client_free(&client);
			return;
		}
	}

	return;
}

static void _mwm_motion_notify(struct mwm *mwm, XMotionEvent *event)
{
	struct monitor *monitor;
	struct geom pointer_geom;

#if MWM_DEBUG
	fprintf(stderr, "%s(%p, %p)\n", __func__, (void*)mwm, (void*)event);
#endif /* MWM_DEBUG */

	/* move focus to the monitor referenced in the event */
	/* printf("%s(%p, %p)\n", __func__, (void*)mwm, (void*)event); */

	pointer_geom.x = event->x_root;
	pointer_geom.y = event->y_root;
	pointer_geom.w = 1;
	pointer_geom.h = 1;

	if(loop_find(&mwm->monitors, FIND_MONITOR_BY_GEOM,
		     &pointer_geom, (void**)&monitor) < 0) {
		return;
	}

	if(mwm_get_focused_monitor(mwm) != monitor) {
		mwm_focus_monitor(mwm, monitor);
	}

	return;
}

static void _mwm_property_notify(struct mwm *mwm, XPropertyEvent *event)
{
#if MWM_DEBUG
	fprintf(stderr, "%s(%p, %p)\n", __func__, (void*)mwm, (void*)event);
#endif /* MWM_DEBUG */

	/* FIXME: Property notification handling must be implemented more thoroughly */

	if((event->window == mwm->root)) {
		/* if(event->atom == XA_WM_NAME) */
		mwm_needs_redraw(mwm);
	}

	return;
}

static void _mwm_unmap_notify(struct mwm *mwm, XUnmapEvent *event)
{
	struct client *client;

#if MWM_DEBUG
	fprintf(stderr, "%s(%p, %p)\n", __func__, (void*)mwm, (void*)event);
#endif /* MWM_DEBUG */

        if(mwm_find_client(mwm, FIND_CLIENT_BY_WINDOW,
                           &event->window, &client) < 0) {
		return;
	}

	XGrabServer(mwm->display);
	XSync(mwm->display, False);
	XSetErrorHandler(_xerror_nop);

	client_set_state(client, WithdrawnState);

	if(!event->send_event) {
		mwm_detach_client(mwm, client);
		client_free(&client);
	}

	XSync(mwm->display, False);
	XSetErrorHandler(_xerror_handle);
	XUngrabServer(mwm->display);

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

	mwm->xhandler[ButtonPress]      = (_mwm_xhandler_t*)_mwm_button_press;
	mwm->xhandler[ClientMessage]    = (_mwm_xhandler_t*)_mwm_client_message;
	mwm->xhandler[ConfigureRequest] = (_mwm_xhandler_t*)_mwm_configure_request;
	mwm->xhandler[ConfigureNotify]  = (_mwm_xhandler_t*)_mwm_configure_notify;
	mwm->xhandler[DestroyNotify]    = (_mwm_xhandler_t*)_mwm_destroy_notify;
	mwm->xhandler[EnterNotify]      = (_mwm_xhandler_t*)_mwm_enter_notify;
	mwm->xhandler[Expose]           = (_mwm_xhandler_t*)_mwm_expose;
	mwm->xhandler[FocusIn]          = (_mwm_xhandler_t*)_mwm_focus_in;
	mwm->xhandler[KeyPress]         = (_mwm_xhandler_t*)_mwm_key_press;
	mwm->xhandler[MappingNotify]    = (_mwm_xhandler_t*)_mwm_mapping_notify;
	mwm->xhandler[MapRequest]       = (_mwm_xhandler_t*)_mwm_map_request;
	mwm->xhandler[MotionNotify]     = (_mwm_xhandler_t*)_mwm_motion_notify;
	mwm->xhandler[PropertyNotify]   = (_mwm_xhandler_t*)_mwm_property_notify;
	mwm->xhandler[UnmapNotify]      = (_mwm_xhandler_t*)_mwm_unmap_notify;

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

Display* mwm_get_display(struct mwm *mwm)
{
	return(mwm->display);
}

Window mwm_get_root_window(struct mwm *mwm)
{
	return(mwm->root);
}

static int _color_init(struct mwm *mwm,
		       unsigned long *color,
		       XftColor *xcolor,
		       const char *colorspec)
{
	Visual *visual;
	Colormap colormap;

	visual = DefaultVisual(mwm->display, mwm->screen);
	colormap = DefaultColormap(mwm->display, mwm->screen);

	if(!XftColorAllocName(mwm->display, visual, colormap,
			      colorspec, xcolor)) {
		return(-EIO);
	}

	*color = xcolor->pixel;
	return(0);
}

static int _palette_init(struct mwm *mwm,
			 struct palette *palette,
			 union colorset *colorset)
{
	int i;

	for(i = 0; i < MWM_COLOR_MAX; i++) {
		_color_init(mwm, &palette->color[i],
			    &palette->xcolor[i],
			    colorset->indexed[i]);
	}

	return(0);
}

void _sigchld(int unused)
{
	if(signal(SIGCHLD, _sigchld) == SIG_ERR) {
		perror("signal");
		exit(1);
	}

	while(waitpid(-1, NULL, WNOHANG) > 0);

	return;
}

static void _cmd_spawn(struct mwm *mwm, void *arg)
{
        char **argv;
	pid_t pid;

	argv = (char**)arg;
	pid = fork();

	if(pid == 0) {
		close(ConnectionNumber(mwm->display));
		setsid();
		execvp(*argv, argv);
		exit(0);
	}

	return;
}

static void _cmd_show_workspace(struct mwm *mwm, void *arg)
{
	struct monitor *monitor;
	struct workspace *workspace;
	long number;

        number = (long)arg;
	monitor = mwm_get_focused_monitor(mwm);

	if(loop_find(&mwm->workspaces, FIND_WORKSPACE_BY_NUMBER,
		     (void*)&number, (void**)&workspace) < 0) {
		return;
	}

	monitor_set_workspace(monitor, workspace);
	return;
}

static void _cmd_move_to_workspace(struct mwm *mwm, void *arg)
{
	struct workspace *src_workspace;
	struct workspace *dst_workspace;
	struct client *client;
	long dst_number;

	dst_number = (long)arg;
	client = mwm_get_focused_client(mwm);

	if(!client) {
		return;
	}

	if(loop_find(&mwm->workspaces, FIND_WORKSPACE_BY_NUMBER,
		     (void*)&dst_number, (void**)&dst_workspace) < 0) {
		return;
	}

	src_workspace = client_get_workspace(client);

	if(src_workspace != dst_workspace) {
		workspace_detach_client(src_workspace, client);
		workspace_attach_client(dst_workspace, client);
	}

	return;
}

static void _cmd_set_layout(struct mwm *mwm, void *arg)
{
	extern struct layout *layouts[];
	struct monitor *monitor;
	long num;

	num = (long)arg;
	monitor = mwm_get_focused_monitor(mwm);

	if(monitor_get_layout(monitor) != layouts[num]) {
		monitor_set_layout(monitor, layouts[num]);
		monitor_needs_redraw(monitor);
	}

	return;
}

static void _cmd_shift_focus(struct mwm *mwm, void *arg)
{
	struct workspace *workspace;
	long dir;

	dir = (long)arg;

	workspace = mwm_get_focused_workspace(mwm);
	workspace_shift_focus(workspace, dir);

	return;
}

static void _cmd_shift_client(struct mwm *mwm, void *arg)
{
	struct workspace *workspace;
	long dir;

	dir = (long)arg;

	workspace = mwm_get_focused_workspace(mwm);
	workspace_shift_client(workspace, NULL, dir);

	return;
}

static void _cmd_shift_monitor_focus(struct mwm *mwm, void *arg)
{
	struct monitor *src_monitor;
	struct monitor *dst_monitor;
	long dir;

	dir = (long)arg;
	/* move focus to previous or next monitor */

	if(!mwm || dir == 0) {
		return;
	}

	src_monitor = mwm_get_focused_monitor(mwm);

	if(dir > 0) {
		if(loop_get_next(&mwm->monitors, src_monitor, (void**)&dst_monitor) < 0) {
			return;
		}
	} else {
		if(loop_get_prev(&mwm->monitors, src_monitor, (void**)&dst_monitor) < 0) {
			return;
		}
	}

	if(src_monitor != dst_monitor) {
		mwm_focus_monitor(mwm, dst_monitor);

		monitor_needs_redraw(src_monitor);
		monitor_needs_redraw(dst_monitor);
	}

	return;
}

static void _cmd_shift_workspace(struct mwm *mwm, void *arg)
{
	struct monitor *src_monitor;
	struct monitor *dst_monitor;
	struct workspace *workspace;
	long dir;

	/* move workspace to the next or previous monitor */

	dir = (long)arg;

	if(!mwm || dir == 0) {
		return;
	}

	src_monitor = mwm_get_focused_monitor(mwm);
	workspace = monitor_get_workspace(src_monitor);

	if(dir > 0) {
		if(loop_get_next(&mwm->monitors, src_monitor, (void**)&dst_monitor) < 0) {
			return;
		}
	} else {
		if(loop_get_prev(&mwm->monitors, src_monitor, (void**)&dst_monitor) < 0) {
			return;
		}
	}

	if(src_monitor != dst_monitor) {
		monitor_set_workspace(dst_monitor, workspace);
		mwm_focus_monitor(mwm, dst_monitor);
	}

	return;
}

static void _cmd_quit(struct mwm *mwm, void *arg)
{
	mwm_stop(mwm);
	return;
}

static void _cmd_kbptr_move(struct mwm *mwm, void *arg)
{
	struct client *client;
	long dir;

	dir = (long)arg;
	client = mwm_get_focused_client(mwm);

	if(client) {
		kbptr_move(mwm, client, dir);
	}

	return;
}

static void _cmd_kbptr_click(struct mwm *mwm, void *arg)
{
	struct client *client;
	long button;

	button = (long)arg;
	client = mwm_get_focused_client(mwm);

	if(client) {
		kbptr_click(mwm, client, button);
	}

	return;
}

static int _xerror_startup(Display *display, XErrorEvent *event)
{
	fprintf(stderr, "Looks like I'm not your only window manager\n");
	exit(1);
	return(-1);
}

static int _xerror_nop(Display *display, XErrorEvent *event)
{
	return(0);
}

static int _can_ignore_error(XErrorEvent *event)
{
	static struct {
		unsigned char error_code;
		unsigned char request_code;
	} ignore_ok[] = {
		{ BadMatch, X_SetInputFocus },
		{ BadDrawable, X_PolyText8 },
		{ BadDrawable, X_PolyFillRectangle },
		{ BadDrawable, X_PolySegment },
		{ BadMatch, X_ConfigureWindow },
		{ BadAccess, X_GrabButton },
		{ BadAccess, X_GrabKey },
		{ BadDrawable, X_CopyArea }
	};
	int i;

	if(event->error_code == BadWindow) {
		return(1);
	}

	for(i = 0; i < (sizeof(ignore_ok) / sizeof(ignore_ok[0])); i++) {
		if(event->request_code == ignore_ok[i].request_code &&
		   event->error_code == ignore_ok[i].error_code) {
			return(1);
		}
	}

	return(0);
}

static int _xerror_handle(Display *display, XErrorEvent *event)
{
	if(_can_ignore_error(event)) {
		return(0);
	}

	return(__mwm->xerror_default_handler(display, event));
}

static void _find_existing_clients(struct mwm *mwm)
{
        Window dontcare;
        Window *windows;
        Window *cur;
        unsigned int num_windows;

        if(!XQueryTree(mwm->display, mwm->root, &dontcare, &dontcare, &windows, &num_windows)) {
                return;
        }

        for(cur = windows; cur < windows + num_windows; cur++) {
                XWindowAttributes attrs;

                if(!XGetWindowAttributes(mwm->display, *cur, &attrs)) {
                        continue;
                }

                if(attrs.override_redirect || XGetTransientForHint(mwm->display, *cur, &dontcare)) {
                        continue;
                }

                if(attrs.map_state == IsViewable /* || IconicState */ ) {
                        struct client *client;
                        int err;

                        if((err = client_new(*cur, &attrs, &client)) < 0) {
                                fprintf(stderr, "%s: client_new: %s\n", __func__, strerror(-err));
                        } else if((err = mwm_attach_client(mwm, client)) < 0) {
                                fprintf(stderr, "%s: mwm_attach_client: %s\n", __func__, strerror(-err));
                                client_free(&client);
                        }
                }
        }

        for(cur = windows; cur < windows + num_windows; cur++) {
                XWindowAttributes attrs;

                if(!XGetWindowAttributes(mwm->display, *cur, &attrs)) {
                        continue;
                }

                if(!XGetTransientForHint(mwm->display, *cur, &dontcare)) {
                        continue;
                }

                if(attrs.map_state == IsViewable /* || IconicState */ ) {
                        struct client *client;
                        int err;

                        if((err = client_new(*cur, &attrs, &client)) < 0) {
                                fprintf(stderr, "%s: client_new: %s\n", __func__, strerror(-err));
                        } else if((err = mwm_attach_client(mwm, client)) < 0) {
                                fprintf(stderr, "%s: mwm_attach_client: %s\n", __func__, strerror(-err));
                                client_free(&client);
                        }
                }
        }

        if(windows) {
                XFree(windows);
        }

        return;
}

int mwm_init(struct mwm *mwm)
{
	extern struct theme config_theme;
	PangoContext *context;
	PangoFontMap *fontmap;
	PangoFontDescription *fontdesc;
	PangoFontMetrics *fontmetrics;

	if(!mwm) {
		return(-EINVAL);
	}

	mwm->display = XOpenDisplay(NULL);

	if(!mwm->display) {
		return(-EIO);
	}

	_sigchld(0);

	mwm->screen = DefaultScreen(mwm->display);
	mwm->root = RootWindow(mwm->display, mwm->screen);
	mwm->xerror_default_handler = XSetErrorHandler(_xerror_startup);

	if(!mwm->xerror_default_handler) {
		return(-EIO);
	}

	XSelectInput(mwm->display, mwm->root,
		     SubstructureRedirectMask |
		     SubstructureNotifyMask |
		     /* ButtonPressMask | */
		     PointerMotionMask |
		     EnterWindowMask |
		     LeaveWindowMask |
		     StructureNotifyMask |
		     PropertyChangeMask);
	XSync(mwm->display, False);

	XSetErrorHandler(_xerror_handle);
	XSync(mwm->display, False);

	mwm_grab_keys(mwm);

	x_configure_notify(mwm->display, mwm->root, NULL, 0);

	fontmap = pango_xft_get_font_map(mwm->display, mwm->screen);
	fontdesc = pango_font_description_from_string(config_theme.statusbar_font);

	/* set up the pango context/layout for horizontal text */
	context = pango_font_map_create_context(fontmap);
	mwm->font.layout = pango_layout_new(context);
	pango_layout_set_font_description(mwm->font.layout, fontdesc);
	fontmetrics = pango_context_get_metrics(context, fontdesc, NULL);
	g_object_unref(context);

	mwm->font.ascent = pango_font_metrics_get_ascent(fontmetrics) / PANGO_SCALE;
	mwm->font.descent = pango_font_metrics_get_descent(fontmetrics) / PANGO_SCALE;
	mwm->font.height = mwm->font.ascent + mwm->font.descent;
	pango_font_metrics_unref(fontmetrics);

	/* set up the pango context/layout for vertical text */
	context = pango_font_map_create_context(fontmap);
	mwm->font.vlayout = pango_layout_new(context);
	pango_layout_set_font_description(mwm->font.vlayout, fontdesc);
	g_object_unref(context);

	_palette_init(mwm, &(mwm->palette[MWM_PALETTE_ACTIVE]),
		      &config_theme.active);
	_palette_init(mwm, &(mwm->palette[MWM_PALETTE_INACTIVE]),
		      &config_theme.inactive);

	mwm->commands[MWM_CMD_QUIT] = _cmd_quit;
	mwm->commands[MWM_CMD_SPAWN] = _cmd_spawn;
	mwm->commands[MWM_CMD_SHOW_WORKSPACE] = _cmd_show_workspace;
	mwm->commands[MWM_CMD_MOVE_TO_WORKSPACE] = _cmd_move_to_workspace;
	mwm->commands[MWM_CMD_SET_LAYOUT] = _cmd_set_layout;
	mwm->commands[MWM_CMD_SHIFT_FOCUS] = _cmd_shift_focus;
	mwm->commands[MWM_CMD_SHIFT_CLIENT] = _cmd_shift_client;
	mwm->commands[MWM_CMD_SHIFT_MONITOR_FOCUS] = _cmd_shift_monitor_focus;
	mwm->commands[MWM_CMD_SHIFT_WORKSPACE] = _cmd_shift_workspace;
	mwm->commands[MWM_CMD_KBPTR_MOVE] = _cmd_kbptr_move;
	mwm->commands[MWM_CMD_KBPTR_CLICK] = _cmd_kbptr_click;

	_find_existing_clients(mwm);

	return(0);
}

int mwm_render_text(struct mwm *mwm, XftDraw *drawable,
		    mwm_palette_t palette, const char *text,
		    const int x, const int y)
{
	XftColor *color;

	if(!mwm || !drawable || !text) {
		return(-EINVAL);
	}

	color = &mwm->palette[palette].xcolor[MWM_COLOR_TEXT];

	pango_layout_set_attributes(mwm->font.layout, NULL);

	pango_layout_set_markup(mwm->font.layout, text, -1);
	pango_xft_render_layout(drawable, color,
				mwm->font.layout,
				x * PANGO_SCALE,
				y * PANGO_SCALE);

	return(0);
}

int mwm_render_text_vertical(struct mwm *mwm, XftDraw *drawable,
			     mwm_palette_t palette, const char *text,
			     const int x, const int y)
{
	PangoMatrix matrix = PANGO_MATRIX_INIT;
	PangoContext *context;
	XftColor *color;
	PangoRectangle extents;

	if(!mwm || ! drawable || !text) {
		return(-EINVAL);
	}

	context = pango_layout_get_context(mwm->font.vlayout);
	color = &mwm->palette[palette].xcolor[MWM_COLOR_TEXT];

	pango_matrix_translate(&matrix, x, y);
	pango_matrix_rotate(&matrix, -90.0);
	pango_context_set_matrix(context, &matrix);
	pango_context_set_base_gravity(context, PANGO_GRAVITY_EAST);

	pango_layout_set_attributes(mwm->font.vlayout, NULL);
	pango_layout_set_markup(mwm->font.vlayout, text, -1);
	pango_layout_get_extents(mwm->font.vlayout, NULL, &extents);

	pango_xft_render_layout(drawable, color, mwm->font.vlayout,
				0, -1.0 * extents.height);

	return(0);
}

int mwm_run(struct mwm *mwm)
{
	XEvent event;

	XSync(mwm->display, False);
	mwm->running = 1;

	while(mwm->running) {
		struct client *focused_client;

		/*
		 * Handle as many events as possible before redrawing. This is necessary
		 * to avoid problems where an application unmaps/destroys multiple windows
		 * in one go. If we process the events one-by-one, redrawing after each
		 * event, we will likely attempt to redraw a window that was already
		 * unmapped or destroyed, we just haven't noticed it yet because the event
		 * is still in the queue.
		 */

		do {
			if(XNextEvent(mwm->display, &event) == 0) {
				if(mwm->xhandler[event.type]) {
					mwm->xhandler[event.type](mwm, &event);
				}
			}
		} while(XEventsQueued(mwm->display, QueuedAfterFlush) > 0);

		if(mwm->needs_redraw) {
			mwm_redraw(mwm);
		}

		focused_client = mwm_get_focused_client(mwm);

		if(mwm->focused_client != focused_client) {
			client_focus(focused_client);
			mwm->focused_client = focused_client;
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
	struct workspace *unviewed;

	if(!mwm || !mon) {
		return(-EINVAL);
	}

	if(loop_append(&mwm->monitors, mon) < 0) {
		return(-ENOMEM);
	}

	if(!mwm_get_focused_monitor(mwm)) {
		mwm_focus_monitor(mwm, mon);
	}

	if(loop_find(&mwm->workspaces, FIND_WORKSPACE_BY_VIEWER, NULL, (void**)&unviewed) < 0) {
		return(-EFAULT);
	}

	monitor_set_workspace(mon, unviewed);

	return(0);
}

int mwm_detach_monitor(struct mwm *mwm, struct monitor *mon)
{
	struct workspace *workspace;

	if(!mwm || !mon) {
		return(-EINVAL);
	}

	if(loop_remove(&mwm->monitors, mon) < 0) {
		return(-ENODEV);
	}

	workspace = monitor_get_workspace(mon);
	workspace_set_viewer(workspace, NULL);

	return(0);
}

int mwm_focus_monitor(struct mwm *mwm, struct monitor *monitor)
{
	if(!mwm || !monitor) {
		return(-EINVAL);
	}

	if(mwm->current_monitor != monitor) {
#if MWM_DEBUG
		fprintf(stderr, "New current monitor: %p\n", (void*)monitor);
#endif /* MWM_DEBUG */

		monitor_needs_redraw(mwm->current_monitor);
		monitor_needs_redraw(monitor);
	}

	mwm->current_monitor = monitor;

	return(0);
}

struct monitor* mwm_get_focused_monitor(struct mwm *mwm)
{
	return(mwm->current_monitor);
}

int mwm_attach_client(struct mwm *mwm, struct client *client)
{
	struct workspace *workspace;

	if(!mwm || !client) {
		return(-EINVAL);
	}

	workspace = NULL;

	if(mwm->current_monitor) {
		workspace = monitor_get_workspace(mwm->current_monitor);
	}

	if(!workspace) {
		/*
		 * there's a chance that we might be attaching clients
		 * before the first monitor has been detected
		 */

		if(loop_get_first(&mwm->workspaces, (void**)&workspace) < 0) {
			/* this really shouldn't happen */
			return(-EFAULT);
		}
	}

#if MWM_DEBUG
	fprintf(stderr, "Attaching client %p to workspace %p\n",
		(void*)client, (void*)workspace);
#endif /* MWM_DEBUG */

        XSelectInput(mwm->display, client_get_window(client),
		     EnterWindowMask | FocusChangeMask |
		     PropertyChangeMask | StructureNotifyMask);

	client_set_state(client, NormalState);

	return(workspace_attach_client(workspace, client));
}

int mwm_detach_client(struct mwm *mwm, struct client *client)
{
	struct workspace *workspace;

	if(!mwm || !client) {
		return(-EINVAL);
	}

	workspace = client_get_workspace(client);

	workspace_detach_client(workspace, client);

	return(0);
}

int mwm_focus_client(struct mwm *mwm, struct client *client)
{
	struct workspace *workspace;

	if(!mwm) {
		return(-EINVAL);
	}

	if(client) {
		workspace = client_get_workspace(client);
	} else {
		workspace = mwm_get_focused_workspace(mwm);
	}

	if(!workspace) {
		return(-EBADFD);
	}

	return(workspace_focus_client(workspace, client));
}

struct client* mwm_get_focused_client(struct mwm *mwm)
{
	struct monitor *focused_monitor;

	focused_monitor = mwm_get_focused_monitor(mwm);

	if(!focused_monitor) {
		return(NULL);
	}

	return(monitor_get_focused_client(focused_monitor));
}

int mwm_find_client(struct mwm *mwm, int(*cmp)(struct client*, void*),
		    void *data, struct client **client)
{
	loop_iter_t first;
	loop_iter_t cur;

	if(!mwm) {
		return(-EINVAL);
	}

       	first = loop_get_iter(&mwm->workspaces);
	cur = first;

	do {
		struct workspace *workspace;

		workspace = (struct workspace*)loop_iter_get_data(cur);

		if(!workspace) {
			fprintf(stderr, "%s: Invalid workspace in loop\n", __func__);
			continue;
		}

		if(workspace_find_client(workspace, cmp, data, client) == 0) {
			return(0);
		}

		cur = loop_iter_get_next(cur);
	} while(cur != first);

	return(-ENOENT);
}

struct workspace *mwm_get_focused_workspace(struct mwm *mwm)
{
	struct monitor *monitor;
	struct workspace *workspace;

	workspace = NULL;
	monitor = mwm_get_focused_monitor(mwm);

	if(monitor) {
		workspace = monitor_get_workspace(monitor);
	}

	return(workspace);
}

int mwm_foreach_workspace(struct mwm *mwm,
			  int (*func)(struct mwm*, struct workspace*, void*),
			  void *data)
{
	loop_iter_t first;
	loop_iter_t cur;

	if(!mwm) {
		return(-EINVAL);
	}

	first = loop_get_iter(&mwm->workspaces);
	cur = first;

	do {
		struct workspace *workspace;

		workspace = (struct workspace*)loop_iter_get_data(cur);

		if(func(mwm, workspace, data) < 0) {
			break;
		}
		cur = loop_iter_get_next(cur);
	} while(cur != first);

	return(0);
}

int mwm_needs_redraw(struct mwm *mwm)
{
	if(!mwm) {
		return(-EINVAL);
	}

	mwm->needs_redraw = 1;
	return(0);
}

int mwm_redraw(struct mwm *mwm)
{
	if(!mwm) {
		return(-EINVAL);
	}

	if(mwm->needs_redraw) {
		loop_foreach(&mwm->monitors, (void(*)(void*))monitor_redraw);
		loop_foreach(&mwm->workspaces, (void(*)(void*))workspace_redraw);
		mwm->needs_redraw = 0;
	}

	return(0);
}

Window mwm_create_window(struct mwm *mwm, const int x, const int y, const int w, const int h)
{
	Window window;
	XSetWindowAttributes attrs;
	int depth;
	Visual *visual;
	unsigned long mask;

	attrs.override_redirect = True;
	attrs.background_pixmap = ParentRelative;
	attrs.event_mask = ExposureMask;

	mask = CWOverrideRedirect | CWBackPixmap | CWEventMask;
	depth = DefaultDepth(mwm->display, mwm->screen);
	visual = DefaultVisual(mwm->display, mwm->screen);

	window = XCreateWindow(mwm->display, mwm->root, x, y, w, h, 0,
			       depth, CopyFromParent, visual, mask, &attrs);

	return(window);
}

GC mwm_create_gc(struct mwm *mwm)
{
	GC context;

	context = XCreateGC(mwm->display, mwm->root, 0, NULL);

	XSetLineAttributes(mwm->display, context, 1, LineSolid, CapButt, JoinMiter);

	return(context);
}

XftDraw* mwm_create_xft_context(struct mwm *mwm, Drawable drawable)
{
	return(XftDrawCreate(mwm->display, drawable,
			     DefaultVisual(mwm->display, mwm->screen),
			     DefaultColormap(mwm->display, mwm->screen)));
}

Drawable mwm_create_pixmap(struct mwm *mwm, Window window, const int width, const int height)
{
	return(XCreatePixmap(mwm->display, window ? window : mwm->root, width, height,
			     DefaultDepth(mwm->display, mwm->screen)));
}

void mwm_free_pixmap(struct mwm *mwm, Drawable drawable)
{
	XFreePixmap(mwm->display, drawable);
	return;
}

int mwm_get_font_height(struct mwm *mwm)
{
	return(mwm->font.height);
}

int mwm_get_text_width(struct mwm *mwm, const char *text)
{
	PangoRectangle extents;

	pango_layout_set_attributes(mwm->font.layout, NULL);
	pango_layout_set_markup(mwm->font.layout, text, -1);
	pango_layout_get_extents(mwm->font.layout, 0, &extents);

	return(extents.width / PANGO_SCALE);
}

unsigned long mwm_get_color(struct mwm *mwm, mwm_palette_t palette, mwm_color_t color)
{
	return(mwm->palette[palette].color[color]);
}

int mwm_get_text_property(struct mwm *mwm, Window window, Atom atom, char *buffer, size_t buffer_size)
{
	XTextProperty property;
	int len;

	if(!mwm || !buffer || buffer_size == 0) {
		return(-EINVAL);
	}

	XGetTextProperty(mwm->display, window, &property, atom);

	if(property.nitems == 0) {
		return(-ENOENT);
	}

	if(property.encoding == XA_STRING) {
		len = snprintf(buffer, buffer_size, "%s", (char*)property.value);
	} else {
		len = -ENOSYS;
	}

	XFree(property.value);
	return(len);
}

int mwm_get_status(struct mwm *mwm, char *buffer, const size_t buffer_size)
{
	int len;

	len = mwm_get_text_property(mwm, mwm->root, XA_WM_NAME, buffer, buffer_size);

	if(len < 0) {
		return(snprintf(buffer, buffer_size, "mwm-0.1"));
	}

	return(len);
}

int mwm_grab_keys(struct mwm *mwm)
{
	extern struct key_binding config_keybindings[];
	struct key_binding *binding;

	if(!mwm) {
		return(-EINVAL);
	}

	XUngrabKey(mwm->display, AnyKey, AnyModifier, mwm->root);

	for(binding = config_keybindings; binding->cmd < MWM_CMD_MAX; binding++) {
		KeyCode code;

		code = XKeysymToKeycode(mwm->display, binding->key);

		XGrabKey(mwm->display, code, binding->mod,
			 mwm->root, True, GrabModeAsync, GrabModeAsync);
		XGrabKey(mwm->display, code, binding->mod | LockMask,
			 mwm->root, True, GrabModeAsync, GrabModeAsync);
	}

	return(0);
}

int mwm_cmd(struct mwm *mwm, mwm_cmd_t cmd, void *data)
{
#ifdef MWM_DEBUG
	static const char *cmd_names[] = {
		"quit",
		"spawn",
		"show_workspace",
		"move_to_workspace",
		"set_layout",
		"shift_focus",
		"shift_client",
		"shift_monitor_focus",
		"shift_workspace",
		"kbptr_move",
		"kbptr_click",
		"(invalid)"
	};
	int nameidx;

	nameidx = sizeof(cmd_names) / sizeof(cmd_names[0]);
	if(cmd >= 0 && cmd < nameidx) {
		nameidx = cmd;
	}

	fprintf(stderr, "%s(%p, %d [%s], %p)\n", __func__, (void*)mwm,
		cmd, cmd_names[nameidx], data);
#endif /* MWM_DEBUG */

	if(!mwm || cmd < 0 || cmd >= MWM_CMD_MAX) {
		return(-EINVAL);
	}

	if(!mwm->commands[cmd]) {
		return(-ENOSYS);
	}

	mwm->commands[cmd](mwm, data);
	return(0);
}

int mwm_get_atom(struct mwm *mwm, const char *name, long *dst)
{
	long atom;

	if(!mwm || !name || !dst) {
		return(-EINVAL);
	}

	/* FIXME: Cache the result */
	atom = XInternAtom(mwm->display, name, False);

	*dst = atom;
	return(0);
}
