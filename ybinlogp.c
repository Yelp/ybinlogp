/*
 * ybinlogp: A mysql binary log parser and query tool
 *
 * (C) 2010 Yelp, Inc.
 *
 * This work is licensed under the ISC/OpenBSD License. The full
 * contents of that license can be found under license.txt
 */

#define _XOPEN_SOURCE 600
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#include "ybinlogp.h"

#define MAX_RETRIES	102400  /* how many bytes to seek ahead looking for a record */

#define GET_BIT(x,bit) (!!(x & 1 << (bit-1)))

/* binlog parameters */
#define MIN_TYPE_CODE 0
#define MAX_TYPE_CODE 27
#define MIN_EVENT_LENGTH 19
#define MAX_EVENT_LENGTH 10485760  // Can't see why you'd have events >10MB.
#define MAX_SERVER_ID 4294967295   // 0 <= server_id  <= 2**31
time_t MIN_TIMESTAMP;              // set to the time in the FDE
time_t MAX_TIMESTAMP;              // Set this time(NULL) on startup

uint32_t SLAVE_SERVER_ID;
uint32_t MASTER_SERVER_ID;

int ENFORCE_SERVER_ID = 1;


char* event_types[27] = {
	"UNKNOWN_EVENT",            // 0
	"START_EVENT_V3",           // 1
	"QUERY_EVENT",              // 2
	"STOP_EVENT",               // 3
	"ROTATE_EVENT",             // 4
	"INTVAR_EVENT",             // 5
	"LOAD_EVENT",               // 6
	"SLAVE_EVENT",              // 7
	"CREATE_FILE_EVENT",        // 8
	"APPEND_BLOCK_EVENT",       // 9
	"EXEC_LOAD_EVENT",          // 10
	"DELETE_FILE_EVENT",        // 11
	"NEW_LOAD_EVENT",           // 12
	"RAND_EVENT",               // 13
	"USER_VAR_EVENT",           // 14
	"FORMAT_DESCRIPTION_EVENT", // 15
	"XID_EVENT",                // 16
	"BEGIN_LOAD_QUERY_EVENT",   // 17
	"EXECUTE_LOAD_QUERY_EVENT", // 18
	"TABLE_MAP_EVENT",          // 19
	"PRE_GA_WRITE_ROWS_EVENT",  // 20
	"PRE_GA_DELETE_ROWS_EVENT", // 21
	"WRITE_ROWS_EVENT",         // 22
	"UPDATE_ROWS_EVENT",        // 23
	"DELETE_ROWS_EVENT",        // 24
	"INCIDENT_EVENT",           // 25
	"HEARTBEAT_LOG_EVENT"	    // 26
};

enum e_event_types {
	UNKNOWN_EVENT=0,
	START_EVENT_V3=1,
	QUERY_EVENT=2,
	STOP_EVENT=3,
	ROTATE_EVENT=4,
	INTVAR_EVENT=5,
	LOAD_EVENT=6,
	SLAVE_EVENT=7,
	CREATE_FILE_EVENT=8,
	APPEND_BLOCK_EVENT=9,
	EXEC_LOAD_EVENT=10,
	DELETE_FILE_EVENT=11,
	NEW_LOAD_EVENT=12,
	RAND_EVENT=13,
	USER_VAR_EVENT=14,
	FORMAT_DESCRIPTION_EVENT=15,
	XID_EVENT=16,
	BEGIN_LOAD_QUERY_EVENT=17,
	EXECUTE_LOAD_QUERY_EVENT=18,
	TABLE_MAP_EVENT=19,
	PRE_GA_WRITE_ROWS_EVENT=20,
	PRE_GA_DELETE_ROWS_EVENT=21,
	WRITE_ROWS_EVENT=22,
	UPDATE_ROWS_EVENT=23,
	DELETE_ROWS_EVENT=24,
	INCIDENT_EVENT=25,
	HEARTBEAT_LOG_EVENT=26
};

char* variable_types[10] = {
	"Q_FLAGS2_CODE",               // 0
	"Q_SQL_MODE_CODE",             // 1
	"Q_CATALOG_CODE",              // 2
	"Q_AUTO_INCREMENT",            // 3
	"Q_CHARSET_CODE",              // 4
	"Q_TIME_ZONE_CODE",            // 5
	"Q_CATALOG_NZ_CODE",           // 6
	"Q_LC_TIME_NAMES_CODE",        // 7
	"Q_CHARSET_DATABASE_CODE",     // 8
	"Q_TABLE_MAP_FOR_UPDATE_CODE", // 9
};

