#include <stdio.h>
#include <getopt.h>

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
	int opt;

	do {
		opt = getopt_long(argc, argv, SHORTOPTS, cmd_opts, NULL);

		switch(opt) {
		case 'h':
			print_usage(argv[0]);
			return(1);

		default:
			opt = -1;
			break;
		}
	} while(opt >= 0);

	return(0);
}
