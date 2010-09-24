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

#define MAX_RETRIES	102400  /* how many bytes to seek ahead looking for a record */

#define GET_BIT(x,bit) (!!(x & 1 << bit))

#define EVENT_HEADER_SIZE 19

/* binlog parameters */
#define MIN_TYPE_CODE 0
#define MAX_TYPE_CODE 27
#define MIN_EVENT_LENGTH 91
#define MAX_EVENT_LENGTH 1073741824
#define MAX_SERVER_ID 2147483648
/* #define MIN_TIMESTAMP 1262325600	* 2010-01-01 00:00:00 */
time_t MIN_TIMESTAMP;				/* set to the time in the FDE */
time_t MAX_TIMESTAMP;				/* Set this time(NULL) on startup */


char* event_types[27] = {
	"UNKNOWN_EVENT",			// 0
	"START_EVENT_V3",			// 1
	"QUERY_EVENT",				// 2
	"STOP_EVENT",				// 3
	"ROTATE_EVENT",				// 4
	"INTVAR_EVENT",				// 5
	"LOAD_EVENT",				// 6
	"SLAVE_EVENT",				// 7
	"CREATE_FILE_EVENT",			// 8
	"APPEND_BLOCK_EVENT",			// 9
	"EXEC_LOAD_EVENT",			// 10
	"DELETE_FILE_EVENT",			// 11
	"NEW_LOAD_EVENT",			// 12
	"RAND_EVENT",				// 13
	"USER_VAR_EVENT",			// 14
	"FORMAT_DESCRIPTION_EVENT",		// 15
	"XID_EVENT",				// 16
	"BEGIN_LOAD_QUERY_EVENT",		// 17
	"EXECUTE_LOAD_QUERY_EVENT",		// 18
	"TABLE_MAP_EVENT",			// 19
	"PRE_GA_WRITE_ROWS_EVENT",		// 20
	"PRE_GA_DELETE_ROWS_EVENT",		// 21
	"WRITE_ROWS_EVENT",			// 22
	"UPDATE_ROWS_EVENT",			// 23
	"DELETE_ROWS_EVENT",			// 24
	"INCIDENT_EVENT",			// 25
	"HEARTBEAT_LOG_EVENT"			// 26
};

