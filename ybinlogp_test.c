#define _XOPEN_SOURCE 600
#define _GNU_SOURCE

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "debugs.h"
#include "ybinlogp.h"

void usage(void) {
	fprintf(stderr, "ybinlogp_test [options] binlog\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options\n");
	fprintf(stderr, "  -h       show this help\n");
}

int main(int argc, char** argv) {
	int opt;
	int fd;
	while ((opt = getopt(argc, argv, "ht:o:a:qQvD:S")) != -1) {
		switch (opt) {
			case 'h':
				usage();
				return 0;
			case '?':
				fprintf(stderr, "Unknown argument %c\n", optopt);
				usage();
				return 1;
				break;
		}
	}
	if (optind >= argc) {
		usage();
		return 2;
	}
	if ((fd = open(argv[optind], O_RDONLY|O_LARGEFILE)) <= 0) {
		perror("Error opening file");
		return 1;
	}
	struct binlog_parser* bp;
	if (ybp_init_binlog_parser(fd, &bp) < 0) {
		perror("init_binlog_parser");
		return 1;
	}
	while (ybp_next_event(bp) != 0) {
	}
}
