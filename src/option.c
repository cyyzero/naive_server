#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include "option.h"

#define _XOPEN_SOURCE

static void print_usage(const char *prog)
{
	printf(
		"Syntax: %s [ OPTS ] <docroot>\n"
		" -p      - port\n", prog);
	exit(-1);
}

struct options parse_options(int argc, char **argv)
{
	struct options o;
	int opt;

	memset(&o, 0, sizeof(o));

	while ((opt = getopt(argc, argv, "hp:")) != -1) {
		switch (opt) {
			case 'p': o.port = atoi(optarg); break;
			case 'h': print_usage(argv[0]); break;
			default : fprintf(stderr, "Unknown option %c\n", opt); break;
		}
	}

	if (optind >= argc || (argc - optind) > 1) {
		print_usage(argv[0]);
	}
	o.docroot = argv[optind];

	return o;
}