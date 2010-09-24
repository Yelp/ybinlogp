#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "binlogp.h"

#define MAX_RETRIES	409600			/* how many bytes to seek ahead looking for a record */

#define GET_BIT(x,bit) (!!(x & 1 << bit))

#define HEADER_SIZE 19

/* binlog parameters */
#define MIN_TYPE_CODE 0
#define MAX_TYPE_CODE 27
#define MIN_EVENT_LENGTH 91
#define MAX_EVENT_LENGTH 1073741824
#define MAX_SERVER_ID 2147483648
#define MIN_TIMESTAMP 1262325600	/* 2010-01-01 00:00:00 */
time_t MAX_TIMESTAMP;				/* Set this time(NULL) on startup */


char* event_types[27] = {
	"UNKNOWN_EVENT",
	"START_EVENT_V3",
	"QUERY_EVENT",
	"STOP_EVENT",
	"ROTATE_EVENT",
	"INTVAR_EVENT",
	"LOAD_EVENT",
	"SLAVE_EVENT",
	"CREATE_FILE_EVENT",
	"APPEND_BLOCK_EVENT",
	"EXEC_LOAD_EVENT",
	"DELETE_FILE_EVENT",
	"NEW_LOAD_EVENT",
	"RAND_EVENT",
	"USER_VAR_EVENT",
	"FORMAT_DESCRIPTION_EVENT",
	"XID_EVENT",
	"BEGIN_LOAD_QUERY_EVENT",
	"EXECUTE_LOAD_QUERY_EVENT",
	"TABLE_MAP_EVENT",
	"PRE_GA_WRITE_ROWS_EVENT",
	"PRE_GA_DELETE_ROWS_EVENT",
	"WRITE_ROWS_EVENT",
	"UPDATE_ROWS_EVENT",
	"DELETE_ROWS_EVENT",
	"INCIDENT_EVENT",
	"HEARTBEAT_LOG_EVENT"
};

/*
 * Attempt to find the first event after a particular position in a mysql
 * binlog and, if you find it, print it out.
 *
 *
 */

void print_event(struct event *e) {
	char datebuf[20];
	time_t t = e->timestamp;
	struct tm *timebuf;
	timebuf = localtime(&t);
	datebuf[19] = '\0';
	strftime(datebuf, 20, "%Y-%m-%d %H:%M:%S", timebuf);
	printf("BYTE OFFSET:        %llu\n", (long long)e->offset);
	printf("timestamp:          %s (%d)\n", datebuf, e->timestamp);
	printf("type_code:          %s\n", event_types[e->type_code]);
	printf("server id:          %d\n", e->server_id);
	printf("length:             %d\n", e->event_length);
	printf("next pos:           %llu\n", (unsigned long long)e->next_position);
	printf("flags:              %d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d\n",
			GET_BIT(e->flags, 0),
			GET_BIT(e->flags, 1),
			GET_BIT(e->flags, 2),
			GET_BIT(e->flags, 3),
			GET_BIT(e->flags, 4),
			GET_BIT(e->flags, 5),
			GET_BIT(e->flags, 6),
			GET_BIT(e->flags, 7),
			GET_BIT(e->flags, 8),
			GET_BIT(e->flags, 9),
			GET_BIT(e->flags, 10),
			GET_BIT(e->flags, 11),
			GET_BIT(e->flags, 12),
			GET_BIT(e->flags, 13),
			GET_BIT(e->flags, 14),
			GET_BIT(e->flags, 15));
	switch (e->type_code) {
		case 2:				/* QUERY_EVENT */
			{
			printf("thread id:          %d\n", ((uint32_t*)e->data)[0]);
			printf("query time (s):     %d\n", ((uint32_t*)e->data)[1]);
			}
			break;
		case 15:			/* FORMAT_DESCRIPTION_EVENT */
			printf("binlog version:     %d\n", *((uint16_t*)e->data));
			printf("server version:     %s\n", (char*)(e->data + 2));
			break;
	}
}

void usage()
{
	fprintf(stderr, "Usage: binlogp logfile offset-start\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Prints the first plausible event after offset-start\n");
}

int check_event(struct event *e)
{
	if (e->type_code > MIN_TYPE_CODE &&
			e->type_code < MAX_TYPE_CODE &&
			e->event_length > MIN_EVENT_LENGTH &&
			e->event_length < MAX_EVENT_LENGTH && 
			e->timestamp > MIN_TIMESTAMP &&
			e->timestamp < MAX_TIMESTAMP &&
			e->server_id < MAX_SERVER_ID) {
		return 1;
	} else {
		return 0;
	}
}

int read_data(int fd, struct event *evbuf, off_t offset)
{
	ssize_t amt_read;
	if ((lseek(fd, offset, SEEK_SET) < 0)) {
		perror("Error seeking");
		return -1;
	}
	amt_read = read(fd, (void*)evbuf, HEADER_SIZE);
	evbuf->offset = offset;
	evbuf->data = 0;
	if (amt_read < 0) {
		fprintf(stderr, "Error reading event at %lld: %s\n", (long long) offset, strerror(errno));
		return -1;
	} else if ((size_t)amt_read != HEADER_SIZE) {
		fprintf(stderr, "Only able to read %zd bytes, good bye\n", amt_read);
		return -1;
	}
	/* Maybe read some extra data */
	switch (evbuf->type_code) {
		case 2:
			evbuf->data = read_extra_data(fd, offset + HEADER_SIZE, 8);
			break;
		case 15:
			evbuf->data = read_extra_data(fd, offset + HEADER_SIZE, 52);
			break;
	}
	return 0;
}

