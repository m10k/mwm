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

#define STATUSBAR_HEIGHT 32
#define INDICATOR_HEIGHT 32
#define INDICATOR_PADDING 8

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
	int id;
	Window statusbar;
	GC gfx_context;
	XftDraw *xft_context;

	struct indicator indicator[2];

	int needs_redraw;
	struct geom geom;
	struct workspace *workspace;
	struct layout *layout;
	struct mwm *mwm;
};

extern struct layout *layouts[];

static const char *_workspace_names[] = {
	"い", "ろ", "は", "に", "ほ", "へ", "と", "ち", "り", "ぬ", "る", "を"
};

static void _indicator_update_window(struct indicator *indicator, struct monitor *monitor)
{
	if(indicator->window) {
		XMoveResizeWindow(mwm_get_display(monitor->mwm), indicator->window,
				  indicator->geom.x, indicator->geom.y,
				  indicator->geom.w, indicator->geom.h);
	} else {
		indicator->window = mwm_create_window(monitor->mwm,
						      indicator->geom.x, indicator->geom.y,
						      indicator->geom.w, indicator->geom.h);
		indicator->gfx_context = mwm_create_gc(monitor->mwm);
		indicator->xft_context = mwm_create_xft_context(monitor->mwm, (Drawable)indicator->window);
		XMapRaised(mwm_get_display(monitor->mwm), indicator->window);
	}

	return;
}

static void _indicator_update_geometry(struct monitor *monitor)
{
	monitor->indicator[HINDICATOR].orientation = HINDICATOR;
	monitor->indicator[HINDICATOR].geom.x = monitor->geom.x;
	monitor->indicator[HINDICATOR].geom.y = monitor->geom.y + STATUSBAR_HEIGHT;
	monitor->indicator[HINDICATOR].geom.w = monitor->geom.w;
	monitor->indicator[HINDICATOR].geom.h = INDICATOR_HEIGHT;
	_indicator_update_window(&monitor->indicator[HINDICATOR], monitor);

	monitor->indicator[VINDICATOR].orientation = VINDICATOR;
	monitor->indicator[VINDICATOR].geom.x = monitor->geom.x + monitor->geom.w - INDICATOR_HEIGHT;
	monitor->indicator[VINDICATOR].geom.y = monitor->geom.y + STATUSBAR_HEIGHT;
	monitor->indicator[VINDICATOR].geom.w = INDICATOR_HEIGHT;
	monitor->indicator[VINDICATOR].geom.h = monitor->geom.h - STATUSBAR_HEIGHT;
	_indicator_update_window(&monitor->indicator[VINDICATOR], monitor);

	return;
}

