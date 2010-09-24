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

#define MIN_TIMESTAMP 1262325600	/* 2010-01-01 00:00:00 */
time_t MAX_TIMESTAMP;
#define MAX_RETRIES	409600			/* how many bytes to seek ahead looking for a record */

#define GET_BIT(x,bit) (!!(x & 1 << bit))

#define HEADER_SIZE 19

/* binlog parameters */
#define MIN_TYPE_CODE 0
#define MAX_TYPE_CODE 27
#define MIN_EVENT_LENGTH 91
#define MAX_EVENT_LENGTH 1073741824


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

#pragma pack(push)
#pragma pack(1)			/* force byte alignment */
struct event {
	uint32_t	timestamp;
	uint8_t		type_code;
	uint32_t	server_id;
	uint32_t	event_length;
	uint32_t	next_position;
	uint16_t	flags;
	char *data;
	off_t		offset;
};						/* 17 bytes */
#pragma pack(pop)

void print_event(struct event *e) {
	char datebuf[20];
	time_t t = e->timestamp;
	struct tm *timebuf;
	timebuf = localtime(&t);
	datebuf[19] = '\0';
	strftime(datebuf, 20, "%Y-%m-%d %H:%M:%S", timebuf);
	printf("BYTE OFFSET:        %llu\n", (long long)e->offset);
	printf("timestamp:          %s\n", datebuf);
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

inline int check_event(struct event *e)
{
	if (e->type_code > MIN_TYPE_CODE &&
			e->type_code < MAX_TYPE_CODE &&
			e->event_length > MIN_EVENT_LENGTH &&
			e->event_length < MAX_EVENT_LENGTH && 
			e->timestamp > MIN_TIMESTAMP &&
			e->timestamp < MAX_TIMESTAMP) {
		return 1;
	} else {
		return 0;
	}
}

char* read_extra_data(int fd, off_t seek_pos, size_t amount)
{
	char* data;
	data = malloc(amount);
#if DEBUG
	fprintf(stderr, "Reading %zd bytes from %llu\n", amount, (long long)seek_pos);
#endif
	data = malloc(amount);
	lseek(fd, SEEK_SET, seek_pos);
	read(fd, data, amount);
	return data;
}

int main(int argc, char **argv)
{
	int fd;
	unsigned int num_increments = 0;
	off_t offset;
	off_t seeked;
	ssize_t amt_read;
	struct stat stbuf;
	struct event evbuf;
	if (argc != 3) {
		usage();
		return 1;
	}
	if (stat(argv[1], &stbuf)) {
		perror("Error stat-ing file");
		return 1;
	}
	MAX_TIMESTAMP = time(NULL);
	offset = atoll(argv[2]);
	fd = open(argv[1], O_RDONLY);
	while (num_increments < MAX_RETRIES) 
	{
		if (stbuf.st_size < offset) {
			fprintf(stderr, "%s is only %lld bytes, requested %lld\n", argv[1], (long long)stbuf.st_size, (long long)offset);
			return 1;
		}
#ifdef DEBUG
		fprintf(stderr, "Seeking to %lld\n", (long long)offset);
#endif
		seeked = lseek(fd, offset, SEEK_SET);
		if (seeked != offset) {
			fprintf(stderr, "Error seeking to offset %lld\n", (long long) offset);
			return 1;
		}
		amt_read = read(fd, (void*)&evbuf, HEADER_SIZE);
		evbuf.offset = offset;
		evbuf.data = 0;
		if (amt_read < 0) {
			fprintf(stderr, "Error reading event at %lld: %s\n", (long long) offset, strerror(errno));
			return 1;
		} else if ((size_t)amt_read != HEADER_SIZE) {
			fprintf(stderr, "Only able to read %zd bytes, good bye\n", amt_read);
			return 1;
		}
		/* Maybe read some extra data */
		switch (evbuf.type_code) {
			case 2:
				evbuf.data = read_extra_data(fd, offset + HEADER_SIZE, 8);
				break;
			case 15:
				evbuf.data = read_extra_data(fd, offset + HEADER_SIZE, 52);
				break;
		}
		if (check_event(&evbuf)) {
			print_event(&evbuf);
			return 0;
		} else {
			++offset;
			++num_increments;
		}
		if (evbuf.data != 0) {
			free(evbuf.data);
		}
	}
	close(fd);
	return 0;
}
