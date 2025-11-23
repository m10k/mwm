#define _POSIX_C_SOURCE 200809L /* for strdup() */

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
#include <X11/Xft/Xft.h>
#include <X11/XKBlib.h>
#include <pango/pango.h>
#include <pango/pangoxft.h>
#include "mwm.h"
#include "event.h"
#include "keys.h"
#include "workspace.h"
#include "monitor.h"
#include "loop.h"
#include "x.h"
#include "common.h"
#include "client.h"
#include "theme.h"
#include "kbptr.h"
#include "xrandr.h"

typedef void (_mwm_xhandler_t)(XEvent*);
typedef int (event_conv_t)(XEvent*, struct event*);
typedef int (event_handler_t)(struct event*);

static const char *_mwm_atom_names[] = {
	[MWM_ATOM_HINT] = "MWM_HINT",
	[MWM_ATOM_UTF8] = "UTF8_STRING",
	[MWM_ATOM_MAX] = NULL
};

struct palette {
	unsigned long color[MWM_COLOR_MAX];
	XftColor xcolor[MWM_COLOR_MAX];
};

struct mwm {
	Display *display;
	int screen;
	Window root;
	struct geom root_geom;
	Atom atoms[MWM_ATOM_MAX];

	int running;
	int needs_redraw;

	struct {
		monitor_t current;
		monitor_t next;
		int changed;
	} focus;

	client_t focused_client;

	struct xrandr *xrandr;

	_mwm_xhandler_t *xhandler[LASTEvent];

	struct {
		PangoLayout *layout;
		PangoLayout *vlayout;
		int ascent;
		int descent;
		int height;
	} font[MWM_FONT_MAX];

	struct palette palette[MWM_PALETTE_MAX];

	void (*commands[MWM_CMD_MAX])(void*);

	int (*xerror_default_handler)(Display*, XErrorEvent*);
};

static struct mwm *_mwm;

static int _xerror_startup(Display *display, XErrorEvent *event);
static int _xerror_handle(Display *display, XErrorEvent *event);
static int _xerror_nop(Display *display, XErrorEvent *event);

static int mwm_new(struct mwm **mwm);
static int mwm_free(struct mwm **mwm);

static void _xev_configure_request(XEvent *event)
{
	XConfigureRequestEvent *configure_request;
	client_t client;

	/*
	 * This event is generated whenever the client attempts to resize itself.
	 * If the client is floating, or the viewer's layout is floating, we will
	 * accept the resize request.
	 * Otherwise we will override the request with the values that we have
	 * stored in the client structure.
	 */
#if MWM_DEBUG
	fprintf(stderr, "%s(%p)\n", __func__, (void*)event);
#endif /* MWM_DEBUG */

	configure_request = &event->xconfigurerequest;

	if ((client = client_of_window(configure_request->window)) < 0) {
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

		XConfigureWindow(_mwm->display, configure_request->window,
		                 value_mask, &changes);
	} else {
		/* Clients don't get to choose their geometry */
		client_set_state(client, NormalState);
	}

	XSync(_mwm->display, False);

	return;
}

static void _xev_configure_notify(XEvent *event)
{
	XConfigureEvent *cevent;

#if MWM_DEBUG
	fprintf(stderr, "%s(%p)\n", __func__, (void*)event);
#endif /* MWM_DEBUG */

	cevent = &event->xconfigure;

	if(cevent->window != _mwm->root) {
		return;
	}

	if(x_get_geom(_mwm->display, _mwm->root, &_mwm->root_geom) < 0) {
		mwm_stop();
	}

	return;
}

static void _xev_destroy_notify(XDestroyWindowEvent *event)
{
	client_t client;

	/* get the client and detach it */
#if MWM_DEBUG
	fprintf(stderr, "%s(%p)\n", __func__, (void*)event);
#endif /* MWM_DEBUG */

	if ((client = client_of_window(event->window)) < 0) {
#if MWM_DEBUG
		fprintf(stderr, "Couldn't find client of window 0x%lx\n", event->window);
#endif /* MWM_DEBUG */
		return;
	}

	if (mwm_detach_client(client) < 0) {
#if MWM_DEBUG
		fprintf(stderr, "Couldn't detach client %ld\n", client);
#endif /* MWM_DEBUG */
		return;
	}

	client_free(client);
	return;
}

static void _xev_enter_notify(XCrossingEvent *event)
{
	client_t client;
	monitor_t monitor;

	/* pointer has entered a window - move focus, if it makes sense */
#if MWM_DEBUG
	fprintf(stderr, "%s(%p)\n", __func__, (void*)event);
#endif /* MWM_DEBUG */

	if ((event->mode != NotifyNormal || event->detail == NotifyInferior) &&
	    event->window == _mwm->root) {
		return;
	}

	if ((client = client_of_window(event->window)) >= 0) {
		mwm_focus_client(client);
	}

	if ((monitor = monitor_of_window(event->window)) >= 0) {
		mwm_focus_monitor(monitor);
	}

	return;
}

static void _xev_expose(XExposeEvent *event)
{
	monitor_t monitor;

#if MWM_DEBUG
	fprintf(stderr, "%s(%p) W=0x%lx\n", __func__, (void*)event, event->window);
#endif /* MWM_DEBUG */

	/* redraw the status bar, if we have one */

	if (event->count > 0) {
		return;
	}

	if ((monitor = monitor_of_window(event->window)) >= 0) {
		monitor_needs_redraw(monitor);
	}

	return;
}

static void _xev_focus_in(XFocusInEvent *event)
{
        client_t client;

	/* move focus to the client referenced by the event */
#if MWM_DEBUG
	fprintf(stderr, "%s(%p)\n", __func__, (void*)event);
#endif /* MWM_DEBUG */

	if ((client = client_of_window(event->window)) >= 0) {
		mwm_focus_client(client);
	}
}

static void _xev_key_press(XKeyEvent *event)
{
	extern struct key_binding config_keybindings[];
	struct key_binding *binding;
	KeySym keysym;
	unsigned int mask;

#if MWM_DEBUG
	fprintf(stderr, "%s(%p)\n", __func__, (void*)event);
#endif /* MWM_DEBUG */

#define BUTTONMASK              (ButtonPressMask | ButtonReleaseMask)
#define ALLMODMASK              (Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask)
#define ALLMASK                 (ShiftMask | ControlMask | ALLMODMASK)
#define CLEANMASK(mask)         (mask & ~LockMask & ALLMASK)

	/* handle keyboard shortcuts */

	keysym = XkbKeycodeToKeysym(_mwm->display, event->keycode, 0, 0);
	mask = CLEANMASK(event->state);

	for(binding = config_keybindings; binding->cmd < MWM_CMD_MAX; binding++) {
		if(keysym == binding->key && mask == CLEANMASK(binding->mod)) {
			mwm_cmd(binding->cmd, binding->arg);
		}
	}

#undef BUTTONMASK
#undef ALLMODMASK
#undef ALLMASK
#undef CLEANMASK
	return;
}

static void _xev_mapping_notify(XMappingEvent *event)
{
#if MWM_DEBUG
	fprintf(stderr, "%s(%p)\n", __func__, (void*)event);
#endif /* MWM_DEBUG */

	XRefreshKeyboardMapping(event);

	if(event->request == MappingKeyboard) {
		mwm_grab_keys();
	}

	return;
}