char* intvar_types[3] = {
	"",
	"LAST_INSERT_ID_EVENT",	       // 1
	"INSERT_ID_EVENT",             // 2
};

char* flags[16] = {
	"LOG_EVENT_BINLOG_IN_USE",     // 0x01
	"LOG_EVENT_FORCED_ROTATE",     // 0x02 (deprecated)
	"LOG_EVENT_THREAD_SPECIFIC",   // 0x04
	"LOG_EVENT_SUPPRESS_USE",      // 0x08
	"LOG_EVENT_UPDATE_TABLE_MAP_VERSION", // 0x10
	"LOG_EVENT_ARTIFICIAL",        // 0x20
	"LOG_EVENT_RELAY_LOG",         // 0x40
	"",
	"",
};

int q_mode = 0;
int v_mode = 0;
int Q_mode = 0;
struct stat stbuf;
char* database_limit = NULL;

void print_statement_event(struct event *e)
{
	if (e-> data == NULL) {
		return;
	}
	switch ((enum e_event_types)e->type_code) {
		case QUERY_EVENT:
			{
			size_t statement_len = query_event_statement_len(e);
			char* db_name = query_event_db_name(e);
			/* Duplicate the statement because the binlog
			 * doesn't NUL-terminate it. */
			char *statement;
            if ((database_limit != NULL) && (strncmp(db_name, database_limit, strlen(database_limit)) != 0))
                return;
			if ((statement = strndup((const char*)query_event_statement(e), statement_len)) == NULL) {
				perror("strndup");
				return;
			}
			if ((Q_mode <= 1) || (strncmp(statement, "BEGIN", 5) != 0)) {
				printf("%s;\n", statement);
			}
			free(statement);
			break;
			}
		case XID_EVENT:
			{
			if (Q_mode <= 1)
				printf("COMMIT\n");
			break;
			}
		default:
			break;
	}
}

void print_event(struct event *e)
{
	int i;
	const time_t t = e->timestamp;
	if (Q_mode) {
		return print_statement_event(e);
    }
	printf("BYTE OFFSET %llu\n", (long long)e->offset);
	printf("------------------------\n");
	printf("timestamp:          %d = %s", e->timestamp, ctime(&t));
	printf("type_code:          %s\n", event_types[e->type_code]);
	if (q_mode > 1)
		return;
	printf("server id:          %u\n", e->server_id);
	if (v_mode) {
        printf("length:             %d\n", e->length);
		printf("next pos:           %llu\n", (unsigned long long)e->next_position);
    }
	printf("flags:              ");
	for(i=16; i > 0; --i)
	{
		printf("%hhd", GET_BIT(e->flags, i));
	}
	printf("\n");
	for(i=16; i > 0; --i)
    {
		if (GET_BIT(e->flags, i))
            printf("	                    %s\n", flags[i-1]);
	}
	if (e->data == NULL) {
		return;
	}
	switch ((enum e_event_types)e->type_code) {
		case QUERY_EVENT:
			{
			struct query_event *q = (struct query_event*)e->data;
			char* db_name = query_event_db_name(e);
			size_t statement_len = query_event_statement_len(e);
			/* Duplicate the statement because the binlog
			 * doesn't NUL-terminate it. */
			char* statement;
            if ((database_limit != NULL) && (strncmp(db_name, database_limit, strlen(database_limit)) != 0))
                return;
			if ((statement = strndup((const char*)query_event_statement(e), statement_len)) == NULL) {
				perror("strndup");
				return;
			}
			printf("thread id:          %d\n", q->thread_id);
			printf("query time (s):     %d\n", q->query_time);
			if (q->error_code == 0) {
				printf("error code:         %d\n", q->error_code);
			}
			else {
				printf("ERROR CODE:         %d\n", q->error_code);
			}
			printf("status var length:  %d\n", q->status_var_len);
			printf("db_name:            %s\n", db_name);
			printf("statement length:   %zd\n", statement_len);
			if (q_mode == 0)
				printf("statement:          %s\n", statement);
			free(statement);
			}
			break;
		case ROTATE_EVENT:
			{
			struct rotate_event *r = (struct rotate_event*)e->data;
			char *file_name = strndup((const char*)rotate_event_file_name(e), rotate_event_file_name_len(e));
			printf("next log position:  %llu\n", (unsigned long long)r->next_position);
			printf("next file name:     %s\n", file_name);
			free(file_name);
			}
			break;
		case INTVAR_EVENT:
			{
			struct intvar_event *i = (struct intvar_event*)e->data;
			printf("variable type:      %s\n", intvar_types[i->type]);
			printf("value:              %llu\n", (unsigned long long) i->value);
			}
			break;
		case RAND_EVENT:
			{
			struct rand_event *r = (struct rand_event*)e->data;
			printf("seed 1:             %llu\n", (unsigned long long) r->seed_1);
			printf("seed 2:             %llu\n", (unsigned long long) r->seed_2);
			}
			break;
		case FORMAT_DESCRIPTION_EVENT:
			{
			struct format_description_event *f = (struct format_description_event*)e->data;
			printf("binlog version:     %d\n", f->format_version);
			printf("server version:     %s\n", f->server_version);
			printf("variable length:    %d\n", format_description_event_data_len(e));
			}
			break;
		case XID_EVENT:
			{
			struct xid_event *x = (struct xid_event*)e->data;
			printf("xid id:             %llu\n", (unsigned long long)x->id);
			}
			break;
		default:
			break;
	}
}

