#ifndef MWM_H
#define MWM_H 1

#include <X11/Xlib.h>

struct mwm;
struct monitor;

typedef enum {
	MWM_EVENT_MONITOR_ATTACHED = 0,
	MWM_EVENT_MONITOR_DETACHED,
	MWM_EVENT_LAST
} mwm_event_t;

typedef void (mwm_handler_t)(struct mwm *mwm, mwm_event_t, void *);

int mwm_new(struct mwm **mwm);
int mwm_free(struct mwm **mwm);

int mwm_init(struct mwm *mwm);
int mwm_run(struct mwm *mwm);
int mwm_stop(struct mwm *mwm);

int mwm_attach_monitor(struct mwm *mwm, struct monitor *mon);
int mwm_detach_monitor(struct mwm *mwm, struct monitor *mon);

#endif /* MWM_H */
