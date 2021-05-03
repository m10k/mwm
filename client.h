#ifndef MWM_CLIENT_H
#define MWM_CLIENT_H 1

#include <X11/Xlib.h>

typedef enum {
	CLIENT_FIXED      = (1 << 0),
	CLIENT_FLOATING   = (1 << 1),
	CLIENT_URGENT     = (1 << 2),
	CLIENT_NEVERFOCUS = (1 << 3),
	CLIENT_FULLSCREEN = (1 << 4)
} client_flags_t;

struct client;

int client_new(Window window, XWindowAttributes *attrs, struct client**);
int client_free(struct client**);

#endif /* MWM_CLIENT_H */