static void _xev_map_request(XMapRequestEvent *event)
{
	XWindowAttributes attrs;

#if MWM_DEBUG
	fprintf(stderr, "%s(%p)\n", __func__, (void*)event);
#endif /* MWM_DEBUG */

	if(!XGetWindowAttributes(_mwm->display, event->window, &attrs)) {
		return;
	}

	if(attrs.override_redirect) {
		return;
	}

	if (client_of_window(event->window) < 0) {
		client_t client;

		if ((client = client_new(event->window)) < 0) {
			/* ENOMEM */
			return;
		}

		if(mwm_attach_client(client) < 0) {
			/* ENOMEM */
			client_free(client);
			return;
		}
	}

	return;
}

static void _xev_motion_notify(XMotionEvent *event)
{
	monitor_t monitor;
	struct geom pointer_geom;

#if MWM_DEBUG
	fprintf(stderr, "%s(%p)\n", __func__, (void*)event);
#endif /* MWM_DEBUG */

	/* move focus to the monitor referenced in the event */
	/* printf("%s(%p, %p)\n", __func__, (void*)mwm, (void*)event); */

	pointer_geom.x = event->x_root;
	pointer_geom.y = event->y_root;
	pointer_geom.w = 1;
	pointer_geom.h = 1;

	if ((monitor = monitor_at(pointer_geom)) < 0) {
		return;
	}

	if(mwm_get_focused_monitor() != monitor) {
		mwm_focus_monitor(monitor);
	}

	return;
}

static void _xev_property_notify(XPropertyEvent *event)
{
#if MWM_DEBUG
	fprintf(stderr, "%s(%p)\n", __func__, (void*)event);
#endif /* MWM_DEBUG */

	/* FIXME: Property notification handling must be implemented more thoroughly */

	if ((event->window == _mwm->root)) {
		/* if(event->atom == XA_WM_NAME) */
		mwm_needs_redraw();
	} else {
		client_t event_client;

		if ((event_client = client_of_window(event->window)) >= 0) {
			client_property_notify(event_client, event);
		}
	}

	return;
}

static void _xev_unmap_notify(XUnmapEvent *event)
{
	client_t client;

#if MWM_DEBUG
	fprintf(stderr, "%s(%p)\n", __func__, (void*)event);
#endif /* MWM_DEBUG */

	if ((client = client_of_window(event->window)) < 0) {
		return;
	}

	XGrabServer(_mwm->display);
	XSync(_mwm->display, False);
	XSetErrorHandler(_xerror_nop);

	client_set_state(client, WithdrawnState);

	if(!event->send_event) {
		mwm_detach_client(client);
		client_free(client);
	}

	XSync(_mwm->display, False);
	XSetErrorHandler(_xerror_handle);
	XUngrabServer(_mwm->display);

	return;
}

static void _attach_monitor(struct xrandr *xrr,
                            xrandr_crtc_t crtc,
                            struct geom *geom,
                            void *data)
{
	monitor_t monitor;
	int err;

	if ((monitor = monitor_new(crtc, *geom)) < 0) {
		fprintf(stderr, "Could not create monitor for CRTC 0x%lx: %s\n", crtc, strerror(-monitor));
		return;
	}

	if ((err = mwm_attach_monitor(monitor)) < 0) {
		fprintf(stderr, "Could not attach monitor for CRTC 0x%lx: %s\n", crtc, strerror(-err));
		monitor_free(monitor);
	}

	return;
}

static void _detach_monitor(struct xrandr *xrr,
                            xrandr_crtc_t crtc,
                            struct geom *geom,
                            void *data)
{
	monitor_t monitor;

	if ((monitor = monitor_of_crtc(crtc)) < 0) {
		fprintf(stderr, "Monitor of CRTC 0x%lx is not attached\n", crtc);
		return;
	}

	mwm_detach_monitor(monitor);
	monitor_free(monitor);

	return;
}

static void _change_monitor_geometry(struct xrandr *xrr,
                                     xrandr_crtc_t crtc,
                                     struct geom *geom,
                                     void *data)
{
	monitor_t monitor;

	if ((monitor = monitor_of_crtc(crtc)) < 0) {
		fprintf(stderr, "Could not find monitor of CRTC 0x%lx: %s\n",
		        crtc, strerror(-monitor));
		return;
	}

	fprintf(stderr, "CRTC 0x%lx changed geometry to [%dx%d @ %d,%d]\n",
	        crtc, geom->w, geom->h, geom->x, geom->y);
	monitor_set_geometry(monitor, geom);
	return;
}

static int mwm_new(struct mwm **dst)
{
	struct mwm *mwm;
	int err;
	int i;

	err = 0;

	if(!dst) {
		return(-EINVAL);
	}

	if (!(mwm = calloc(1, sizeof(*mwm)))) {
		return -ENOMEM;
	}

	for(i = 0; i < 12; i++) {
		workspace_t ws;

		if ((ws = workspace_new(i)) < 0) {
			err = (int)-ws;
			goto cleanup;
		}
	}

	mwm->xhandler[ConfigureRequest] = (_mwm_xhandler_t*)_xev_configure_request;
	mwm->xhandler[ConfigureNotify]  = (_mwm_xhandler_t*)_xev_configure_notify;
	mwm->xhandler[DestroyNotify]    = (_mwm_xhandler_t*)_xev_destroy_notify;
	mwm->xhandler[EnterNotify]      = (_mwm_xhandler_t*)_xev_enter_notify;
	mwm->xhandler[Expose]           = (_mwm_xhandler_t*)_xev_expose;
	mwm->xhandler[FocusIn]          = (_mwm_xhandler_t*)_xev_focus_in;
	mwm->xhandler[KeyPress]         = (_mwm_xhandler_t*)_xev_key_press;
	mwm->xhandler[MappingNotify]    = (_mwm_xhandler_t*)_xev_mapping_notify;
	mwm->xhandler[MapRequest]       = (_mwm_xhandler_t*)_xev_map_request;
	mwm->xhandler[MotionNotify]     = (_mwm_xhandler_t*)_xev_motion_notify;
	mwm->xhandler[PropertyNotify]   = (_mwm_xhandler_t*)_xev_property_notify;
	mwm->xhandler[UnmapNotify]      = (_mwm_xhandler_t*)_xev_unmap_notify;

cleanup:
	if(err < 0) {
		mwm_free(&mwm);
	} else {
		*dst = mwm;
	}

	return(err);
}

static int mwm_free(struct mwm **mwm)
{
	if (!mwm) {
		return -EINVAL;
	}

	if (!*mwm) {
		return -EALREADY;
	}

	if ((*mwm)->display) {
		XCloseDisplay((*mwm)->display);
	}

	free(*mwm);
	*mwm = NULL;

	return 0;
}

int mwm_cleanup(void)
{
	return mwm_free(&_mwm);
}

Display* mwm_get_display(void)
{
	return(_mwm->display);
}

Window mwm_get_root_window(void)
{
	return(_mwm->root);
}

