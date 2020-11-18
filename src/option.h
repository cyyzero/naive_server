#ifndef OPTION_H
#define OPTION_H

struct options {
	int port;
	const char *docroot;
};

extern struct options parse_options(int argc, char **argv);

#endif // OPTION_H