#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <X11/Xlib.h>
#include "x.h"
#include "mwm.h"
#include "monitor.h"
#include "workspace.h"
#include "common.h"
#include "loop.h"
#include "client.h"
#include "layout.h"
#include "xrandr.h"

#define STATUSBAR_HEIGHT 32
#define INDICATOR_HEIGHT 64
#define INDICATOR_PADDING 16

#define HINDICATOR 0
#define VINDICATOR 1

struct indicator {
	Window window;
	struct geom geom;
	GC gfx_context;
	XftDraw *xft_context;
	int orientation;
};

struct monitor {
	xrandr_crtc_t crtc;
	Window statusbar;
	Drawable draw_buffer;

	GC gfx_context;
	XftDraw *xft_context;

	struct indicator indicator[2];

	int needs_redraw;

	struct {
		struct geom current;
		struct geom next;
		int changed;
	} geom;

	struct {
		struct workspace *current;
		struct workspace *next;
		int changed;
	} workspace;

	struct layout *layout;
};

extern struct layout *layouts[];

static const char *_workspace_names[] = {
	"１", "２", "３", "４", "５", "６", "７", "８", "９", "０", "−", "＾"
};

static void _indicator_update_window(struct indicator *indicator, struct monitor *monitor)
{
	if (indicator->window) {
		XMoveResizeWindow(mwm_get_display(), indicator->window,
				  indicator->geom.x, indicator->geom.y,
				  indicator->geom.w, indicator->geom.h);
	} else {
		indicator->window = mwm_create_window(indicator->geom.x, indicator->geom.y,
						      indicator->geom.w, indicator->geom.h);
		indicator->gfx_context = mwm_create_gc();
		indicator->xft_context = mwm_create_xft_context((Drawable)indicator->window);
		XMapRaised(mwm_get_display(), indicator->window);
	}

	return;
}

static void _indicator_update_geometry(struct monitor *monitor)
{
	monitor->indicator[HINDICATOR].orientation = HINDICATOR;
	monitor->indicator[HINDICATOR].geom.x = monitor->geom.current.x;
	monitor->indicator[HINDICATOR].geom.y = monitor->geom.current.y + STATUSBAR_HEIGHT;
	monitor->indicator[HINDICATOR].geom.w = monitor->geom.current.w;
	monitor->indicator[HINDICATOR].geom.h = INDICATOR_HEIGHT;
	_indicator_update_window(&monitor->indicator[HINDICATOR], monitor);

	monitor->indicator[VINDICATOR].orientation = VINDICATOR;
	monitor->indicator[VINDICATOR].geom.x = monitor->geom.current.x + monitor->geom.current.w -
		INDICATOR_HEIGHT;
	monitor->indicator[VINDICATOR].geom.y = monitor->geom.current.y + STATUSBAR_HEIGHT;
	monitor->indicator[VINDICATOR].geom.w = INDICATOR_HEIGHT;
	monitor->indicator[VINDICATOR].geom.h = monitor->geom.current.h - STATUSBAR_HEIGHT;
	_indicator_update_window(&monitor->indicator[VINDICATOR], monitor);

	return;
}

void _indicator_set_visible(struct indicator *indicator, int visible, struct monitor *monitor)
{
	Display *display;

	display = mwm_get_display();

	if(visible) {
		XMoveWindow(display, indicator->window, indicator->geom.x, indicator->geom.y);
		/* XMapRaised(display, indicator->window); */
	} else {
		XMoveWindow(display, indicator->window, indicator->geom.w * -2, indicator->geom.y);
		/* XUnmapWindow(display, indicator->window); */
	}

	return;
}