char* variable_types[10] = {
	"Q_FLAGS2_CODE",			// 0
	"Q_SQL_MODE_CODE",			// 1
	"Q_CATALOG_CODE",			// 2
	"Q_AUTO_INCREMENT",			// 3
	"Q_CHARSET_CODE",			// 4
	"Q_TIME_ZONE_CODE",			// 5
	"Q_CATALOG_NZ_CODE",			// 6
	"Q_LC_TIME_NAMES_CODE",			// 7
	"Q_CHARSET_DATABASE_CODE",		// 8
	"Q_TABLE_MAP_FOR_UPDATE_CODE",		// 9
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
	printf("length:             %d\n", e->length);
	printf("next pos:           %llu\n", (unsigned long long)e->next_position);
	printf("flags:              %hhd%hhd%hhd%hhd%hhd%hhd%hhd%hhd%hhd%hhd%hhd%hhd%hhd%hhd%hhd%hhd\n",
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
	if (e->data == NULL) {
		return;
	}
	switch (e->type_code) {
		case 2:				/* QUERY_EVENT */
			{
			struct query_event *q = (struct query_event*)e->data;
			char* status_vars = (e->data + sizeof(struct query_event));
			char* db_name = (status_vars + q->status_var_len);
			char* statement = (db_name + q->db_name_len + 1);
			printf("thread id:          %d\n", q->thread_id);
			printf("query time (s):     %f\n", q->query_time);
			if (q->error_code == 0) {
				printf("error code:         %d\n", q->error_code);
			}
			else {
				printf("ERROR CODE:         %d\n", q->error_code);
			}
			printf("db_name:            %s\n", db_name);
			printf("statement:          %s\n", statement);
			}
			break;
		case 13:
			{
			struct rand_event *r = (struct rand_event*)e->data;
			printf("seed 1:	            %llu\n", (unsigned long long) r->seed_1);
			printf("seed 2:	            %llu\n", (unsigned long long) r->seed_2);
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
	fprintf(stderr, "Usage: binlogp [mode] logfile [mode-args]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "binlogp supports several different modes:\n");
	fprintf(stderr, "\t-o Find the first event after the given offset\n");
	fprintf(stderr, "\t\tbinlogp -o offset logfile\n");
	fprintf(stderr, "\t-t Find the event closest to the given unix time\n");
	fprintf(stderr, "\t\tbinlogp -t timestamp logfile\n");
	fprintf(stderr, "\t-a When used with one of the above, print N items after the first one\n");
	fprintf(stderr, "\t\tbinlogp -a N -t timestamp logfile\n");
}

int check_event(struct event *e)
{
	if (e->server_id < MAX_SERVER_ID &&
			e->type_code > MIN_TYPE_CODE &&
			e->type_code < MAX_TYPE_CODE &&
			e->length > MIN_EVENT_LENGTH &&
			e->length < MAX_EVENT_LENGTH && 
			e->timestamp >= MIN_TIMESTAMP &&
			e->timestamp <= MAX_TIMESTAMP) {
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
	amt_read = read(fd, (void*)evbuf, EVENT_HEADER_SIZE);
	evbuf->offset = offset;
	evbuf->data = 0;
	if (amt_read < 0) {
		fprintf(stderr, "Error reading event at %lld: %s\n", (long long) offset, strerror(errno));
		return -1;
	} else if ((size_t)amt_read != EVENT_HEADER_SIZE) {
		fprintf(stderr, "Only able to read %zd bytes, good bye\n", amt_read);
		return -1;
	}
	return 0;
}

/**
 * Maybe read in some dynamic data
 **/
int read_extra_data(int fd, struct event *evbuf)
{
	if (evbuf->data != 0) {
		free(evbuf->data);
		evbuf->data = 0;
	}
#if DEBUG
	fprintf(stderr, "mallocing %d bytes\n", evbuf->length - EVENT_HEADER_SIZE);
#endif
	if ((evbuf->data = malloc(evbuf->length - EVENT_HEADER_SIZE)) == NULL) {
		perror("malloc:");
		return -1;
	}
	if (pread(fd, evbuf->data, evbuf->length - EVENT_HEADER_SIZE, evbuf->offset + EVENT_HEADER_SIZE) < 0) {
		perror("reading extra data:");
		return -1;
	}
	return 0;
}

/*
 * If necessary, free any dynamic data in evbuf
 */
int try_free_data(struct event *evbuf)
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
long long nearest_offset(int fd, off_t starting_offset, struct stat *stbuf, struct event *outbuf, int direction)
{
	unsigned int num_increments = 0;
	off_t offset;
	struct event evbuf;
	offset = starting_offset;
#if DEBUG
	fprintf(stderr, "In nearest offset mode, got fd=%d, starting_offset=%llu\n", fd, (long long)starting_offset);
#endif
	while (num_increments < MAX_RETRIES && offset >= 0) 
	{
		if (stbuf->st_size < offset) {
			fprintf(stderr, "file is only %lld bytes, requested %lld\n", (long long)stbuf->st_size, (long long)offset);
			return -1;
		}
#if DEBUG
		//fprintf(stderr, "Seeking to %lld\n", (long long)offset);
#endif
		if (read_data(fd, &evbuf, offset) < 0) {
			return -1;
		}
		if (check_event(&evbuf)) {
			read_extra_data(fd, &evbuf);
			if (outbuf != NULL) {
				*outbuf = evbuf;
			}
			return offset;
		} else {
			offset += direction;
			++num_increments;
		}
		try_free_data(&evbuf);
	}
#if DEBUG
	fprintf(stderr, "Unable to find anything (offset=%llu)\n",(long long) offset);
#endif
	return -2;
}

int nearest_time(int fd, time_t target, struct stat *stbuf, struct event *outbuf)
{
	off_t file_size = stbuf->st_size;
	struct event evbuf;
	off_t offset = file_size / 2;
	off_t next_increment = file_size / 4;
	int directionality = 1;
	off_t found;
	while (next_increment > 2) {
		found = nearest_offset(fd, offset, stbuf, &evbuf, directionality);
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
		fprintf(stderr, "delta=%lld at %llu, directionality=%d, next_increment=%lld\n", (long long)delta, (unsigned long long)found, directionality, (long long)next_increment);
#endif
		if (delta == 0) {
			break;
		}
	  	if (directionality == -1) {
			offset += (next_increment  * directionality);
		}
		else {
			offset += (next_increment * directionality);
		}
		next_increment /= 2;
	}
	*outbuf = evbuf;
	return found;
}

/**
 * Read the FDE and set the server-id
 **/
int read_fde(int fd)
{
	struct event evbuf;
	read_data(fd, &evbuf, 4);
#if DEBUG
	fprintf(stderr, "Got server-id of %d\n", evbuf.server_id);
#endif
	MIN_TIMESTAMP = evbuf.timestamp;
	try_free_data(&evbuf);
	return 0;
}

int main(int argc, char **argv)
{
	int fd, exit_status;
	struct stat stbuf;
	struct event evbuf;
	int opt;
	off_t offset;
	int shown;
	
	time_t target_time;
	off_t starting_offset;
	int num_to_show = 1;
	int t_mode = 0;
	int o_mode = 0;

	MAX_TIMESTAMP = time(NULL);

	/* Parse args */
	while ((opt = getopt(argc, argv, "t:o:a:")) != -1) {
		switch (opt) {
			case 't':		/* Time mode */
				target_time = atol(optarg);
				t_mode = 1;
				break;
			case 'o':		/* Offset mode */
				starting_offset = atoll(optarg);
				o_mode = 1;
				break;
			case 'a':
				num_to_show = atoi(optarg);
				if (num_to_show < 1)
					num_to_show = 1;
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
	read_fde(fd);
	exit_status = 0;
	if (t_mode) {
		offset = nearest_time(fd, target_time, &stbuf, &evbuf);
	}
	else if (o_mode) {
		offset = nearest_offset(fd, starting_offset, &stbuf, &evbuf, 1);
	}

	if (offset == -2) {
		fprintf(stderr, "Could not find any records\n");
		exit_status = 2;
	}
	if (offset < 0) {
		exit_status = 1;
	}
	else {
		print_event(&evbuf);
		while ((shown < num_to_show) && (offset > 0)) {
			try_free_data(&evbuf);
			offset = nearest_offset(fd, offset+1, &stbuf, &evbuf, 1);
			if (offset > 0) {
				printf("\n");
				print_event(&evbuf);
			}
			++shown;
		}
	}

	close(fd);
	return exit_status;
}