static int _color_init(unsigned long *color,
		       XftColor *xcolor,
		       const char *colorspec)
{
	Visual *visual;
	Colormap colormap;

	visual = DefaultVisual(_mwm->display, _mwm->screen);
	colormap = DefaultColormap(_mwm->display, _mwm->screen);

	if (!XftColorAllocName(_mwm->display, visual, colormap,
			      colorspec, xcolor)) {
		return -EIO;
	}

	*color = xcolor->pixel;
	return 0;
}

static int _palette_init(struct palette *palette,
			 union colorset *colorset)
{
	int i;

	for (i = 0; i < MWM_COLOR_MAX; i++) {
		_color_init(&palette->color[i],
			    &palette->xcolor[i],
			    colorset->indexed[i]);
	}

	return 0;
}

void _sigchld(int unused)
{
	if(signal(SIGCHLD, _sigchld) == SIG_ERR) {
		perror("signal");
		exit(1);
	}

	while (waitpid(-1, NULL, WNOHANG) > 0);

	return;
}

#if MWM_DEBUG
static void _handle_signal(int sig)
{
	fprintf(stderr,
	        "MWM %p\n"
	        "    Screen:         %d\n"
	        "    Root window:    %lx\n"
	        "    Root geometry:  %dx%d @ %dx%d\n"
	        "    Running:        %d\n"
	        "    Needs redraw:   %d\n"
	        "    Current focus:  %ld\n"
	        "    Next focus:     %ld\n"
	        "    Focus changed:  %d\n"
	        "    Focused client: %ld\n",
	        (void*)_mwm,
	        _mwm->screen,
	        _mwm->root,
	        _mwm->root_geom.w, _mwm->root_geom.h, _mwm->root_geom.x, _mwm->root_geom.y,
	        _mwm->running,
	        _mwm->needs_redraw,
	        _mwm->focus.current,
	        _mwm->focus.next,
	        _mwm->focus.changed,
	        _mwm->focused_client);

	fprintf(stderr, "----- BEGIN monitors -----\n");
	monitor_foreach((int(*)(const monitor_t, void*))monitor_dump, NULL);
	fprintf(stderr, "----- END monitors -----\n");

	fprintf(stderr, "----- BEGIN workspaces -----\n");
	workspace_foreach((int(*)(const workspace_t, void*))workspace_dump, NULL);
	fprintf(stderr, "----- END workspaces -----\n");
}

static void _setup_sigusr_handler(void)
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_handler = _handle_signal;

	if (sigaction(SIGUSR1, &act, NULL) < 0) {
		perror("sigaction");
		exit(1);
	}
}
#endif /* MWM_DEBUG */

static void _cmd_spawn(void *arg)
{
        char **argv;
	pid_t pid;

	argv = (char**)arg;
	pid = fork();

	if(pid == 0) {
		close(ConnectionNumber(_mwm->display));
		setsid();
		execvp(*argv, argv);
		exit(0);
	}

	return;
}

static void _cmd_show_workspace(void *arg)
{
	monitor_t monitor;
	workspace_t workspace;
	long number;

	number = (long)arg;
	monitor = mwm_get_focused_monitor();

	workspace = workspace_number(number);

	monitor_set_workspace(monitor, workspace);
	return;
}

static void _cmd_move_to_workspace(void *arg)
{
	workspace_t src_workspace;
	workspace_t dst_workspace;
	client_t client;
	long dst_number;

	src_workspace = -1;
	dst_number = (long)arg;
	client = mwm_get_focused_client();

	if (client < 0) {
		return;
	}

	dst_workspace = workspace_number(dst_number);
	if (dst_workspace < 0) {
		return;
	}

	if (client_get_workspace(client, &src_workspace) < 0) {
		return;
	}

	if (src_workspace != dst_workspace) {
		if (src_workspace >= 0) {
			workspace_detach_client(src_workspace, client);
		}
		workspace_attach_client(dst_workspace, client);
	}

	return;
}

static void _cmd_set_layout(void *arg)
{
	extern struct layout *layouts[];
	struct layout *layout;
	monitor_t monitor;
	long num;
	int err;

	num = (long)arg;
	monitor = mwm_get_focused_monitor();

	err = monitor_get_layout(monitor, &layout);
	if (!err && layout != layouts[num]) {
		monitor_set_layout(monitor, layouts[num]);
		monitor_needs_redraw(monitor);
	}

	return;
}

static void _cmd_shift_focus(void *arg)
{
	workspace_t workspace;
	long dir;

	dir = (long)arg;

	workspace = mwm_get_focused_workspace();
	if (workspace >= 0) {
		workspace_shift_focus(workspace, dir);
	}

	return;
}

static void _cmd_shift_client(void *arg)
{
	workspace_t workspace;
	long dir;

	dir = (long)arg;

	workspace = mwm_get_focused_workspace();
	if (workspace >= 0) {
		workspace_shift_client(workspace, -1, dir);
	}

	return;
}

static void _cmd_shift_monitor_focus(void *arg)
{
	monitor_t src_monitor;
	monitor_t dst_monitor;
	client_t src_client;
	long dir;

	dir = (long)arg;
	/* move focus to previous or next monitor */

	if (dir == 0) {
		return;
	}

	src_monitor = mwm_get_focused_monitor();
	dst_monitor = src_monitor + dir;

	if (monitor_get_focused_client(src_monitor, &src_client) && src_client >= 0) {
		client_save_pointer(src_client);
	}

	mwm_focus_monitor(dst_monitor);
	monitor_needs_redraw(src_monitor);
	monitor_needs_redraw(dst_monitor);
	return;
}

static void _cmd_shift_workspace(void *arg)
{
	monitor_t src_monitor;
	monitor_t dst_monitor;
        workspace_t workspace;
	long dir;

	/* move workspace to another monitor */

	dir = (long)arg;
	workspace = -1;

	src_monitor = mwm_get_focused_monitor();
	if (src_monitor < 0) {
		return;
	}
	dst_monitor = src_monitor + dir;

	monitor_get_workspace(src_monitor, &workspace);
	monitor_set_workspace(dst_monitor, workspace);
	mwm_focus_monitor(dst_monitor);

	return;
}

static void _cmd_quit(void *arg)
{
	mwm_stop();
	return;
}

static void _cmd_kbptr_move(void *arg)
{
	client_t client;
	long dir;

	dir = (long)arg;
	client = mwm_get_focused_client();

	if (client) {
		kbptr_move(client, dir);
	}

	return;
}

static void _cmd_kbptr_click(void *arg)
{
	client_t client;
	long button;

	button = (long)arg;
	client = mwm_get_focused_client();

	if (client) {
		kbptr_click(client, button);
	}

	return;
}

static int _xerror_startup(Display *display, XErrorEvent *event)
{
	fprintf(stderr, "Looks like I'm not your only window manager\n");
	exit(1);
	return -1;
}

static int _xerror_nop(Display *display, XErrorEvent *event)
{
	return 0;
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

	if (event->error_code == BadWindow) {
		return 1;
	}

	for (i = 0; i < (sizeof(ignore_ok) / sizeof(ignore_ok[0])); i++) {
		if (event->request_code == ignore_ok[i].request_code &&
		    event->error_code == ignore_ok[i].error_code) {
			return 1;
		}
	}

	return 0;
}

static int _xerror_handle(Display *display, XErrorEvent *event)
{
	if (_can_ignore_error(event)) {
		return 0;
	}

	return _mwm->xerror_default_handler(display, event);
}