void _indicator_set_visible(struct indicator *indicator, int visible, struct monitor *monitor)
{
	Display *display;

	display = mwm_get_display(monitor->mwm);

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
	struct client *focused;
	Display *display;
	Window root;
	mwm_palette_t palette;
	int font_height;
	int font_padding;

	display = mwm_get_display(monitor->mwm);
	root = mwm_get_root_window(monitor->mwm);
	font_height = mwm_get_font_height(monitor->mwm);
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
		char status[512];

		client_get_geometry(focused, &focus_pos);
		memcpy(&client_pos, &focus_pos, sizeof(client_pos));

		fg_color = mwm_get_color(monitor->mwm, palette, MWM_COLOR_INDICATOR_FILL);
		bg_color = mwm_get_color(monitor->mwm, palette, MWM_COLOR_INDICATOR_BORDER);

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

		snprintf(status, sizeof(status), "W=0x%lx %dx%d+%d+%d",
			 client_get_window(focused),
			 client_pos.w, client_pos.h,
			 client_pos.x, client_pos.y);

		if(indicator->orientation == HINDICATOR) {
			mwm_render_text(monitor->mwm, indicator->xft_context,
					palette, status,
					focus_pos.x + font_padding,
					focus_pos.y + font_padding);
		} else {
			mwm_render_text_vertical(monitor->mwm, indicator->xft_context,
						 palette, status,
						 focus_pos.x + font_padding,
						 focus_pos.y + font_padding);
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

int monitor_new(struct mwm *mwm, int id, int x, int y, int w, int h,
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

	memset(mon, 0, sizeof(*mon));

	mon->mwm = mwm;
	mon->id = id;
	mon->geom.x = x;
	mon->geom.y = y;
	mon->geom.w = w;
	mon->geom.h = h;
	mon->layout = layouts[0];

	mon->statusbar = mwm_create_window(mwm, x, y, w, STATUSBAR_HEIGHT);
	mon->gfx_context = mwm_create_gc(mwm);
	mon->xft_context = mwm_create_xft_context(mwm, (Drawable)mon->statusbar);
	XMapRaised(mwm_get_display(mwm), mon->statusbar);

	_indicator_update_geometry(mon);

	*monitor = mon;

	return(0);
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

	display = mwm_get_display((*monitor)->mwm);

	XUnmapWindow(display, (*monitor)->statusbar);
	XDestroyWindow(display, (*monitor)->statusbar);
	XFreeGC(display, (*monitor)->gfx_context);

	free(*monitor);
	*monitor = NULL;

	return(0);
}

Display* monitor_get_display(struct monitor *monitor)
{
	return(mwm_get_display(monitor->mwm));
}

int monitor_get_id(struct monitor *monitor)
{
	if(!monitor) {
		return(-EINVAL);
	}

	return(monitor->id);
}

int monitor_get_geometry(struct monitor *monitor, struct geom *geom)
{
	if(!monitor || !geom) {
		return(-EINVAL);
	}

	memcpy(geom, &monitor->geom, sizeof(*geom));
	return(0);
}

int monitor_set_geometry(struct monitor *monitor, struct geom *geom)
{
	if(!monitor || !geom) {
		return(-EINVAL);
	}

	memcpy(&monitor->geom, geom, sizeof(*geom));

	XMoveResizeWindow(mwm_get_display(monitor->mwm), monitor->statusbar,
			  monitor->geom.x, monitor->geom.y,
			  monitor->geom.w, STATUSBAR_HEIGHT);

	return(0);
}

int monitor_swap_workspace(struct monitor *first, struct monitor *second)
{
	struct workspace *swap;

	swap = first->workspace;
	first->workspace = second->workspace;
	second->workspace = swap;

	workspace_set_viewer(first->workspace, first);
	workspace_set_viewer(second->workspace, second);

	workspace_needs_redraw(first->workspace);
	workspace_needs_redraw(second->workspace);

	return(0);
}

int monitor_set_workspace(struct monitor *monitor, struct workspace *workspace)
{
	struct monitor *other;

	if(!monitor || !workspace) {
		return(-EINVAL);
	}

	printf("%s(%p, %p)\n", __func__, (void*)monitor, (void*)workspace);
	other = workspace_get_viewer(workspace);

	if(other) {
		return(monitor_swap_workspace(monitor, other));
	} else {
		struct workspace *old;

		old = monitor_get_workspace(monitor);
		workspace_set_viewer(old, NULL);
		workspace_needs_redraw(old);
	}

	workspace_set_viewer(workspace, monitor);
	monitor->workspace = workspace;
	monitor_needs_redraw(monitor);

	return(0);
}

struct workspace* monitor_get_workspace(struct monitor *monitor)
{
	return(monitor->workspace);
}

struct client* monitor_get_focused_client(struct monitor *monitor)
{
	struct workspace *workspace;

	workspace = monitor_get_workspace(monitor);

	if(!workspace) {
		return(NULL);
	}

	return(workspace_get_focused_client(workspace));
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
		       monitor->workspace,
		       &geom);

	return(0);
}

int monitor_get_usable_area(struct monitor *monitor, struct geom *usable_area)
{
	layout_orientation_t orientation;

	usable_area->x = monitor->geom.x;
	usable_area->y = monitor->geom.y + STATUSBAR_HEIGHT;
	usable_area->w = monitor->geom.w;
	usable_area->h = monitor->geom.h - STATUSBAR_HEIGHT;

	orientation = layout_get_orientation(monitor->layout);

	if(orientation & LAYOUT_VERTICAL) {
		usable_area->w -= INDICATOR_HEIGHT;
	}

	if(orientation & LAYOUT_HORIZONTAL) {
		usable_area->y += INDICATOR_HEIGHT;
		usable_area->h -= INDICATOR_HEIGHT;
	}

	return(0);
}

int _draw_client(struct workspace *workspace, struct client *client, void *data)
{
	client_show(client);
	return(0);
}

int monitor_draw_clients(struct monitor *monitor)
{
	if(!monitor) {
		return(-EINVAL);
	}

	workspace_foreach_client(monitor->workspace, _draw_client, monitor);

	return(0);
}

int monitor_needs_redraw(struct monitor *monitor)
{
	if(!monitor) {
		return(-EINVAL);
	}

	monitor->needs_redraw = 1;
	mwm_needs_redraw(monitor->mwm);

	return(0);
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

static int _draw_workspace_button(struct mwm *mwm, struct workspace *workspace, void *data)
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
		       mwm_get_color(mwm, dwdata->palette, color));

	XFillRectangle(dwdata->display, dwdata->monitor->statusbar,
		       dwdata->monitor->gfx_context, x, 0,
		       button_width, STATUSBAR_HEIGHT);

	mwm_render_text(mwm, dwdata->monitor->xft_context, dwdata->palette,
			_workspace_names[dwdata->i], x + dwdata->text_padding, dwdata->text_padding);

	/* A workspace necessarily has a focused client if it isn't empty */
	if(workspace_get_focused_client(workspace)) {
		XSetForeground(dwdata->display, dwdata->monitor->gfx_context,
			       mwm_get_color(mwm, dwdata->palette, MWM_COLOR_CLIENT_INDICATOR));
		XFillRectangle(dwdata->display, dwdata->monitor->statusbar,
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
	Window root;
	int status_x;
	int status_width;
	int workspace_button_width;
	char status[512];

	if(!monitor) {
		return(-EINVAL);
	}

	display = mwm_get_display(monitor->mwm);
	root = mwm_get_root_window(monitor->mwm);
	focused_monitor = mwm_get_focused_monitor(monitor->mwm);
	status[0] = 0;

	/* start with background from root */
	XCopyArea(display, root, monitor->statusbar,
		  monitor->gfx_context,
		  monitor->geom.x, monitor->geom.y,
		  monitor->geom.w, STATUSBAR_HEIGHT,
		  0, 0);

	/* draw the workspace buttons */
	dwdata.monitor = monitor;
	dwdata.display = display;
	dwdata.palette = focused_monitor == monitor ? MWM_PALETTE_ACTIVE : MWM_PALETTE_INACTIVE;
	dwdata.text_padding = (STATUSBAR_HEIGHT - mwm_get_font_height(monitor->mwm)) / 2;
	dwdata.text_width = mwm_get_text_width(monitor->mwm, _workspace_names[0]);
	dwdata.i = 0;
	dwdata.focused_workspace = monitor_get_workspace(monitor);

	mwm_foreach_workspace(monitor->mwm, _draw_workspace_button, &dwdata);

	workspace_button_width = dwdata.i * (dwdata.text_width + 2 * dwdata.text_padding);

	mwm_get_status(monitor->mwm, status, sizeof(status));

	/* right-align the status */
	status_width = mwm_get_text_width(monitor->mwm, status) +
		dwdata.text_padding * 2;
	status_x = monitor->geom.w - status_width;

	/*
	 * If there isn't enough space, left-align. I'd prefer part of the status to be cut
	 * off rather than drawing over the workspace buttons.
	 */
	if(status_x < workspace_button_width) {
		status_x = workspace_button_width;
		status_width = monitor->geom.w - status_x;
	}

	XSetForeground(display, monitor->gfx_context,
		       mwm_get_color(monitor->mwm, dwdata.palette, MWM_COLOR_FOCUSED));
	XDrawRectangle(display, monitor->statusbar,
		       monitor->gfx_context, status_x, 0,
		       status_width, STATUSBAR_HEIGHT - 1);
	XSetForeground(display, monitor->gfx_context,
		       mwm_get_color(monitor->mwm, dwdata.palette, MWM_COLOR_BACKGROUND));
	XFillRectangle(display, monitor->statusbar,
		       monitor->gfx_context, status_x + 1, 0,
		       status_width - 1, STATUSBAR_HEIGHT - 1);
	mwm_render_text(monitor->mwm, monitor->xft_context, dwdata.palette, status,
			status_x + dwdata.text_padding, dwdata.text_padding);

	return(0);
}

int monitor_redraw(struct monitor *monitor)
{
	if(monitor->needs_redraw) {
		monitor_arrange_clients(monitor);
		monitor_draw_clients(monitor);

		monitor->needs_redraw = 0;
	}

	_redraw_statusbar(monitor);
	monitor_redraw_indicators(monitor);

	return(0);
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

int monitor_is_floating(struct monitor *monitor)
{
	if(!monitor) {
		return(FALSE);
	}

	return(monitor->layout == NULL);
}

int monitor_is_dirty(struct monitor *monitor)
{
	return(monitor->needs_redraw);
}

int monitor_is_focused(struct monitor *monitor)
{
	return(mwm_get_focused_monitor(monitor->mwm) == monitor);
}