void usage(void)
{
	fprintf(stderr, "Usage: ybinlogp [mode] logfile\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "ybinlogp supports several different modes:\n");
	fprintf(stderr, "\t-o OFFSET\t\tFind the first event after the given offset\n");
	fprintf(stderr, "\t\t\t\tybinlogp -o offset logfile\n");
	fprintf(stderr, "\t-t TIMESTAMP\t\tFind the event closest to the given unix time\n");
	fprintf(stderr, "\t\t\t\tybinlogp -t timestamp logfile\n");
	fprintf(stderr, "\t-a COUNT\t\tWhen used with one of the above, print COUNT items after the first one\n");
	fprintf(stderr, "\t\t\t\tAccepts either an integer or the text 'all'\n");
	fprintf(stderr, "\t\t\t\tybinlogp -a N -t timestamp logfile\n");
	fprintf(stderr, "\t-q\t\t\tBe quieter (may be specified more than once)\n");
	fprintf(stderr, "\t-Q\t\t\tOnly print QUERY_EVENTS, and only print the statement\n");
	fprintf(stderr, "\t\t\t\tIf passed twice, do not print transaction events\n");
	fprintf(stderr, "\t-D DBNAME\t\tFilter query events that were not in DBNAME\n");
    fprintf(stderr, "\t\t\t\tNote that this still shows transaction control events\n");
    fprintf(stderr, "\t\t\t\tsince those do not have an associated database. Mea culpa.\n");
	fprintf(stderr, "\t-v\t\t\tBe more verbose (may be specified more than once)\n");
	fprintf(stderr, "\t-S\t\t\tRemove server-id checks (by default, will only allow 1 slave and 1 master server-id in a log file\n");
    fprintf(stderr, "\t-h\t\t\tShow this help\n");
}

int check_event(struct event *e)
{
	if ((!ENFORCE_SERVER_ID || (e->server_id == SLAVE_SERVER_ID ||
			e->server_id == MASTER_SERVER_ID)) &&
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

int read_event(int fd, struct event *evbuf, off64_t offset)
{
	ssize_t amt_read;
	if ((lseek(fd, offset, SEEK_SET) < 0)) {
		perror("Error seeking");
		return -1;
	}
	amt_read = read(fd, (void*)evbuf, EVENT_HEADER_SIZE);
	evbuf->offset = offset;
	evbuf->data = NULL;
	if (amt_read < 0) {
		fprintf(stderr, "Error reading event at %lld: %s\n", (long long) offset, strerror(errno));
		return -1;
	} else if ((size_t)amt_read != EVENT_HEADER_SIZE) {
		return -1;
	}
	if (check_event(evbuf)) {
#if DEBUG
		fprintf(stdout, "mallocing %d bytes\n", evbuf->length - EVENT_HEADER_SIZE);
#endif
		if ((evbuf->data = malloc(evbuf->length - EVENT_HEADER_SIZE)) == NULL) {
			perror("malloc:");
			return -1;
		}
#if DEBUG
		fprintf(stderr, "malloced %d bytes at 0x%p for a %s\n", evbuf->length - EVENT_HEADER_SIZE, evbuf->data, event_types[evbuf->type_code]);
#endif
		if (read(fd, evbuf->data, evbuf->length - EVENT_HEADER_SIZE) < 0) {
			perror("reading extra data:");
			return -1;
		}
	}
	return 0;
}

void init_event(struct event *evbuf)
{
	memset(evbuf, 0, sizeof(struct event));
}

void dispose_event(struct event *evbuf)
{
#if DEBUG
	fprintf(stderr, "About to dispose_event 0x%p\n", evbuf);
#endif
	if (evbuf->data != 0) {
		free(evbuf->data);
		evbuf->data = 0;
	}
	free(evbuf);
}

void reset_event(struct event *evbuf)
{
#if DEBUG
	fprintf(stderr, "Resetting event\n");
#endif
	if (evbuf->data != 0) {
		free(evbuf->data);
		evbuf->data = 0;
	}
	init_event(evbuf);
}

int copy_event(struct event *dest, struct event *source)
{
#if DEBUG
	fprintf(stderr, "About to copy 0x%p to 0x%p\n", source, dest);
#endif
	memmove(dest, source, sizeof(struct event));
	if (source->data != 0) {
#if DEBUG
		fprintf(stderr, "mallocing %d bytes for the target\n", source->length - EVENT_HEADER_SIZE);
#endif
		if ((dest->data = malloc(source->length - EVENT_HEADER_SIZE)) == NULL) {
			perror("malloc:");
			return -1;
		}
#if DEBUG
		fprintf(stderr, "copying extra data from 0x%p to 0x%p\n", source->data, dest->data);
#endif
		memmove(dest->data, source->data, source->length - EVENT_HEADER_SIZE);
	}
	return 0;
}

/**
 * Get the event after the given event (using built-in chaining)
 **/
off64_t next_after(struct event *evbuf)
{
	/* Can't actually use next_position, because it will vary between
	 * messages that are from master and messages that are from slave.
	 * Usually, only the FDE is from the slave. But, still...
	 */
	return evbuf->offset + evbuf->length;
}

/*
 * Get the first event after starting_offset in fd
 *
 * If evbuf is non-null, copy it into there
 */
off64_t nearest_offset(int fd, off64_t starting_offset, struct event *outbuf, int direction)
{
	unsigned int num_increments = 0;
	off64_t offset;
	struct event *evbuf = malloc(sizeof(struct event));
	init_event(evbuf);
	offset = starting_offset;
#if DEBUG
	fprintf(stderr, "In nearest offset mode, got fd=%d, starting_offset=%llu\n", fd, (long long)starting_offset);
#endif
	while (num_increments < MAX_RETRIES && offset >= 0 && offset <= stbuf.st_size - 19) 
	{
		reset_event(evbuf);
		if (read_event(fd, evbuf, offset) < 0) {
			dispose_event(evbuf);
			return -1;
		}
		if (check_event(evbuf)) {
			if (outbuf != NULL) {
				copy_event(outbuf, evbuf);
			}
			dispose_event(evbuf);
			return offset;
		} else {
			offset += direction;
			++num_increments;
		}
	}
	dispose_event(evbuf);
#if DEBUG
	fprintf(stderr, "Unable to find anything (offset=%llu)\n",(long long) offset);
#endif
	return -2;
}


/**
 * Binary-search to find the record closest to the requested time
 **/
int nearest_time(int fd, time_t target, struct event *outbuf)
{
	off64_t file_size = stbuf.st_size;
	struct event *evbuf = malloc(sizeof(struct event));
	init_event(evbuf);
	off64_t offset = file_size / 2;
	off64_t next_increment = file_size / 4;
	int directionality = 1;
	off64_t found, last_found = 0;
	while (next_increment > 2) {
		long long delta;
		reset_event(evbuf);
		found = nearest_offset(fd, offset, evbuf, directionality);
		if (found == -1) {
			return found;
		}
		else if (found == -2) {
			fprintf(stderr, "Ran off the end of the file, probably going to have a bad match\n");
			last_found = found;
			break;
		}
		last_found = found;
		delta = (evbuf->timestamp - target);
		if (delta > 0) {
			directionality = -1;
		}
		else if (delta < 0) {
			directionality = 1;
		}
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
	if (outbuf)
		copy_event(outbuf, evbuf);
	dispose_event(evbuf);
	return last_found;
}

/**
 * Read the FDE and set the server-id
 **/
int read_fde(int fd)
{
	struct event* evbuf = malloc(sizeof(struct event));
	off64_t offset;
	int old_esi = ENFORCE_SERVER_ID;
	init_event(evbuf);

	ENFORCE_SERVER_ID = 0;
	if (read_event(fd, evbuf, 4) < 0) {
		return -1;
	}
	ENFORCE_SERVER_ID = old_esi;
#if DEBUG
	print_event(evbuf);
#endif
	struct format_description_event *f = (struct format_description_event*) evbuf->data;
	if (f->format_version != BINLOG_VERSION) {
		fprintf(stderr, "Invalid binlog! Expected version %d, got %d\n", BINLOG_VERSION, f->format_version);
		exit(1);
	}
	MIN_TIMESTAMP = evbuf->timestamp;
	SLAVE_SERVER_ID = evbuf->server_id;

	offset = next_after(evbuf);
	reset_event(evbuf);
	read_event(fd, evbuf, offset);

	MASTER_SERVER_ID = evbuf->server_id;
	dispose_event(evbuf);

	lseek(fd, 0, SEEK_SET);
	return 0;
}

int main(int argc, char **argv)
{
	int fd, exit_status;
	struct event *evbuf = malloc(sizeof(struct event));
	init_event(evbuf);
	int opt;
	off64_t offset = 4;
	int shown = 1;
	
	time_t target_time = time(NULL);
	off64_t starting_offset = 0;
	int show_all = 0;
	int num_to_show = 1;
	int t_mode = 0;
	int o_mode = 1;

	MAX_TIMESTAMP = time(NULL);

	/* Parse args */
	while ((opt = getopt(argc, argv, "ht:o:a:qQvD:S")) != -1) {
		switch (opt) {
			case 't':      /* Time mode */
				target_time = atol(optarg);
				t_mode = 1;
				o_mode = 0;
				break;
            case 'o':      /* Offset mode */
				starting_offset = atoll(optarg);
				t_mode  = 0;
				o_mode = 1;
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
			case 'v':
				v_mode++;
				break;
			case 'q':
				q_mode++;
				break;
			case 'Q':
				Q_mode++;
				break;
			case 'S':
				ENFORCE_SERVER_ID = 0;
				break;
            case 'h':
                usage();
                return 0;
			case '?':
				fprintf(stderr, "Unknown argument %c\n", optopt);
				usage();
				free(evbuf);
				return 1;
				break;
			case 'D':
				database_limit = strdup(optarg);
				break;
			default:
				usage();
				free(evbuf);
				return 1;
		}
	}
	if (v_mode && q_mode) {
		fprintf(stderr, "-v and -q/-Q may not be specified\n");
		return 2;
	}
	if (optind >= argc) {
		usage();
		return 2;
	}
	/* First argument is always filename */
	if (stat(argv[optind], &stbuf)) {
		perror("Error stat-ing file");
		return 1;
	}
#if DEBUG
	fprintf(stderr, "Opening file %s\n", argv[optind]);
#endif
	if ((fd = open(argv[optind], O_RDONLY|O_LARGEFILE)) <= 0) {
		perror("Error opening file");
		return 1;
	}
	read_fde(fd);
	exit_status = 0;
	if (t_mode) {
		offset = nearest_time(fd, target_time, evbuf);
	}
	else if (o_mode) {
		offset = nearest_offset(fd, starting_offset, evbuf, 1);
	}

	if (offset == -2) {
		fprintf(stderr, "Could not find any records\n");
		exit_status = 2;
	}
	if (offset < 0) {
		exit_status = 1;
	}
	else {
		print_event(evbuf);
		while ((shown < num_to_show) && (offset > 0) &&
				(evbuf->next_position != evbuf->offset) &&
				(evbuf->next_position < stbuf.st_size)) {
			/* 
			 * When we're in a_mode, just use the built-in
			 * chaining to be BLAZINGLY FASTish
			 */
			offset = next_after(evbuf);
			reset_event(evbuf);
			read_event(fd, evbuf, offset);
			if (offset > 0) {
				if (!Q_mode)
					printf("\n");
				print_event(evbuf);
			}
			if (!show_all)
				++shown;
		}
	}

	close(fd);
	dispose_event(evbuf);
	return exit_status;
}