static void _find_existing_clients(void)
{
	Window dontcare;
	Window *windows;
	Window *cur;
	unsigned int num_windows;

	if (!XQueryTree(_mwm->display, _mwm->root, &dontcare, &dontcare, &windows, &num_windows)) {
		return;
	}

	for (cur = windows; cur < windows + num_windows; cur++) {
		XWindowAttributes attrs;

		if (!XGetWindowAttributes(_mwm->display, *cur, &attrs)) {
			continue;
		}

		if (attrs.override_redirect || XGetTransientForHint(_mwm->display, *cur, &dontcare)) {
			continue;
		}

		if (attrs.map_state == IsViewable /* || IconicState */ ) {
			client_t client;
			int err;

			if ((client = client_new(*cur)) < 0) {
				fprintf(stderr, "%s: client_new: %s\n", __func__, strerror(-client));
			} else if ((err = mwm_attach_client(client)) < 0) {
				fprintf(stderr, "%s: mwm_attach_client: %s\n", __func__, strerror(-err));
				client_free(client);
			}
		}
	}

	for (cur = windows; cur < windows + num_windows; cur++) {
		XWindowAttributes attrs;

		if (!XGetWindowAttributes(_mwm->display, *cur, &attrs)) {
			continue;
		}

		if (!XGetTransientForHint(_mwm->display, *cur, &dontcare)) {
			continue;
		}

		if (attrs.map_state == IsViewable /* || IconicState */ ) {
			client_t client;
			int err;

			if ((client = client_new(*cur)) < 0) {
				fprintf(stderr, "%s: client_new: %s\n", __func__, strerror(-client));
			} else if ((err = mwm_attach_client(client)) < 0) {
				fprintf(stderr, "%s: mwm_attach_client: %s\n", __func__, strerror(-err));
				client_free(client);
			}
		}
	}

	if (windows) {
		XFree(windows);
	}

	return;
}

static int _load_font(const mwm_font_t idx, const char *font_desc)
{
	PangoContext *context;
	PangoFontMap *fontmap;
	PangoFontDescription *fontdesc;
	PangoFontMetrics *fontmetrics;

	fontmap = pango_xft_get_font_map(_mwm->display, _mwm->screen);
	fontdesc = pango_font_description_from_string(font_desc);

	/* horizontal text */
	context = pango_font_map_create_context(fontmap);
	_mwm->font[idx].layout = pango_layout_new(context);
	pango_layout_set_font_description(_mwm->font[idx].layout, fontdesc);
	fontmetrics = pango_context_get_metrics(context, fontdesc, NULL);
	g_object_unref(context);

	_mwm->font[idx].ascent = pango_font_metrics_get_ascent(fontmetrics) / PANGO_SCALE;
	_mwm->font[idx].descent = pango_font_metrics_get_descent(fontmetrics) / PANGO_SCALE;
	_mwm->font[idx].height = _mwm->font[idx].ascent + _mwm->font[idx].descent;
	pango_font_metrics_unref(fontmetrics);

	/* vertical text */
	context = pango_font_map_create_context(fontmap);
	_mwm->font[idx].vlayout = pango_layout_new(context);
	pango_layout_set_font_description(_mwm->font[idx].vlayout, fontdesc);
	g_object_unref(context);

	return 0;
}

int mwm_init(void)
{
	extern struct theme config_theme;
	int err;
	int i;

	if (_mwm) {
		return -EALREADY;
	}

	if ((err = mwm_new(&_mwm)) < 0) {
		return err;
	}

	_mwm->display = XOpenDisplay(NULL);

	if (!_mwm->display) {
		return -EIO;
	}

	_sigchld(0);
#if MWM_DEBUG
	_setup_sigusr_handler();
#endif /* MWM_DEBUG */

	_mwm->screen = DefaultScreen(_mwm->display);
	_mwm->root = RootWindow(_mwm->display, _mwm->screen);
	_mwm->xerror_default_handler = XSetErrorHandler(_xerror_startup);

	if (!_mwm->xerror_default_handler) {
		return -EIO;
	}

	for (i = 0; i < (sizeof(_mwm->atoms) / sizeof(_mwm->atoms[0])); i++) {
		mwm_get_atom_by_name(_mwm_atom_names[i], &_mwm->atoms[i]);
	}

	if ((err = xrandr_new(&_mwm->xrandr, _mwm->display, _mwm->root)) < 0) {
		switch (err) {
		case -ENOTSUP:
			fprintf(stderr, "XRandR is required but not supported by the X server\n");
			break;

		case -ENOMEM:
			fprintf(stderr, "Not enough memory to initialize XRandR extension\n");
			break;

		default:
			fprintf(stderr, "Bug in initialization of XRandR extension\n");
			break;
		}

		return err;
	}

	xrandr_set_callback(_mwm->xrandr, XRANDR_MONITOR_ATTACHED,
	                    (xrandr_func_t*)_attach_monitor, NULL);
	xrandr_set_callback(_mwm->xrandr, XRANDR_MONITOR_DETACHED,
	                    (xrandr_func_t*)_detach_monitor, NULL);
	xrandr_set_callback(_mwm->xrandr, XRANDR_MONITOR_GEOMETRY_CHANGED,
	                    (xrandr_func_t*)_change_monitor_geometry, NULL);

	XSelectInput(_mwm->display, _mwm->root,
		     SubstructureRedirectMask |
		     SubstructureNotifyMask |
		     PointerMotionMask |
		     EnterWindowMask |
		     LeaveWindowMask |
		     StructureNotifyMask |
		     PropertyChangeMask);
	XSync(_mwm->display, False);

	XSetErrorHandler(_xerror_handle);
	XSync(_mwm->display, False);

	mwm_grab_keys();

	x_configure_notify(_mwm->display, _mwm->root, NULL, 0);

	_load_font(MWM_FONT_STATUSBAR, config_theme.statusbar_font);
	_load_font(MWM_FONT_INDICATOR, config_theme.indicator_font);

	_palette_init(&(_mwm->palette[MWM_PALETTE_ACTIVE]),
		      &config_theme.active);
	_palette_init(&(_mwm->palette[MWM_PALETTE_INACTIVE]),
		      &config_theme.inactive);

	_mwm->commands[MWM_CMD_QUIT] = _cmd_quit;
	_mwm->commands[MWM_CMD_SPAWN] = _cmd_spawn;
	_mwm->commands[MWM_CMD_SHOW_WORKSPACE] = _cmd_show_workspace;
	_mwm->commands[MWM_CMD_MOVE_TO_WORKSPACE] = _cmd_move_to_workspace;
	_mwm->commands[MWM_CMD_SET_LAYOUT] = _cmd_set_layout;
	_mwm->commands[MWM_CMD_SHIFT_FOCUS] = _cmd_shift_focus;
	_mwm->commands[MWM_CMD_SHIFT_CLIENT] = _cmd_shift_client;
	_mwm->commands[MWM_CMD_SHIFT_MONITOR_FOCUS] = _cmd_shift_monitor_focus;
	_mwm->commands[MWM_CMD_SHIFT_WORKSPACE] = _cmd_shift_workspace;
	_mwm->commands[MWM_CMD_KBPTR_MOVE] = _cmd_kbptr_move;
	_mwm->commands[MWM_CMD_KBPTR_CLICK] = _cmd_kbptr_click;

	xrandr_update(_mwm->xrandr);
	_find_existing_clients();

	return 0;
}