char* read_extra_data(int fd, off_t seek_pos, size_t amount)
{
	char* data;
	data = malloc(amount);
#if DEBUG
	fprintf(stderr, "Reading %zd bytes from %llu\n", amount, (long long)seek_pos);
#endif
	data = malloc(amount);
	if (pread(fd, data, amount, seek_pos) < 0) {
		perror("read in read_extra_data:");
	}
	return data;
}

/*
 * If necessary, free any dynamic data in evbuf
 */
int free_data(struct event *evbuf)
{
	if (evbuf->data != 0) {
		free(evbuf->data);
	}
	return 0;
}

/*
 * Get the first event after starting_offset in fd
 *
 * If evbuf is non-null, copy it into there
 */
long long nearest_offset(int fd, off_t starting_offset, struct stat *stbuf, struct event *outbuf)
{
	unsigned int num_increments = 0;
	off_t offset;
	struct event evbuf;
	offset = starting_offset;
#if DEBUG
	fprintf(stderr, "In nearest offset mode, got fd=%d, starting_offset=%llu\n", fd, (long long)starting_offset);
#endif
	while (num_increments < MAX_RETRIES) 
	{
		if (stbuf->st_size < offset) {
			fprintf(stderr, "file is only %lld bytes, requested %lld\n", (long long)stbuf->st_size, (long long)offset);
			return -1;
		}
#ifdef DEBUG
		//fprintf(stderr, "Seeking to %lld\n", (long long)offset);
#endif
		read_data(fd, &evbuf, offset);
		if (check_event(&evbuf)) {
			if (outbuf != NULL) {
				*outbuf = evbuf;
			}
			return offset;
		} else {
			++offset;
			++num_increments;
		}
		free_data(&evbuf);
	}
	return -2;
}

int nearest_time(int fd, time_t target, struct stat *stbuf, struct event *outbuf)
{
	off_t file_size = stbuf->st_size;
	struct event evbuf;
	off_t offset = file_size / 2;
	off_t next_increment = file_size / 4;
	int directionality = 0;
	off_t found;
	while (next_increment > 2) {
		int old_directionality = directionality;
		found = nearest_offset(fd, offset, stbuf, &evbuf);
		long long delta;
		if (found < 0) {
			return found;
		}
		delta = (evbuf.timestamp - target);
		if (delta > 0) {
			directionality = -1;
		}
		else if (delta < 0) {
			directionality = 1;
		}
		/* Success when we switch directions */
#if DEBUG
		fprintf(stderr, "delta=%d, directionality=%d, old_directionality=%d\n", delta, directionality, old_directionality);
#endif
		if (delta == 0) {
			break;
		}
		offset += next_increment * directionality;
		next_increment /= 2;
	}
	*outbuf = evbuf;
	return found;
	return 0;
}

int main(int argc, char **argv)
{
	int fd, exit_status;
	struct stat stbuf;
	int opt;
	
	int t_mode = 0;
	int o_mode = 0;

	MAX_TIMESTAMP = time(NULL);

	/* Parse args */
	while ((opt = getopt(argc, argv, "to")) != -1) {
		switch (opt) {
			case 't':		/* Time mode */
				t_mode = 1;
				break;
			case 'o':		/* Offset mode */
				o_mode = 1;
				break;
			default:
				usage();
				return 1;
		}
	}
	if (optind >= argc) {
		usage();
		return 1;
	}
	/* First argument is always filename */
	if (stat(argv[optind], &stbuf)) {
		perror("Error stat-ing file");
		return 1;
	}
#if DEBUG
	fprintf(stderr, "Opening file %s\n", argv[optind]);
#endif
	if ((fd = open(argv[optind], O_RDONLY)) <= 0) {
		perror("Error opening file");
		return 1;
	}
	exit_status = 0;
	if (t_mode) {
		time_t target_time = atol(argv[optind+1]);
		struct event evbuf;
		off_t offset;
		offset = nearest_time(fd, target_time, &stbuf, &evbuf);
		if (offset == -2) {
			fprintf(stderr, "Could not find any records\n");
		}
		if (offset < 0) {
			exit_status = 1;
		}
		else {
			print_event(&evbuf);
		}
	}
	else if (o_mode) {
		off_t starting_offset = atoll(argv[optind+1]);
		long long offset;
		struct event evbuf;
		offset = nearest_offset(fd, starting_offset, &stbuf, &evbuf);
		if (offset == -2) {
			fprintf(stderr, "Could not find any records\n");
		}
		if (offset < 0) {
			exit_status = 1;
		}
		else {
			print_event(&evbuf);
		}
	}
	close(fd);
	return exit_status;
}