void _redraw_indicator(struct indicator *indicator, struct monitor *monitor)
{
	struct workspace *workspace;
	client_t focused;
	Display *display;
	Window root;
	mwm_palette_t palette;
	int font_height;
	int font_padding;

	display = mwm_get_display();
	root = mwm_get_root_window();
	font_height = mwm_get_font_height();
	font_padding = (INDICATOR_HEIGHT - 2 * INDICATOR_PADDING - font_height) / 2;

	XCopyArea(display, root, indicator->window, indicator->gfx_context,
		  indicator->geom.x, indicator->geom.y,
		  indicator->geom.w, indicator->geom.h,
		  0, 0);

	workspace = monitor_get_workspace(monitor);
	focused = workspace_get_focused_client(workspace);
	palette = monitor_is_focused(monitor) ? MWM_PALETTE_ACTIVE : MWM_PALETTE_INACTIVE;

	if(focused) {
		struct geom focus_pos;
		struct geom client_pos;
		unsigned long fg_color;
		unsigned long bg_color;
		const char *hint;

		hint = NULL;

		client_get_hint(focused, &hint);
		if (!hint) {
			hint = "";
		}
		client_get_geometry(focused, &focus_pos);
		memcpy(&client_pos, &focus_pos, sizeof(client_pos));

		fg_color = mwm_get_color(palette, MWM_COLOR_INDICATOR_FILL);
		bg_color = mwm_get_color(palette, MWM_COLOR_INDICATOR_BORDER);

		if(indicator->orientation == HINDICATOR) {
			focus_pos.x -= indicator->geom.x;
			focus_pos.y = INDICATOR_PADDING;
			focus_pos.h = INDICATOR_HEIGHT - 2 * INDICATOR_PADDING;
		} else {
			focus_pos.x = INDICATOR_PADDING;
			focus_pos.y -= indicator->geom.y;
			focus_pos.w = INDICATOR_HEIGHT - 2 * INDICATOR_PADDING;
		}

		XSetForeground(display, indicator->gfx_context, fg_color);
		XFillRectangle(display, indicator->window, indicator->gfx_context,
			       focus_pos.x, focus_pos.y, focus_pos.w, focus_pos.h);
		XSetForeground(display, indicator->gfx_context, bg_color);
		XDrawRectangle(display, indicator->window, indicator->gfx_context,
			       focus_pos.x, focus_pos.y, focus_pos.w, focus_pos.h);

		if(indicator->orientation == HINDICATOR) {
			mwm_render_text(indicator->xft_context,
			                palette, hint,
					focus_pos.x + font_padding,
			                focus_pos.y + font_padding,
			                focus_pos.w - (2 * font_padding),
			                focus_pos.h - (2 * font_padding));
		} else {
			mwm_render_text_vertical(indicator->xft_context,
			                         palette, hint,
						 focus_pos.x + font_padding,
			                         focus_pos.y + font_padding,
			                         focus_pos.w - (2 * font_padding),
			                         focus_pos.h - (2 * font_padding));
		}
	}

	return;
}

int monitor_redraw_indicators(struct monitor *monitor)
{
	layout_orientation_t orientation;

	if(!monitor) {
		return(-EINVAL);
	}

	orientation = layout_get_orientation(monitor->layout);

	/* NOTE: LAYOUT_HORIZONTAL and LAYOUT_VERTICAL are *not* mutually exclusive */

	if(orientation & LAYOUT_HORIZONTAL) {
		_redraw_indicator(&monitor->indicator[HINDICATOR], monitor);
		_indicator_set_visible(&monitor->indicator[HINDICATOR], 1, monitor);
	} else {
		_indicator_set_visible(&monitor->indicator[HINDICATOR], 0, monitor);
	}

	if(orientation & LAYOUT_VERTICAL) {
		_redraw_indicator(&monitor->indicator[VINDICATOR], monitor);
		_indicator_set_visible(&monitor->indicator[VINDICATOR], 1, monitor);
	} else {
		_indicator_set_visible(&monitor->indicator[VINDICATOR], 0, monitor);
	}

	return(0);
}

int monitor_new(xrandr_crtc_t crtc, int x, int y, int w, int h,
		struct monitor **monitor)
{
	struct monitor *mon;

	if (!monitor) {
		return -EINVAL;
	}

	if (!(mon = calloc(1, sizeof(*mon)))) {
		return -ENOMEM;
	}

	mon->crtc = crtc;
	mon->geom.current.x = x;
	mon->geom.current.y = y;
	mon->geom.current.w = w;
	mon->geom.current.h = h;
	mon->layout = layouts[0];

	mon->statusbar = mwm_create_window(x, y, w, STATUSBAR_HEIGHT);
	mon->gfx_context = mwm_create_gc();
	mon->draw_buffer = mwm_create_pixmap(0, w, STATUSBAR_HEIGHT);
	mon->xft_context = mwm_create_xft_context(mon->draw_buffer);
	XMapRaised(mwm_get_display(), mon->statusbar);

