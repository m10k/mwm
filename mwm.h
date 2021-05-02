#ifndef MWM_H
#define MWM_H 1

struct mwm;
struct monitor;

int mwm_new(struct mwm **mwm);
int mwm_free(struct mwm **mwm);

int mwm_init(struct mwm *mwm);
int mwm_run(struct mwm *mwm);

int mwm_attach_monitor(struct mwm *mwm, struct monitor *mon);
int mwm_detach_monitor(struct mwm *mwm, struct monitor *mon);

#endif /* MWM_H */
