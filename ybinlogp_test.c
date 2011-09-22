#define _XOPEN_SOURCE 600
#define _GNU_SOURCE

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

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
	struct ybp_binlog_parser* bp;
	struct ybp_event* evbuf;
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
	if (ybp_init_binlog_parser(fd, &bp) < 0) {
		perror("init_binlog_parser");
		return 1;
	}
	if ((evbuf = malloc(sizeof(struct ybp_event))) == NULL) {
		perror("malloc event");
		return 1;
	}
	ybp_init_event(evbuf);
	while (ybp_next_event(bp, evbuf) >= 0) {
		switch(evbuf->type_code) {
			case FORMAT_DESCRIPTION_EVENT:
				{
				struct ybp_format_description_event* fde = ybp_event_as_fde(evbuf);
				time_t buf_time = evbuf->timestamp;
				printf("FDE:\n");
				printf("\t%s\t%s\n", ctime(&buf_time), fde->server_version);
				break;
				}
			default:
				printf("%s\n", ybp_event_type(evbuf));
		}
		ybp_reset_event(evbuf);
	}
	ybp_dispose_event(evbuf);
	ybp_dispose_binlog_parser(bp);
}