	_indicator_update_geometry(mon);

	*monitor = mon;

	return 0;
}

int monitor_free(struct monitor **monitor)
{
	Display *display;

	if(!monitor) {
		return(-EINVAL);
	}

	if(!*monitor) {
		return(-EALREADY);
	}

	display = mwm_get_display();

	XUnmapWindow(display, (*monitor)->statusbar);
	XDestroyWindow(display, (*monitor)->statusbar);
	XFreeGC(display, (*monitor)->gfx_context);

	free(*monitor);
	*monitor = NULL;

	return(0);
}

Display* monitor_get_display(struct monitor *monitor)
{
	return mwm_get_display();
}

xrandr_crtc_t monitor_get_crtc(struct monitor *monitor)
{
	if (!monitor) {
		return -EINVAL;
	}

	return monitor->crtc;
}

int monitor_get_geometry(struct monitor *monitor, struct geom *geom)
{
	if (!monitor || !geom) {
		return -EINVAL;
	}

	memcpy(geom, &monitor->geom.current, sizeof(*geom));
	return 0;
}

int monitor_set_geometry(struct monitor *monitor, struct geom *geom)
{
	if (!monitor || !geom) {
		return -EINVAL;
	}

	if (memcmp(&monitor->geom.current, geom, sizeof(monitor->geom.current)) == 0) {
		return -EALREADY;
	}

#if MWM_DEBUG
	fprintf(stderr, "Setting geometry of monitor 0x%lx to %dx%d @ %dx%d\n",
	        monitor->crtc, geom->w, geom->h, geom->x, geom->y);
#endif /* MWM_DEBUG */

	memcpy(&monitor->geom.next, geom, sizeof(*geom));
	monitor->geom.changed = 1;

	return 0;
}

int monitor_swap_workspace(struct monitor *first, struct monitor *second)
{
	first->workspace.next = second->workspace.current;
	first->workspace.changed = 1;

	second->workspace.next = first->workspace.current;
	second->workspace.changed = 1;

	workspace_set_viewer(first->workspace.current, first);
	workspace_set_viewer(second->workspace.current, second);

	workspace_needs_redraw(first->workspace.current);
	workspace_needs_redraw(second->workspace.current);

	return 0;
}

int monitor_set_workspace(struct monitor *monitor, struct workspace *workspace)
{
	struct monitor *other;

	if (!monitor || !workspace) {
		return -EINVAL;
	}

#if MWM_DEBUG
	fprintf(stderr, "%s(%p, %p)\n", __func__, (void*)monitor, (void*)workspace);
#endif /* MWM_DEBUG */

	other = workspace_get_viewer(workspace);

	if (other) {
		return monitor_swap_workspace(monitor, other);
	} else {
		struct workspace *old;

		old = monitor_get_workspace(monitor);
		workspace_set_viewer(old, NULL);
		workspace_needs_redraw(old);
	}

	workspace_set_viewer(workspace, monitor);

	monitor->workspace.next = workspace;
	monitor->workspace.changed = 1;
	monitor_needs_redraw(monitor);

	return 0;
}

struct workspace* monitor_get_workspace(struct monitor *monitor)
{
	return monitor->workspace.current;
}

client_t monitor_get_focused_client(struct monitor *monitor)
{
	struct workspace *workspace;

	workspace = monitor_get_workspace(monitor);

	if (!workspace) {
		return -ENOMEDIUM;
	}

	return workspace_get_focused_client(workspace);
}

int monitor_arrange_clients(struct monitor *monitor)
{
	struct geom geom;

	if(!monitor) {
		return(-EINVAL);
	}

	if(monitor_get_usable_area(monitor, &geom) < 0) {
		return(-EFAULT);
	}

	layout_arrange(monitor->layout,
		       monitor->workspace.current,
		       &geom);

	return(0);
}

