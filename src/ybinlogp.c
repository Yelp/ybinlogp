/*
 * ybinlogp: A mysql binary log parser and query tool
 *
 * (C) 2010-2011 Yelp, Inc.
 *
 * This work is licensed under the ISC/OpenBSD License. The full
 * contents of that license can be found under license.txt
 */

#define _XOPEN_SOURCE 600
#define _GNU_SOURCE

#include <fcntl.h>
#include <stdbool.h>
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
	fprintf(stderr, "\t-h           show this help\n");
	fprintf(stderr, "\t-E           do not enforce server-id checking\n");
	fprintf(stderr, "\t-o OFFSET    find the first event after the given offset\n");
	fprintf(stderr, "\t-t TIME      find the first event after the given time\n");
	fprintf(stderr, "\t-a COUNT     When used with one of the above, print COUNT items after the first one, default 2\n");
	fprintf(stderr, "\t\t\t\tAccepts either an integer or the text 'all'\n");
	fprintf(stderr, "\t-D DBNAME    Filter query events that were not in DBNAME\n");
	fprintf(stderr, "\t\t\t\tNote that this still shows transaction control events\n");
	fprintf(stderr, "\t\t\t\tsince those do not have an associated database. Mea culpa.\n");
	fprintf(stderr, "\t-q           Be quieter\n");
}

int main(int argc, char** argv) {
	int opt;
	int fd;
	struct ybp_binlog_parser* bp;
	struct ybp_event* evbuf;
	long starting_offset = -1;
	long starting_time = -1;
	int num_to_show = 2;
	int show_all = false;
	bool q_mode = false;
	bool esi = true;
	char* database_limit = NULL;
	while ((opt = getopt(argc, argv, "ho:t:a:D:qE")) != -1) {
		switch (opt) {
			case 'h':
				usage();
				return 0;
			case 'o':      /* Offset mode */
				starting_offset = atoll(optarg);
				break;
			case 'E':
				esi = false;
				break;
			case 't':      /* Time mode */
				starting_time = atoll(optarg);
				break;
			case 'a':
				if (strncmp(optarg, "all", 3) == 0) {
					num_to_show = 2;
					show_all = 1;
					break;
				}
				num_to_show = atoi(optarg);
				if (num_to_show < 1)
					num_to_show = 1;
				break;
			case 'D':
				database_limit = strdup(optarg);
				break;
			case 'q':
				q_mode = true;
				break;
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
	if ((bp = ybp_get_binlog_parser(fd)) == NULL) {
		perror("init_binlog_parser");
		return 1;
	}
	bp->enforce_server_id = esi;
	if ((evbuf = malloc(sizeof(struct ybp_event))) == NULL) {
		perror("malloc event");
		return 1;
	}
	ybp_init_event(evbuf);
	if (starting_offset >= 0) {
		off64_t offset = ybp_nearest_offset(bp, starting_offset);
		if (offset == -2) {
			fprintf(stderr, "Unable to find anything after offset %ld\n", starting_offset);
			return 1;
		}
		else if (offset == -1) {
			perror("nearest_offset");
			return 1;
		}
		else {
			ybp_rewind_bp(bp, offset);
		}
	}
	if (starting_time >= 0) {
		off64_t offset = ybp_nearest_time(bp, starting_time);
		if (offset == -2) {
			fprintf(stderr, "Unable to find anything after time %ld\n", starting_time);
			return 1;
		}
		else if (offset == -1) {
			perror("nearest_time");
			return 1;
		}
		else {
			ybp_rewind_bp(bp, offset);
		}
	}
	int i = 0;
	while ((ybp_next_event(bp, evbuf) >= 0) && (show_all || i < num_to_show)) {
		if (q_mode) {
			if (evbuf->type_code == QUERY_EVENT) {
				struct ybp_query_event_safe* s = ybp_event_to_safe_qe(evbuf);
				if ((database_limit == NULL) || (strcmp(s->db_name, database_limit) == 0))  {
					printf("%d %s\n", evbuf->timestamp, s->statement);
				}
				ybp_dispose_safe_qe(s);
			}
			else if (evbuf->type_code == XID_EVENT) {
				struct ybp_xid_event* s = ybp_event_to_safe_xe(evbuf);
				printf("%d XID %llu\n", evbuf->timestamp, (long long unsigned)s->id);
				ybp_dispose_safe_xe(s);
			}
		} else {
			ybp_print_event(evbuf, bp, stdout, q_mode, false, database_limit);
			fprintf(stdout, "\n");
		}
		ybp_reset_event(evbuf);
		i+=1;
	}
	ybp_dispose_event(evbuf);
	ybp_dispose_binlog_parser(bp);
}

/* vim: set sts=0 sw=4 ts=4 noexpandtab: */