int mwm_render_text(XftDraw *drawable, const mwm_font_t font,
                    mwm_palette_t palette, const char *text,
                    const int x, const int y,
                    const int w, const int h)
{
	XftColor *color;

	if (!drawable || !text) {
		return -EINVAL;
	}

	color = &_mwm->palette[palette].xcolor[MWM_COLOR_TEXT];

	pango_layout_set_attributes(_mwm->font[font].layout, NULL);
	pango_layout_set_width(_mwm->font[font].layout, w * PANGO_SCALE);
	pango_layout_set_height(_mwm->font[font].layout, h * PANGO_SCALE);
	pango_layout_set_ellipsize(_mwm->font[font].layout, PANGO_ELLIPSIZE_END);
	pango_layout_set_wrap(_mwm->font[font].layout, PANGO_WRAP_CHAR);

	pango_layout_set_markup(_mwm->font[font].layout, text, -1);
	pango_xft_render_layout(drawable, color,
				_mwm->font[font].layout,
				x * PANGO_SCALE,
				y * PANGO_SCALE);

	return 0;
}

int mwm_render_text_vertical(XftDraw *drawable, const mwm_font_t font,
                             mwm_palette_t palette, const char *text,
                             const int x, const int y,
                             const int w, const int h)
{
	PangoMatrix matrix = PANGO_MATRIX_INIT;
	PangoContext *context;
	XftColor *color;
	PangoRectangle extents;

	if (!drawable || !text) {
		return -EINVAL;
	}

	context = pango_layout_get_context(_mwm->font[font].vlayout);
	color = &_mwm->palette[palette].xcolor[MWM_COLOR_TEXT];

	pango_matrix_translate(&matrix, x, y);
	pango_matrix_rotate(&matrix, -90.0);
	pango_context_set_matrix(context, &matrix);
	pango_context_set_base_gravity(context, PANGO_GRAVITY_EAST);

	pango_layout_set_attributes(_mwm->font[font].vlayout, NULL);
	pango_layout_set_width(_mwm->font[font].vlayout, w * PANGO_SCALE);
	pango_layout_set_height(_mwm->font[font].vlayout, h * PANGO_SCALE);
	pango_layout_set_ellipsize(_mwm->font[font].vlayout, PANGO_ELLIPSIZE_END);
	pango_layout_set_wrap(_mwm->font[font].vlayout, PANGO_WRAP_CHAR);
	pango_layout_set_markup(_mwm->font[font].vlayout, text, -1);
	pango_layout_get_extents(_mwm->font[font].vlayout, NULL, &extents);

	pango_xft_render_layout(drawable, color, _mwm->font[font].vlayout,
				0, -1.0 * extents.height);

	return 0;
}

static int _xevent_to_configure_request(XConfigureRequestEvent *xevent, struct event *event)
{
	event->data.configure_request.window     = xevent->window;
	event->data.configure_request.geom.x     = xevent->x;
	event->data.configure_request.geom.y     = xevent->y;
	event->data.configure_request.geom.w     = xevent->width;
	event->data.configure_request.geom.h     = xevent->height;
	event->data.configure_request.above      = xevent->above;
	event->data.configure_request.detail     = xevent->detail;
	event->data.configure_request.value_mask = xevent->value_mask;
	event->data.configure_request.client     = client_of_window(xevent->window);

	return 0;
}

static int _xevent_to_configure_notify(XConfigureEvent *xevent, struct event *event)
{
	event->data.configure_notify.window = xevent->window;
	event->data.configure_notify.geom.x = xevent->x;
	event->data.configure_notify.geom.y = xevent->y;
	event->data.configure_notify.geom.w = xevent->width;
	event->data.configure_notify.geom.h = xevent->height;

	return 0;
}

static int _xevent_to_destroy_notify(XDestroyWindowEvent *xevent, struct event *event)
{
	client_t client;
	int err;

	client = client_of_window(xevent->window);

	if (client < 0) {
		fprintf(stderr, "%s: Could not find client of window 0x%lx\n",
		        __func__, xevent->window);
		err = -EFAULT;
	} else {
		event->data.destroy_notify.client = client;
		err = 0;
	}

	return err;
}

static int _xevent_to_enter_notify(XCrossingEvent *xevent, struct event *event)
{
	event->data.enter_notify.window  = xevent->window;
	event->data.enter_notify.mode    = xevent->mode;
	event->data.enter_notify.detail  = xevent->detail;
	event->data.enter_notify.client  = client_of_window(xevent->window);
	event->data.enter_notify.monitor = monitor_of_window(xevent->window);

	return 0;
}

static int _xevent_to_expose(XExposeEvent *xevent, struct event *event)
{
	event->data.expose.count   = xevent->count;
	event->data.expose.window  = xevent->window;
	event->data.expose.monitor = monitor_of_window(xevent->window);

	return 0;
}

static int _xevent_to_focus_in(XFocusInEvent *xevent, struct event *event)
{
	event->data.focus_in.window = xevent->window;
	event->data.focus_in.client = client_of_window(xevent->window);

	return 0;
}

static int _xevent_to_key_press(XKeyEvent *xevent, struct event *event)
{
	event->data.key_press.keysym = XkbKeycodeToKeysym(_mwm->display, xevent->keycode, 0, 0);
	event->data.key_press.mask   = xevent->state;

	return 0;
}

static int _xevent_to_mapping_notify(XMappingEvent *xevent, struct event *event)
{
	memcpy(&event->data.mapping_notify.xevent,
	       xevent,
	       sizeof(event->data.mapping_notify.xevent));

	return 0;
}

static int _xevent_to_map_request(XMapRequestEvent *xevent, struct event *event)
{
	event->data.map_request.window = xevent->window;
	event->data.map_request.client = client_of_window(xevent->window);

	return 0;
}

static int _xevent_to_motion_notify(XMotionEvent *xevent, struct event *event)
{
	/* TODO: Remove this in the future */
	event->data.motion_notify.pointer.x = xevent->x_root;
	event->data.motion_notify.pointer.y = xevent->y_root;
	event->data.motion_notify.pointer.w = 1;
	event->data.motion_notify.pointer.h = 1;

	event->data.motion_notify.client = client_of_window(xevent->window);
	event->data.motion_notify.monitor = monitor_of_window(xevent->window);

	return 0;
}

static int _xevent_to_property_notify(XPropertyEvent *xevent, struct event *event)
{
	event->data.property_notify.window = xevent->window;
	event->data.property_notify.client = client_of_window(xevent->window);
	memcpy(&event->data.property_notify.xevent,
	       xevent,
	       sizeof(event->data.property_notify.xevent));

	return 0;
}

static int _xevent_to_unmap_notify(XUnmapEvent *xevent, struct event *event)
{
	client_t client;
	int err;

	if ((client = client_of_window(xevent->window))) {
		/* No client associated with this window. This is probably fine. */
		err = -ENOENT;
	} else {
		event->data.unmap_notify.client = client;
		err = 0;
	}

	return err;
}

