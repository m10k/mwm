#ifndef MWM_KEYS_H
#define MWM_KEYS_H

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "mwm.h"

struct key_binding {
        unsigned int mod;
        KeySym key;
	mwm_cmd_t cmd;
        void *arg;
};

#endif /* MWM_KEYS_H */
