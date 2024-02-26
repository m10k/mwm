#include <stdlib.h>
#include <errno.h>
#include <X11/extensions/Xrandr.h>
#include <stdio.h>
#include <string.h>
#include "xrandr.h"
#include "loop.h"
#include "common.h"

#define FIND_OUTPUT_BY_ID ((int(*)(void*, void*))_cmp_output_id)
#define FIND_CRTC_BY_ID   ((int(*)(void*, void*))_cmp_crtc_id)

struct crtc {
	xrandr_crtc_t id;
	int outputs;
	int active_outputs;
	struct geom geom;
};

struct output {
	xrandr_output_t id;
	xrandr_crtc_t crtc;
	int connected;
};

struct xrandr {
	Display *display;
	Window window;

	int event_base;
	int error_base;

	struct loop *crtcs;
	struct loop *outputs;

	struct {
		xrandr_func_t *handler;
		void *userdata;
	} events[XRANDR_EVENT_MAX];
};

static int _cmp_output_id(xrandr_output_t *left, xrandr_output_t *right)
{
	return *left - *right;
}

static int _cmp_crtc_id(xrandr_crtc_t *left, xrandr_crtc_t *right)
{
	return *left - *right;
}

int xrandr_new(struct xrandr **dst, Display *display, Window window)
{
	struct xrandr *xrr;
	int event_base;
	int error_base;
	int event_mask;

#if MWM_DEBUG_XRANDR
	fprintf(stderr, "%s()\n", __func__);
#endif /* MWM_DEBUG_XRANDR */

	if (!XRRQueryExtension(display, &event_base, &error_base)) {
		return -ENOTSUP;
	}

	if (!(xrr = calloc(1, sizeof(*xrr)))) {
		return -ENOMEM;
	}

	xrr->event_base = event_base;
	xrr->error_base = error_base;
	xrr->display = display;
	xrr->window = window;

#if MWM_DEBUG_XRANDR
	fprintf(stderr, "%s: event_base = %x, error_base = %x\n",
	        __func__, event_base, error_base);
#endif /* MWM_DEBUG_XRANDR */

	event_mask = RRCrtcChangeNotifyMask   |
	             RROutputChangeNotifyMask |
	             RRScreenChangeNotifyMask;

#if MWM_DEBUG_XRANDR
	fprintf(stderr, "%s: event_mask = %x\n", __func__, event_mask);
#endif /* MWM_DEBUG_XRANDR */

	XRRSelectInput(display, window, event_mask);

	*dst = xrr;
	return 0;
}

int xrandr_free(struct xrandr **xrr)
{
	if (!xrr) {
		return -EINVAL;
	}

	if (!*xrr) {
		return -EALREADY;
	}

	loop_foreach(&(*xrr)->crtcs, free);
	loop_free(&(*xrr)->crtcs);
	loop_foreach(&(*xrr)->outputs, free);
	loop_free(&(*xrr)->outputs);

	free(*xrr);
	*xrr = NULL;

	return 0;
}

static void _invoke_handler(struct xrandr *xrr,
                            xrandr_event_t event,
                            xrandr_crtc_t crtc,
                            struct geom *geom)
{
	if (xrr->events[event].handler) {
		xrr->events[event].handler(xrr, crtc, geom, xrr->events[event].userdata);
	}
}

static void _handle_screen_change_event(struct xrandr *xrr,
                                        XRRScreenChangeNotifyEvent *event)
{
#if MWM_DEBUG_XRANDR
	fprintf(stderr, "%s()\n", __func__);
#endif /* MWM_DEBUG_XRANDR */

	/* TODO: Do we need to handle screen change notifications? */
	return;
}

static struct crtc* _add_crtc(struct xrandr *xrr,
                              xrandr_crtc_t crtc_id);
static struct output* _add_output(struct xrandr *xrr,
                                  xrandr_output_t output_id);

static struct crtc* _add_crtc(struct xrandr *xrr,
                              xrandr_crtc_t crtc_id)
{
	struct crtc *crtc;

	if ((crtc = calloc(1, sizeof(*crtc)))) {
		crtc->id = crtc_id;

		if (loop_append(&xrr->crtcs, crtc) < 0) {
			free(crtc);
			crtc = NULL;
		}
	}

	return crtc;
}

static struct output* _add_output(struct xrandr *xrr,
                                  xrandr_output_t output_id)
{
	struct output *output;

	if ((output = calloc(1, sizeof(*output)))) {
		output->id = output_id;

		if (loop_append(&xrr->outputs, output) < 0) {
			free(output);
			output = NULL;
		}
	}

	return output;
}