static int _event_configure_request_handler(struct event *event)
{
	if (event->data.configure_request.client < 0) {
		XWindowChanges changes;
		unsigned int value_mask;

		changes.x = event->data.configure_request.geom.x;
		changes.y = event->data.configure_request.geom.y;
		changes.width = event->data.configure_request.geom.w;
		changes.height = event->data.configure_request.geom.h;
		changes.border_width = 0;
		changes.sibling = event->data.configure_request.above;
		changes.stack_mode = event->data.configure_request.detail;
		value_mask = event->data.configure_request.value_mask | CWBorderWidth;

		XConfigureWindow(_mwm->display, event->data.configure_request.window,
		                 value_mask, &changes);
	} else {
		client_set_state(event->data.configure_request.client, NormalState);
	}

	XSync(_mwm->display, False);

	return 0;
}

static int _event_configure_notify_handler(struct event *event)
{
	if (event->data.configure_notify.window == _mwm->root &&
	    x_get_geom(_mwm->display, _mwm->root, &_mwm->root_geom) < 0) {
		mwm_stop();
	}

	return 0;
}

static int _event_destroy_notify_handler(struct event *event)
{
	client_t client;
	int err;

	client = event->data.destroy_notify.client;

	if (client >= 0) {
		if ((err = mwm_detach_client(client)) < 0) {
			fprintf(stderr, "%s: Could not detach client %ld\n", __func__, client);
			return err;
		}

		client_free(client);
	}

	return 0;
}

static int _event_enter_notify_handler(struct event *event)
{
	if ((event->data.enter_notify.mode != NotifyNormal ||
	     event->data.enter_notify.detail == NotifyInferior) &&
	    event->data.enter_notify.window == _mwm->root) {
		return 0;
	}

	if (event->data.enter_notify.client >= 0) {
		mwm_focus_client(event->data.enter_notify.client);
	}
	if (event->data.enter_notify.monitor >= 0) {
		mwm_focus_monitor(event->data.enter_notify.monitor);
	}

	return 0;
}

static int _event_expose_handler(struct event *event)
{
	if (event->data.expose.count == 0 &&
	    event->data.expose.monitor >= 0) {
		monitor_needs_redraw(event->data.expose.monitor);
	}

	return 0;
}

static int _event_focus_in_handler(struct event *event)
{
	if (event->data.focus_in.client >= 0) {
		mwm_focus_client(event->data.focus_in.window);
	}

	return 0;
}

static int _event_key_press_handler(struct event *event)
{
	extern struct key_binding config_keybindings[];
	struct key_binding *binding;
	KeySym keysym;
	unsigned int mask;

#define BUTTONMASK      (ButtonPressMask | ButtonReleaseMask)
#define ALLMODMASK      (Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask)
#define ALLMASK         (ShiftMask | ControlMask | ALLMODMASK)
#define CLEANMASK(mask) ((mask) & ~LockMask & ALLMASK)

	keysym = event->data.key_press.keysym;
	mask = CLEANMASK(event->data.key_press.mask);

	for (binding = config_keybindings; binding->cmd < MWM_CMD_MAX; binding++) {
		if (keysym == binding->key &&
		    mask == CLEANMASK(binding->mod)) {
			mwm_cmd(binding->cmd, binding->arg);
		}
	}

#undef BUTTONMASK
#undef ALLMODMASK
#undef ALLMASK
#undef CLEANMASK

	return 0;
}

static int _event_mapping_notify_handler(struct event *event)
{
	XRefreshKeyboardMapping(&event->data.mapping_notify.xevent);

	if (event->data.mapping_notify.xevent.request == MappingKeyboard) {
		mwm_grab_keys();
	}

	return 0;
}

static int _event_map_request_handler(struct event *event)
{
	XWindowAttributes attrs;

	if (!XGetWindowAttributes(_mwm->display,
	                          event->data.map_request.window,
	                          &attrs)) {
		return 0;
	}

	if (attrs.override_redirect) {
		return 0;
	}

	if (event->data.map_request.client < 0) {
		client_t client;
		int err;

		if ((client = client_new(event->data.map_request.window)) < 0) {
			return (int)client;
		}

		if ((err = mwm_attach_client(client)) < 0) {
			client_free(client);
			return err;
		}
	}

	return 0;
}

static int _event_motion_notify_handler(struct event *event)
{
	monitor_t monitor;

	/* TODO: Don't handle this event in the future */

	monitor = event->data.motion_notify.monitor;

	if (monitor >= 0 && mwm_get_focused_monitor() != monitor) {
		mwm_focus_monitor(monitor);
	}

	return 0;
}

static int _event_property_notify_handler(struct event *event)
{
	if (event->data.property_notify.window == _mwm->root) {
		mwm_needs_redraw();
	} else if (event->data.property_notify.client >= 0) {
		client_property_notify(event->data.property_notify.client,
		                       &event->data.property_notify.xevent);
	}

	return 0;
}

static int _event_unmap_notify_handler(struct event *event)
{
	client_t client;

	if ((client = event->data.unmap_notify.client) < 0) {
		return 0;
	}

	XGrabServer(_mwm->display);
	XSync(_mwm->display, False);
	XSetErrorHandler(_xerror_nop);

	client_set_state(client, WithdrawnState);

	if (!event->data.unmap_notify.send_event) {
		mwm_detach_client(client);
		client_free(client);
	}

	XSync(_mwm->display, False);
	XSetErrorHandler(_xerror_handle);
	XUngrabServer(_mwm->display);

	return 0;
}

static const struct {
	event_type_t type;
	event_conv_t *converter;
} _event_converters[] = {
	[ConfigureRequest] = { EVENT_CONFIGURE_REQUEST, (event_conv_t*)_xevent_to_configure_request },
	[ConfigureNotify]  = { EVENT_CONFIGURE_NOTIFY,  (event_conv_t*)_xevent_to_configure_notify  },
	[DestroyNotify]    = { EVENT_DESTROY_NOTIFY,    (event_conv_t*)_xevent_to_destroy_notify    },
	[EnterNotify]      = { EVENT_ENTER_NOTIFY,      (event_conv_t*)_xevent_to_enter_notify      },
	[Expose]           = { EVENT_EXPOSE,            (event_conv_t*)_xevent_to_expose            },
	[FocusIn]          = { EVENT_FOCUS_IN,          (event_conv_t*)_xevent_to_focus_in          },
	[KeyPress]         = { EVENT_KEY_PRESS,         (event_conv_t*)_xevent_to_key_press         },
	[MappingNotify]    = { EVENT_MAPPING_NOTIFY,    (event_conv_t*)_xevent_to_mapping_notify    },
	[MapRequest]       = { EVENT_MAP_REQUEST,       (event_conv_t*)_xevent_to_map_request       },
	[MotionNotify]     = { EVENT_MOTION_NOTIFY,     (event_conv_t*)_xevent_to_motion_notify     },
	[PropertyNotify]   = { EVENT_PROPERTY_NOTIFY,   (event_conv_t*)_xevent_to_property_notify   },
	[UnmapNotify]      = { EVENT_UNMAP_NOTIFY,      (event_conv_t*)_xevent_to_unmap_notify      },
};

