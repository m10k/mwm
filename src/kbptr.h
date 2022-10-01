#ifndef MWM_KBPTR_H
#define MWM_KBPTR_H

struct mwm;
struct client;

#define KBPTR_CENTER   0
#define KBPTR_NORTH    (1 << 1)
#define KBPTR_EAST     (1 << 2)
#define KBPTR_SOUTH    (1 << 3)
#define KBPTR_WEST     (1 << 4)
#define KBPTR_DMASK    (KBPTR_NORTH | KBPTR_EAST | KBPTR_SOUTH | KBPTR_WEST)
#define KBPTR_HALFSTEP (1 << 0)

#define KBPTR_LEFT     Button1
#define KBPTR_MIDDLE   Button2
#define KBPTR_RIGHT    Button3

void kbptr_move(struct mwm*, struct client *client, long direction);
void kbptr_click(struct mwm*, struct client *client, long button);

#endif /* MWM_KBPTR_H */