static struct crtc* _get_crtc(struct xrandr *xrr,
                              xrandr_crtc_t crtc_id)
{
	struct crtc *crtc;

	if (loop_find(&xrr->crtcs, FIND_CRTC_BY_ID,
	              (void*)&crtc_id, (void**)&crtc) < 0) {
		crtc = _add_crtc(xrr, crtc_id);
	}

	return crtc;
}

static struct output* _get_output(struct xrandr *xrr,
                                  xrandr_output_t output_id)
{
	struct output *output;

	if (loop_find(&xrr->outputs, FIND_OUTPUT_BY_ID,
	              (void*)&output_id, (void**)&output) < 0) {
		output = _add_output(xrr, output_id);
	}

	return output;
}

static void _update_associations(struct xrandr *xrr);

static int _update_output(struct xrandr *xrr,
                          xrandr_output_t output_id,
                          xrandr_crtc_t crtc_id,
                          int connected)
{
	struct output *output;
	int output_changed;

	if (!(output = _get_output(xrr, output_id))) {
		return FALSE;
	}

	output_changed = output->crtc != crtc_id ||
	                 output->connected != connected;
	output->crtc = crtc_id;
	output->connected = connected;

	if (output_changed) {
		_update_associations(xrr);
	}

	return output_changed;
}

static int _update_crtc(struct xrandr *xrr,
			xrandr_crtc_t crtc_id,
			struct geom *geom)
{
	struct crtc *crtc;
	int crtc_changed;

	if (!(crtc = _get_crtc(xrr, crtc_id))) {
		return FALSE;
	}

	crtc_changed = memcmp(geom, &crtc->geom, sizeof(*geom)) != 0;
	if (crtc_changed) {
		fprintf(stderr, "%s: %lx changed [%d/%d]\n", __func__, crtc_id,
		        crtc->outputs, crtc->active_outputs);
		memcpy(&crtc->geom, geom, sizeof(*geom));

		if (geom->w > 0 && geom->h > 0) {
			_invoke_handler(xrr, XRANDR_MONITOR_GEOMETRY_CHANGED,
			                crtc_id, geom);
		} else {
			_invoke_handler(xrr, XRANDR_MONITOR_DETACHED, crtc_id, geom);
		}
	}

	return crtc_changed;
}

static int _count_crtc_outputs(struct output *output, struct crtc *crtc)
{
	if (output->crtc == crtc->id) {
		crtc->outputs++;

		if (output->connected) {
			crtc->active_outputs++;
		}
	}

	return 0;
}

static int _update_crtc_outputs(struct crtc *crtc, struct xrandr *xrr)
{
	int was_attached;

	fprintf(stderr, "%s: %lx outputs B: %d/%d\n", __func__, crtc->id, crtc->outputs, crtc->active_outputs);

	was_attached = crtc->active_outputs > 0;
	crtc->outputs = 0;
	crtc->active_outputs = 0;

	loop_foreach_with_data(&xrr->outputs, (int(*)(void*, void*))_count_crtc_outputs, crtc);

	fprintf(stderr, "%s: %lx outputs A: %d/%d\n", __func__, crtc->id, crtc->outputs, crtc->active_outputs);

	if (was_attached && crtc->active_outputs == 0) {
		_invoke_handler(xrr, XRANDR_MONITOR_DETACHED, crtc->id, &crtc->geom);
	} else if (!was_attached && crtc->active_outputs > 0) {
		_invoke_handler(xrr, XRANDR_MONITOR_ATTACHED, crtc->id, &crtc->geom);
	}

	return 0;
}

static void _update_associations(struct xrandr *xrr)
{
	loop_foreach_with_data(&xrr->crtcs, (int(*)(void*, void*))_update_crtc_outputs, xrr);
	return;
}

static void _handle_output_change_event(struct xrandr *xrr,
                                        XRROutputChangeNotifyEvent *event)
{
#if MWM_DEBUG_XRANDR
	fprintf(stderr, "%s()\n", __func__);
#endif /* MWM_DEBUG_XRANDR */

	if (!event->output) {
		/* No way to sensibly handle this event */
		return;
	}

	_update_output(xrr, event->output, event->crtc,
	               event->connection == RR_Connected);
	return;
}