static event_handler_t * const _event_handlers[] = {
	[EVENT_CONFIGURE_REQUEST] = _event_configure_request_handler,
	[EVENT_CONFIGURE_NOTIFY]  = _event_configure_notify_handler,
	[EVENT_DESTROY_NOTIFY]    = _event_destroy_notify_handler,
	[EVENT_ENTER_NOTIFY]      = _event_enter_notify_handler,
	[EVENT_EXPOSE]            = _event_expose_handler,
	[EVENT_FOCUS_IN]          = _event_focus_in_handler,
	[EVENT_KEY_PRESS]         = _event_key_press_handler,
	[EVENT_MAPPING_NOTIFY]    = _event_mapping_notify_handler,
	[EVENT_MAP_REQUEST]       = _event_map_request_handler,
	[EVENT_MOTION_NOTIFY]     = _event_motion_notify_handler,
	[EVENT_PROPERTY_NOTIFY]   = _event_property_notify_handler,
	[EVENT_UNMAP_NOTIFY]      = _event_unmap_notify_handler,
};

int xevent_to_event(XEvent *xevent, struct event **dst)
{
	int err;
	event_type_t type;
	struct event *event;

	event = NULL;

	if (xevent->type < (sizeof(_event_converters) / sizeof(_event_converters[0]))) {
		if (_event_converters[xevent->type].converter) {
			type = _event_converters[xevent->type].type;

			err = event_new(&event, type);

			if (!err) {
				err = _event_converters[xevent->type].converter(xevent, event);
			}
		} else {
			err = -ENOSYS;
		}
	} else {
		err = xrandr_event_to_event(_mwm->xrandr, xevent, &event);
	}

	if (!err) {
		*dst = event;
	} else if (event) {
		event_free(&event);
	}

	return err;
}

int mwm_run(void)
{
	XEvent event;

	XSync(_mwm->display, False);
	_mwm->running = 1;

	while (_mwm->running) {
		client_t focused_client;
		struct event *mwm_event;

		/*
		 * Handle as many events as possible before redrawing. This is necessary
		 * to avoid problems where an application unmaps/destroys multiple windows
		 * in one go. If we process the events one-by-one, redrawing after each
		 * event, we will likely attempt to redraw a window that was already
		 * unmapped or destroyed, we just haven't noticed it yet because the event
		 * is still in the queue.
		 */

		do {
			int err;

			if (XNextEvent(_mwm->display, &event) != 0) {
				continue;
			}

			if ((err = xevent_to_event(&event, &mwm_event)) < 0) {
				if (err != -ENOSYS) {
					fprintf(stderr, "Could not convert event: %s\n", strerror(-err));
				}

				continue;
			}

			if ((err = event_nq(mwm_event)) < 0) {
				fprintf(stderr, "Could not enqueue event: %s\n", strerror(-err));
				event_free(&mwm_event);
				continue;
			}
		} while (XEventsQueued(_mwm->display, QueuedAfterFlush) > 0);

		while (event_dq(&mwm_event) >= 0) {
			fprintf(stderr, "Handling event %d\n", mwm_event->type);

			if (mwm_event->type < (sizeof(_event_handlers) / sizeof(_event_handlers[0]))) {
				_event_handlers[mwm_event->type](mwm_event);
			} else {
				xrandr_handle_mevent(_mwm->xrandr, mwm_event);
			}

			event_free(&mwm_event);
		}

		if (_mwm->needs_redraw) {
			mwm_redraw();
		}

		focused_client = mwm_get_focused_client();

		if (_mwm->focused_client != focused_client) {
			client_focus(focused_client);
			_mwm->focused_client = focused_client;
		}
	}

	return 0;
}

int mwm_stop(void)
{
	if (!_mwm->running) {
		return -EALREADY;
	}

	_mwm->running = 0;

	return 0;
}

int mwm_attach_monitor(const monitor_t monitor)
{
	workspace_t unviewed;

	unviewed = -1;

	if (monitor < 0) {
		return -EINVAL;
	}

	if (mwm_get_focused_monitor() < 0) {
		mwm_focus_monitor(monitor);
	}

	unviewed = workspace_unviewed();
	if (unviewed >= 0) {
		monitor_set_workspace(monitor, unviewed);
	}

	return 0;
}

int mwm_detach_monitor(const monitor_t monitor)
{
	workspace_t workspace;
	monitor_t next_monitor;

	if (monitor < 0) {
		return -EINVAL;
	}

	next_monitor = monitor + 1;

	if (mwm_get_focused_monitor() == monitor) {
#if MWM_DEBUG
		fprintf(stderr, "%s: Detaching focused monitor %ld. Shifting focus to %ld\n",
		        __func__, monitor, next_monitor);
#endif /* MWM_DEBUG */

		/* FIXME: Need to determine if the ids refer to the same monitor */
		if (next_monitor == monitor) {
#if MWM_DEBUG
			fprintf(stderr, "%s: There are no other monitors?\n", __func__);
#endif /* MWM_DEBUG */
			next_monitor = -1;
		}

		mwm_focus_monitor(next_monitor);
	}

	if (monitor_get_workspace(monitor, &workspace) == 0 && workspace) {
		workspace_set_viewer(workspace, -1);
	}

	return 0;
}

int mwm_focus_monitor(const monitor_t monitor)
{
	if (monitor < 0) {
		return -EINVAL;
	}

#if MWM_DEBUG
	fprintf(stderr, "New monitor will be: %ld\n", monitor);
#endif /* MWM_DEBUG */

	_mwm->focus.next = monitor;
	_mwm->focus.changed = 1;

	mwm_needs_redraw();

	return 0;
}

monitor_t mwm_get_focused_monitor(void)
{
	return _mwm->focus.current;
}

int mwm_attach_client(const client_t client)
{
	workspace_t workspace;
	monitor_t monitor;
	Window window;
	int err;

	if (client < 0) {
		return -EINVAL;
	}

	monitor = mwm_get_focused_monitor();
	workspace = -1;

	if (monitor >= 0) {
		monitor_get_workspace(monitor, &workspace);
	}

	if (workspace < 0) {
		/*
		 * There's a chance that we might be attaching clients
		 * before the first monitor has been detected. In this
		 * case, always attach to the first workspace.
		 */
		workspace = 0;
	}

#if MWM_DEBUG
	fprintf(stderr, "Attaching client %ld to workspace %ld\n",
		client, workspace);
#endif /* MWM_DEBUG */
	if ((err = client_get_window(client, &window)) < 0) {
		return err;
	}

	XSelectInput(_mwm->display, window,
	             EnterWindowMask | FocusChangeMask |
	             PropertyChangeMask | StructureNotifyMask);

	client_set_state(client, NormalState);

	return workspace_attach_client(workspace, client);
}

int mwm_detach_client(const client_t client)
{
	workspace_t workspace;
	int err;

	if (client < 0) {
		return -EINVAL;
	}

	if ((err = client_get_workspace(client, &workspace)) < 0) {
		return err;
	}

	return workspace_detach_client(workspace, client);
}

int mwm_focus_client(const client_t client)
{
	workspace_t workspace;

	if (client_get_workspace(client, &workspace) < 0) {
		workspace = mwm_get_focused_workspace();
	}

	if (workspace < 0) {
		return -EBADFD;
	}

	return workspace_focus_client(workspace, client);
}

client_t mwm_get_focused_client(void)
{
	client_t focused_client;
	monitor_t focused_monitor;
	int err;

	if ((focused_monitor = mwm_get_focused_monitor()) < 0) {
		return -1;
	}

	if ((err = monitor_get_focused_client(focused_monitor, &focused_client)) < 0) {
		return err;
	}

	return focused_client;
}