int monitor_get_usable_area(struct monitor *monitor, struct geom *usable_area)
{
	layout_orientation_t orientation;

	if (!monitor || !usable_area) {
		return -EINVAL;
	}

	usable_area->x = monitor->geom.current.x;
	usable_area->y = monitor->geom.current.y + STATUSBAR_HEIGHT;
	usable_area->w = monitor->geom.current.w;
	usable_area->h = monitor->geom.current.h - STATUSBAR_HEIGHT;

	orientation = layout_get_orientation(monitor->layout);

	if(orientation & LAYOUT_VERTICAL) {
		usable_area->w -= INDICATOR_HEIGHT;
	}

	if(orientation & LAYOUT_HORIZONTAL) {
		usable_area->y += INDICATOR_HEIGHT;
		usable_area->h -= INDICATOR_HEIGHT;
	}

	return 0;
}

int _draw_client(struct workspace *workspace, const client_t client, void *data)
{
	client_redraw(client);
	return(0);
}

int monitor_draw_clients(struct monitor *monitor)
{
	if(!monitor) {
		return(-EINVAL);
	}

	workspace_foreach_client(monitor->workspace.current, _draw_client, monitor);

	return(0);
}

int monitor_needs_redraw(struct monitor *monitor)
{
	if (!monitor) {
		return -EINVAL;
	}

	monitor->needs_redraw = 1;
	mwm_needs_redraw();

	return 0;
}

struct _draw_workspace_data {
	struct monitor *monitor;
	mwm_palette_t palette;
	Display *display;
	int text_padding;
	int text_width;
	int i;
	struct workspace *focused_workspace;
};

static int _draw_workspace_button(struct workspace *workspace, void *data)
{
	struct _draw_workspace_data *dwdata;
	mwm_color_t color;
	int button_width;
	int focused;
	int visible;
	int x;

	dwdata = (struct _draw_workspace_data*)data;

	focused = workspace == dwdata->focused_workspace;
	visible = workspace_get_viewer(workspace) != NULL;

	button_width = dwdata->text_width + 2 * dwdata->text_padding;
	x = dwdata->i * button_width;

	if(focused) {
		color = MWM_COLOR_FOCUSED;
	} else if(visible) {
		color = MWM_COLOR_VISIBLE;
	} else {
		color = MWM_COLOR_BACKGROUND;
	}

	XSetForeground(dwdata->display, dwdata->monitor->gfx_context,
		       mwm_get_color(dwdata->palette, color));

	XFillRectangle(dwdata->display, dwdata->monitor->draw_buffer,
		       dwdata->monitor->gfx_context, x, 0,
		       button_width, STATUSBAR_HEIGHT);

	mwm_render_text(dwdata->monitor->xft_context, dwdata->palette,
	                _workspace_names[dwdata->i], x + dwdata->text_padding, dwdata->text_padding,
	                button_width, button_width);

	/* A workspace necessarily has a focused client if it isn't empty */
	if(workspace_get_focused_client(workspace)) {
		XSetForeground(dwdata->display, dwdata->monitor->gfx_context,
			       mwm_get_color(dwdata->palette, MWM_COLOR_CLIENT_INDICATOR));
		XFillRectangle(dwdata->display, dwdata->monitor->draw_buffer,
			       dwdata->monitor->gfx_context, x + 2, 2, button_width - 4, 2);
	}

	dwdata->i++;

	return(0);
}