static void _handle_crtc_change_event(struct xrandr *xrr, XRRCrtcChangeNotifyEvent *event)
{
	struct geom geom;

#if MWM_DEBUG_XRANDR
	fprintf(stderr, "%s: crtc=%lx, %dx%d @ %d,%d, mode=%lx\n",
	        __func__, event->crtc, event->width, event->height,
	        event->x, event->y, event->mode);
#endif /* MWM_DEBUG_XRANDR */

	if (!event->crtc) {
		/* No way to sensibly handle this event */
		return;
	}

	geom.x = event->x;
	geom.y = event->y;
	geom.w = event->width;
	geom.h = event->height;

	_update_crtc(xrr, event->crtc, &geom);
	return;
}

void xrandr_handle_event(struct xrandr *xrr, XEvent *event)
{
#if MWM_DEBUG_XRANDR
	fprintf(stderr, "%s() -> %x\n", __func__, event->type);
#endif /* MWM_DEBUG_XRANDR */

	switch (event->type - xrr->event_base) {
	case RRScreenChangeNotify:
		_handle_screen_change_event(xrr, (XRRScreenChangeNotifyEvent*)event);
		break;

	case RRNotify:
		fprintf(stderr, "%s: %x\n", __func__, ((XRRNotifyEvent*)event)->subtype);
		switch (((XRRNotifyEvent*)event)->subtype) {
		case RRNotify_OutputChange:
			_handle_output_change_event(xrr, (XRROutputChangeNotifyEvent*)event);
			break;

		case RRNotify_CrtcChange:
			_handle_crtc_change_event(xrr, (XRRCrtcChangeNotifyEvent*)event);
			break;

		default:
			/* don't care */
			break;
		}
		break;

	default:
		/* don't care */
		break;
	}

	return;
}

void xrandr_update(struct xrandr *xrr)
{
	XRRScreenResources *resources;
	int i;

	if (!(resources = XRRGetScreenResources(xrr->display, xrr->window))) {
		return;
	}

#if MWM_DEBUG_XRANDR
	fprintf(stderr, "Found %d CRTCs\n", resources->ncrtc);
#endif /* MWM_DEBUG_XRANDR */

	/* TODO: Ideally, this should call the same methods as the event handlers */

	for (i = 0; i < resources->ncrtc; i++) {
		xrandr_crtc_t crtc_id;
		XRRCrtcInfo *crtc_info;
		struct geom geom;

		crtc_id = resources->crtcs[i];

		if (!(crtc_info = XRRGetCrtcInfo(xrr->display, resources, crtc_id))) {
			continue;
		}

		geom.x = crtc_info->x;
		geom.y = crtc_info->y;
		geom.w = crtc_info->width;
		geom.h = crtc_info->height;

#if MWM_DEBUG_XRANDR
		fprintf(stderr, "New CRTC: %lx [%dx%d @ %d,%d]\n",
		        crtc_id, geom.w, geom.h, geom.x, geom.y);
#endif /* MWM_DEBUG_XRANDR */

		_update_crtc(xrr, crtc_id, &geom);

		XRRFreeCrtcInfo(crtc_info);
	}

#if MWM_DEBUG_XRANDR
	fprintf(stderr, "Found %d outputs\n", resources->noutput);
#endif /* MWM_DEBUG_XRANDR */

	for (i = 0; i < resources->noutput; i++) {
		xrandr_output_t output_id;
		XRROutputInfo *output_info;

		output_id = resources->outputs[i];

		if (!(output_info = XRRGetOutputInfo(xrr->display, resources, output_id))) {
			continue;
		}

#if MWM_DEBUG_XRANDR
		fprintf(stderr, "New output %lx -> %lx [%x]\n",
		        output_id,
		        output_info->crtc,
		        output_info->connection);
#endif /* MWM_DEBUG_XRANDR */

		_update_output(xrr, output_id,
		               output_info->crtc, /* ncrtc > 0 ? output_info->crtcs[0] : 0, */
		               output_info->connection == RR_Connected);

		XRRFreeOutputInfo(output_info);
	}

	XRRFreeScreenResources(resources);

	return;
}

int xrandr_set_callback(struct xrandr *xrr,
                        xrandr_event_t event,
                        xrandr_func_t *handler,
                        void *userdata)
{
	if (!xrr || event >= XRANDR_EVENT_MAX) {
		return -EINVAL;
	}

	xrr->events[event].handler = handler;
	xrr->events[event].userdata = userdata;

	return 0;
}