workspace_t mwm_get_focused_workspace(void)
{
	monitor_t monitor;
	workspace_t workspace;

	workspace = -ENOENT;
	monitor = mwm_get_focused_monitor();

	if (monitor >= 0) {
		monitor_get_workspace(monitor, &workspace);
	}

	return workspace;
}

int mwm_needs_redraw(void)
{
	_mwm->needs_redraw = 1;
	return(0);
}

int mwm_redraw(void)
{
	if (_mwm->focus.changed) {
		monitor_needs_redraw(_mwm->focus.current);
		monitor_needs_redraw(_mwm->focus.next);

		_mwm->focus.current = _mwm->focus.next;
		_mwm->focus.next = -1;
	}

	if(_mwm->needs_redraw) {
		monitor_foreach((int(*)(const monitor_t, void*))monitor_redraw, NULL);
		workspace_foreach((int(*)(const workspace_t, void*))workspace_redraw, NULL);
	}

	_mwm->needs_redraw = 0;
	_mwm->focus.changed = 0;

	return 0;
}

Window mwm_create_window(const int x, const int y, const int w, const int h)
{
	XSetWindowAttributes attrs;
	int depth;
	Visual *visual;
	unsigned long mask;

	attrs.override_redirect = True;
	attrs.background_pixmap = ParentRelative;
	attrs.event_mask = ExposureMask;

	mask = CWOverrideRedirect | CWBackPixmap | CWEventMask;
	depth = DefaultDepth(_mwm->display, _mwm->screen);
	visual = DefaultVisual(_mwm->display, _mwm->screen);

	return XCreateWindow(_mwm->display, _mwm->root, x, y, w, h, 0,
	                     depth, CopyFromParent, visual, mask, &attrs);
}

GC mwm_create_gc(void)
{
	GC context;

	context = XCreateGC(_mwm->display, _mwm->root, 0, NULL);

	XSetLineAttributes(_mwm->display, context, 1, LineSolid, CapButt, JoinMiter);

	return context;
}

XftDraw* mwm_create_xft_context(Drawable drawable)
{
	return XftDrawCreate(_mwm->display, drawable,
	                     DefaultVisual(_mwm->display, _mwm->screen),
	                     DefaultColormap(_mwm->display, _mwm->screen));
}

Drawable mwm_create_pixmap(Window window, const int width, const int height)
{
	return(XCreatePixmap(_mwm->display, window ? window : _mwm->root, width, height,
			     DefaultDepth(_mwm->display, _mwm->screen)));
}

void mwm_free_pixmap(Drawable drawable)
{
	XFreePixmap(_mwm->display, drawable);
	return;
}

int mwm_get_font_height(const mwm_font_t font)
{
	return _mwm->font[font].height;
}

int mwm_get_text_width(const char *text, const mwm_font_t font)
{
	PangoRectangle extents;

	pango_layout_set_attributes(_mwm->font[font].layout, NULL);
	pango_layout_set_width(_mwm->font[font].layout, -1);
	pango_layout_set_height(_mwm->font[font].layout, -1);
	pango_layout_set_markup(_mwm->font[font].layout, text, -1);
	pango_layout_get_extents(_mwm->font[font].layout, 0, &extents);

	return extents.width / PANGO_SCALE;
}

unsigned long mwm_get_color(mwm_palette_t palette, mwm_color_t color)
{
	return _mwm->palette[palette].color[color];
}

int mwm_get_text_property(Window window, Atom atom, char **dst)
{
	XTextProperty property;
	int len;
	Atom UTF8_STRING;

	if (!dst) {
		return -EINVAL;
	}

	if (mwm_get_atom(MWM_ATOM_UTF8, &UTF8_STRING) < 0) {
		return -EIO;
	}

	XGetTextProperty(_mwm->display, window, &property, atom);

	if(property.nitems == 0) {
		return(-ENOENT);
	}

	if (property.encoding == XA_STRING ||
	    property.encoding == UTF8_STRING) {
		char *dup;

		if (!(dup = strdup((char*)property.value))) {
			len = -ENOMEM;
		} else {
			*dst = dup;
			len = strlen(dup);
		}
	} else {
		len = -ENOSYS;
	}

	XFree(property.value);
	return len;
}

int mwm_get_pointer(struct geom *pointer)
{
	Window root;
	Window dontcare_w;
	int x, y;
	int dontcare_i;
	unsigned int dontcare_ui;

	if (!pointer) {
		return -EINVAL;
	}

	if (XQueryPointer(_mwm->display, _mwm->root,
	                  &root, &dontcare_w,
	                  &x, &y,
	                  &dontcare_i, &dontcare_i,
	                  &dontcare_ui) == False) {
		/* pointer is not on this screen */
		return -ENOMEDIUM;
	}

	pointer->x = x;
	pointer->y = y;

	return 0;
}

int mwm_get_status(char **buffer)
{
	char *status;
	int len;

	status = NULL;

	if ((len = mwm_get_text_property(_mwm->root, XA_WM_NAME, &status)) < 0) {
		if (!(status = strdup("mwm-0.1"))) {
			return -ENOMEM;
		}
	}

	*buffer = status;
	return 0;
}

int mwm_grab_keys(void)
{
	extern struct key_binding config_keybindings[];
	struct key_binding *binding;

	XUngrabKey(_mwm->display, AnyKey, AnyModifier, _mwm->root);

	for (binding = config_keybindings; binding->cmd < MWM_CMD_MAX; binding++) {
		KeyCode code;

		code = XKeysymToKeycode(_mwm->display, binding->key);

		XGrabKey(_mwm->display, code, binding->mod,
			 _mwm->root, True, GrabModeAsync, GrabModeAsync);
		XGrabKey(_mwm->display, code, binding->mod | LockMask,
			 _mwm->root, True, GrabModeAsync, GrabModeAsync);
	}

	return 0;
}

int mwm_cmd(mwm_cmd_t cmd, void *data)
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

	nameidx = sizeof(cmd_names) / sizeof(cmd_names[0]) - 1;
	if(cmd >= 0 && cmd < nameidx) {
		nameidx = cmd;
	}

	fprintf(stderr, "%s(%d [%s], %p)\n", __func__,
		cmd, cmd_names[nameidx], data);
#endif /* MWM_DEBUG */

	if (cmd < 0 || cmd >= MWM_CMD_MAX) {
		return(-EINVAL);
	}

	if (!_mwm->commands[cmd]) {
		return(-ENOSYS);
	}

	_mwm->commands[cmd](data);
	return 0;
}

int mwm_get_atom_by_name(const char *name, Atom *dst)
{
	Atom atom;

	if (!name || !dst) {
		return -EINVAL;
	}

	/* FIXME: Cache the result */
	atom = XInternAtom(_mwm->display, name, False);

	*dst = atom;
	return 0;
}

int mwm_get_atom(mwm_atom_t atom_id, Atom *dst)
{
	if (!dst) {
		return -EINVAL;
	}

	if (atom_id < 0 || atom_id >= MWM_ATOM_MAX) {
		return -EBADSLT;
	}

	*dst = _mwm->atoms[atom_id];
	return 0;
}
