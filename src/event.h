#ifndef EVENT_H
#define EVENT_H 1

#include "common.h"
#include "xrandr.h"
#include <sys/time.h>
#include <X11/Xlib.h>

enum event_type {
	EVENT_NONE = 0,
	/* EVENT_BUTTON_PRESS, */
	/* EVENT_CLIENT_MESSAGE, */
	EVENT_ROOT_GEOMETRY_CHANGE,
	EVENT_CONFIGURE_NOTIFY,
	EVENT_CONFIGURE_REQUEST,
	EVENT_DESTROY_NOTIFY,
	EVENT_ENTER_NOTIFY,
	EVENT_EXPOSE,
	EVENT_FOCUS_IN,
	EVENT_KEY_PRESS,
	EVENT_MAPPING_NOTIFY,
	EVENT_MAP_REQUEST,
	EVENT_MOTION_NOTIFY,
	EVENT_PROPERTY_NOTIFY,
	EVENT_UNMAP_NOTIFY,
	EVENT_CRTC_CHANGE,
	EVENT_OUTPUT_CHANGE,
	/* EVENT_SCREEN_CHANGE, */
	EVENT_LAST
};

typedef enum event_type event_type_t;

struct event {
	event_type_t type;
	struct timeval time;

	union {
		/* struct {} button_press; */
		/* struct {} client_message; */
		struct {
			Window window;
			Window above;
			struct geom geom;
			int detail;
			unsigned long value_mask;
			client_t client;
		} configure_request;

		struct {
			Window window;
			struct geom geom;
		} configure_notify;

		struct {
			struct geom geom;
		} root_geom_change;

		struct {
			client_t client;
		} destroy_notify;

		struct {
			client_t client;
			monitor_t monitor;
			int mode;
			int detail;
			Window window;
		} enter_notify;

		struct {
			monitor_t monitor;
			Window window;
			int count;
		} expose;

		struct {
			client_t client;
			Window window;
		} focus_in;

		struct {
			KeySym keysym;
			unsigned int mask;
		} key_press;

		struct {
			XMappingEvent xevent;
		} mapping_notify;

		struct {
			Window window;
			client_t client;
		} map_request;

		struct {
			struct geom pointer;
			client_t client;
			monitor_t monitor;
		} motion_notify;

		struct {
			Window window;
			client_t client;
			XPropertyEvent xevent;
		} property_notify;

		struct {
			client_t client;
			int send_event;
		} unmap_notify;

		struct {
			xrandr_crtc_t crtc;
			struct geom geom;
			int rotation;
			int mode;
		} crtc_change;

		struct {
			xrandr_crtc_t crtc;
			int output;
			int connection;
		} output_change;
	} data;
};

int event_new(struct event **event, event_type_t type);
int event_free(struct event **event);

int event_nq(struct event *event);
int event_dq(struct event **event);

#endif /* EVENT_H */