static int _redraw_statusbar(struct monitor *monitor)
{
	struct _draw_workspace_data dwdata;
	struct monitor *focused_monitor;
	Display *display;
	int status_x;
	int status_width;
	int status_width_max;
	int workspace_button_width;
	char *status;

	if(!monitor) {
		return(-EINVAL);
	}

	status = NULL;
	display = mwm_get_display();
	focused_monitor = mwm_get_focused_monitor();

	/* draw the workspace buttons */
	dwdata.monitor = monitor;
	dwdata.display = display;
	dwdata.palette = focused_monitor == monitor ? MWM_PALETTE_ACTIVE : MWM_PALETTE_INACTIVE;
	dwdata.text_padding = (STATUSBAR_HEIGHT - mwm_get_font_height()) / 2;
	dwdata.text_width = mwm_get_text_width(_workspace_names[0]);
	dwdata.i = 0;
	dwdata.focused_workspace = monitor_get_workspace(monitor);

	mwm_foreach_workspace(_draw_workspace_button, &dwdata);

	workspace_button_width = dwdata.i * (dwdata.text_width + 2 * dwdata.text_padding);

	mwm_get_status(&status);

	/* right-align the status */
	status_width = mwm_get_text_width(status ? status :  "") +
		dwdata.text_padding * 2;
	status_x = monitor->geom.current.w - status_width;
	status_width_max = monitor->geom.current.w - workspace_button_width;

	/*
	 * If there isn't enough space, left-align. I'd prefer part of the status to be cut
	 * off rather than drawing over the workspace buttons.
	 */
	if(status_x < workspace_button_width) {
		status_x = workspace_button_width;
		status_width = monitor->geom.current.w - status_x;
	} else if(status_x > workspace_button_width) {
		XSetForeground(display, monitor->gfx_context,
			       mwm_get_color(dwdata.palette, MWM_COLOR_FOCUSED));
		XFillRectangle(display, monitor->draw_buffer,
			       monitor->gfx_context, workspace_button_width, 0,
			       status_x - workspace_button_width, STATUSBAR_HEIGHT);
	}

	XSetForeground(display, monitor->gfx_context,
		       mwm_get_color(dwdata.palette, MWM_COLOR_BACKGROUND));
	XFillRectangle(display, monitor->draw_buffer,
		       monitor->gfx_context, status_x, 0,
		       status_width, STATUSBAR_HEIGHT);

	mwm_render_text(monitor->xft_context, dwdata.palette, status ? status : "",
	                status_x + dwdata.text_padding, dwdata.text_padding,
	                status_width_max, STATUSBAR_HEIGHT);

	XCopyArea(display, monitor->draw_buffer, monitor->statusbar, monitor->gfx_context,
		  0, 0, monitor->geom.current.w, STATUSBAR_HEIGHT, 0, 0);
	free(status);

	return(0);
}

int monitor_redraw(struct monitor *monitor)
{
	if (monitor->workspace.changed) {
		monitor->workspace.current = monitor->workspace.next;
	}

	if (monitor->geom.changed) {
		memcpy(&monitor->geom.current, &monitor->geom.next, sizeof(monitor->geom.current));
		memset(&monitor->geom.next, 0, sizeof(monitor->geom.next));

		XMoveResizeWindow(mwm_get_display(), monitor->statusbar,
		                  monitor->geom.current.x, monitor->geom.current.y,
		                  monitor->geom.current.w, STATUSBAR_HEIGHT);
		mwm_free_pixmap(monitor->draw_buffer);
		monitor->draw_buffer = mwm_create_pixmap(0, monitor->geom.current.w, STATUSBAR_HEIGHT);
		XftDrawChange(monitor->xft_context, monitor->draw_buffer);

		_indicator_update_geometry(monitor);
	}

	if(monitor->needs_redraw) {
		monitor_arrange_clients(monitor);
		monitor_draw_clients(monitor);
	}

	_redraw_statusbar(monitor);
	monitor_redraw_indicators(monitor);

	monitor->workspace.changed = 0;
	monitor->geom.changed = 0;
	monitor->needs_redraw = 0;

	return 0;
}

int monitor_set_layout(struct monitor *monitor,
		       struct layout *layout)
{
	if(!monitor) {
		return(-EINVAL);
	}

	if(layout != monitor->layout) {
		monitor->layout = layout;
		monitor_needs_redraw(monitor);
	}

	return(0);
}

struct layout* monitor_get_layout(struct monitor *monitor)
{
	return(monitor->layout);
}

int monitor_is_focused(struct monitor *monitor)
{
	return mwm_get_focused_monitor() == monitor;
}

#if MWM_DEBUG
void monitor_dump(struct monitor *monitor)
{
	fprintf(stderr,
	        "  Monitor %p\n"
	        "    Identifier:       0x%lx\n"
	        "    Current geometry: %dx%d @ %dx%d\n"
	        "    Next geometry:    %dx%d @ %dx%d\n"
	        "    Geometry changed: %d\n",
	        (void*)monitor,
	        monitor->crtc,
	        monitor->geom.current.w, monitor->geom.current.h, monitor->geom.current.x, monitor->geom.current.y,
	        monitor->geom.next.w, monitor->geom.next.h, monitor->geom.next.x, monitor->geom.next.y,
	        monitor->geom.changed);
}
#endif /* MWM_DEBUG */
