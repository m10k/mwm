#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include "mwm.h"

#define SHORTOPTS "h"

static struct option cmd_opts[] = {
	{ "help", no_argument, 0, 'h' },
	{ NULL }
};

static void print_usage(const char *argv0)
{
	printf("Usage: %s [-h]\n"
	       "\n"
	       "Options:\n"
	       " -h  --help     Print this text\n",
	       argv0);
	return;
}

int main(int argc, char *argv[])
{
	struct mwm *mwm;
	int opt;
	int err;

	do {
		opt = getopt_long(argc, argv, SHORTOPTS,
				  cmd_opts, NULL);

		switch(opt) {
		case 'h':
			print_usage(argv[0]);
			return(1);

		default:
			opt = -1;
			break;
		}
	} while(opt >= 0);

	err = mwm_new(&mwm);

	if(err < 0) {
		fprintf(stderr, "mwm_new: %s\n", strerror(-err));
		return(1);
	}

	err = mwm_init(mwm);

	if(err < 0) {
		fprintf(stderr, "mwm_init: %s\n", strerror(-err));
	} else {
		err = mwm_run(mwm);

		if(err < 0) {
			fprintf(stderr, "mwm_run: %s\n",
				strerror(-err));
		}
	}

	mwm_free(&mwm);

	return(err);
}
