/*
 * ybinlogp: A mysql binary log parser and query tool
 *
 * (C) 2010-2011 Yelp, Inc.
 *
 * This work is licensed under the ISC/OpenBSD License. The full
 * contents of that license can be found under license.txt
 */

/* Conventions used in this file:
 *
 * Functions starting with ybp_ will be in the .h file and will be exported
 * Functions starting with ybpi_ are internal-only and should be static
 */

#define _XOPEN_SOURCE 600
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#include "debugs.h"
#include "ybinlogp.h"

/******* binlog parameters ********/
#define MIN_TYPE_CODE 0
#define MAX_TYPE_CODE 27
#define MIN_EVENT_LENGTH 19
#define MAX_EVENT_LENGTH 16*1048576	/* Max statement len is generally 16MB */
#define MAX_SERVER_ID 4294967295	   /* 0 <= server_id  <= 2**32 */
#define TIMESTAMP_FUDGE_FACTOR 3600	  /* seconds */

/******* more defines ********/
#define MAX_RETRIES	16*1048576  /* how many bytes to seek ahead looking for a record */

#define GET_BIT(x,bit) (unsigned char)(!!(x & 1 << (bit-1)))

#define min(x,y) (((x) < (y)) ? (x) : (y))

/* Pulls in a bunch of strings and things that I don't really want in this
 * file, but are only to be used here.
 */
#include "ybinlogp-private.h"

/******* predeclarations of ybpi functions *******/
static int ybpi_read_fde(struct ybp_binlog_parser* restrict);
static int ybpi_read_event(struct ybp_binlog_parser* restrict, off_t, struct ybp_event* restrict);
static bool ybpi_check_event(struct ybp_event*, struct ybp_binlog_parser*);
static off64_t ybpi_next_after(struct ybp_event* restrict);
static off64_t ybpi_nearest_offset(struct ybp_binlog_parser* restrict, off64_t, struct ybp_event* restrict, int);

/******** implementation begins here ********/

struct ybp_binlog_parser* ybp_get_binlog_parser(int fd)
{
	struct ybp_binlog_parser* result;
	if ((result = malloc(sizeof(struct ybp_binlog_parser))) == NULL) {
		return NULL;
	}
	result->fd = fd;
	result->offset = 4;
	result->enforce_server_id = false;
	result->slave_server_id = 0;
	result->master_server_id = 0;
	result->min_timestamp = 0;
	result->max_timestamp = time(NULL) + TIMESTAMP_FUDGE_FACTOR;
	result->has_read_fde = false;
	ybp_update_bp(result);
	ybpi_read_fde(result);
	return result;
}

void ybp_rewind_bp(struct ybp_binlog_parser* p, off_t offset)
{
	p->offset = offset;
}

off64_t ybp_tell_bp(struct ybp_binlog_parser* p)
{
	return p->offset;
}

void ybp_update_bp(struct ybp_binlog_parser* p)
{
	struct stat stbuf;
	fstat(p->fd, &stbuf);
	p->file_size = stbuf.st_size;
}

void ybp_dispose_binlog_parser(struct ybp_binlog_parser* p)
{
	if (p != NULL)
		free(p);
}

void ybp_init_event(struct ybp_event* evbuf)
{
	memset(evbuf, 0, sizeof(struct ybp_event));
}

struct ybp_event* ybp_get_event(void)
{
	struct ybp_event* event = malloc(sizeof(struct ybp_event));
	Dprintf("Creating event at 0x%p\n", (void*)event);
	ybp_init_event(event);
	return event;
}

void ybp_dispose_event(struct ybp_event* evbuf)
{
	Dprintf("About to dispose_event 0x%p\n", (void*)evbuf);
	if (evbuf->data != NULL) {
		Dprintf("Freeing data at 0x%p\n", (void*)evbuf->data);
		free(evbuf->data);
		evbuf->data = NULL;
	}
	free(evbuf);
}

int ybp_copy_event(struct ybp_event *dest, struct ybp_event *source)
{
	Dprintf("About to copy 0x%p to 0x%p\n", (void*)source, (void*)dest);
	memmove(dest, source, sizeof(struct ybp_event));
	if (source->data != 0) {
		Dprintf("mallocing %d bytes for the target\n", source->length - EVENT_HEADER_SIZE);
		if ((dest->data = malloc(source->length - EVENT_HEADER_SIZE)) == NULL) {
			perror("malloc:");
			return -1;
		}
		Dprintf("copying extra data from 0x%p to 0x%p\n", source->data, dest->data);
		memmove(dest->data, source->data, source->length - EVENT_HEADER_SIZE);
	}
	return 0;
}

void ybp_reset_event(struct ybp_event* evbuf)
{
	Dprintf("Resetting event\n");
	if (evbuf->data != 0) {
		Dprintf("Freeing data at 0x%p\n", (void*)evbuf->data);
		free(evbuf->data);
		evbuf->data = 0;
	}
	ybp_init_event(evbuf);
}

/**
 * check if an event "looks" valid. returns true if it does and false if it
 * doesn't
 **/
static bool ybpi_check_event(struct ybp_event* e, struct ybp_binlog_parser* p)
{
	Dprintf("e->type_code = %d, e->length=%zd, e->timestamp=%d\n",
				e->type_code,
				e->length,
				e->timestamp);
	Dprintf("p->min_timestamp = %d, p->max_timestamp = %d\n",
				p->min_timestamp,
				p->max_timestamp);

	return ((!p->enforce_server_id ||
				 (e->server_id == p->slave_server_id) ||
				 (e->server_id == p->master_server_id)) &&
			e->type_code > MIN_TYPE_CODE &&
			e->type_code < MAX_TYPE_CODE &&
			e->length >= MIN_EVENT_LENGTH &&
			e->length < MAX_EVENT_LENGTH);
}

/**
 * Find the offset of the next event after the one passed in.
 * Uses the built-in event chaining.
 **/
static off64_t ybpi_next_after(struct ybp_event *evbuf) {
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
off64_t ybp_nearest_offset(struct ybp_binlog_parser* p, off64_t starting_offset)
{
	return ybpi_nearest_offset(p, starting_offset, NULL, 1);
}

off64_t ybpi_nearest_offset(struct ybp_binlog_parser* restrict p, off64_t starting_offset, struct ybp_event* restrict outbuf, int direction)
{
	unsigned int num_increments = 0;
	off64_t offset;
	struct ybp_event *evbuf = ybp_get_event();
	offset = starting_offset;
	Dprintf("In nearest offset mode, got fd=%d, starting_offset=%llu, direction=%d\n", p->fd, (long long)starting_offset, direction);
	while ((num_increments < MAX_RETRIES) && (offset >= 0) && (offset <= p->file_size - EVENT_HEADER_SIZE) )
	{
		ybp_reset_event(evbuf);
		if (ybpi_read_event(p, offset, evbuf) < 0) {
			ybp_dispose_event(evbuf);
			return -1;
		}
		if (ybpi_check_event(evbuf, p)) {
			if (outbuf != NULL)
				ybp_copy_event(outbuf, evbuf);
			ybp_dispose_event(evbuf);
			return offset;
		}
		else {
			Dprintf("incrementing offset from %zd to %zd\n", offset, offset + direction);
			offset += direction;
			++num_increments;
		}
	}
	ybp_dispose_event(evbuf);
	Dprintf("Unable to find anything (offset=%llu)\n",(long long) offset);
	return -2;
}

/**
 * Binary-search to find the record closest to the requested time
 **/
off64_t ybp_nearest_time(struct ybp_binlog_parser* restrict p, time_t target)
{
	off64_t file_size = p->file_size;
	struct ybp_event *evbuf = ybp_get_event();
	off64_t offset = file_size / 2;
	off64_t next_increment = file_size / 4;
	int directionality = 1;
	off64_t found, last_found = 0;
	while (next_increment > 2) {
		long long delta;
		ybp_reset_event(evbuf);
		found = ybpi_nearest_offset(p, offset, evbuf, directionality);
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
		Dprintf("delta=%lld at %llu, directionality=%d, next_increment=%lld\n", (long long)delta, (unsigned long long)found, directionality, (long long)next_increment);
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
	ybp_dispose_event(evbuf);
	return last_found;
}


/**
 * Read an event from the parser parser, at offset offet, storing it in
 * event evbuf (which should be already init'd)
 */
static int ybpi_read_event(struct ybp_binlog_parser* restrict p, off_t offset, struct ybp_event* restrict evbuf)
{
	ssize_t amt_read;
	Dprintf("Reading event at offset %zd\n", offset);
	p->max_timestamp = time(NULL) + TIMESTAMP_FUDGE_FACTOR;
	if ((lseek(p->fd, offset, SEEK_SET) < 0)) {
		perror("Error seeking");
		return -1;
	}
	amt_read = read(p->fd, (void*)evbuf, EVENT_HEADER_SIZE);
	evbuf->offset = offset;
	evbuf->data = NULL;
	if (amt_read < 0) {
		fprintf(stderr, "Error reading event at %lld: %s\n", (long long) offset, strerror(errno));
		return -1;
	} else if ((size_t)amt_read != EVENT_HEADER_SIZE) {
		Dprintf("read %zd bytes, expected to read %d bytes in ybpi_read_event", amt_read, EVENT_HEADER_SIZE);
		return -1;
	}
	if (evbuf->length + evbuf->offset > p->file_size) {
		return -2;
	}
	if (ybpi_check_event(evbuf, p)) {
		Dprintf("mallocing %d bytes\n", evbuf->length - EVENT_HEADER_SIZE);
		if ((evbuf->data = malloc(evbuf->length - EVENT_HEADER_SIZE)) == NULL) {
			perror("malloc:");
			return -1;
		}
		amt_read = 0;
		Dprintf("malloced %d bytes at 0x%p for a %s\n", evbuf->length - EVENT_HEADER_SIZE, evbuf->data, ybpi_event_types[evbuf->type_code]);
		while (amt_read < evbuf->length - EVENT_HEADER_SIZE) {
			ssize_t remaining = evbuf->length - EVENT_HEADER_SIZE - amt_read;
			char* target = evbuf->data + amt_read;
			ssize_t read_this_time = read(p->fd, target, remaining);
			if (read_this_time < 0) {
				perror("read extra data");
				free(evbuf->data);
				return -1;
			}
			amt_read += read_this_time;
		}
	}
	else {
		Dprintf("check_event failed\n");
	}
	return 0;
}

/**
 * Read the FDE. It's the first record in ALL binlogs
 **/
static int ybpi_read_fde(struct ybp_binlog_parser* p)
{
	struct ybp_event* evbuf;
	off64_t offset;
	bool esi = p->enforce_server_id;
	int fd = p->fd;
	time_t fde_time;
	time_t evt_time;

	p->enforce_server_id = false;

	if ((evbuf = ybp_get_event()) == NULL) {
		return -1;
	}

	if (ybpi_read_event(p, 4, evbuf) < 0) {
		Dprintf("Reading FDE failed\n");
		ybp_dispose_event(evbuf);
		return -1;
	}
	p->enforce_server_id = esi;
	struct ybp_format_description_event *f = ybp_event_as_fde(evbuf);
	Dprintf("passed in evbuf 0x%p, got back fde 0x%p\n", (void*)evbuf, (void*)f);
	if (f->format_version != BINLOG_VERSION) {
		fprintf(stderr, "Invalid binlog! Expected version %d, got %d\n", BINLOG_VERSION, f->format_version);
		exit(1);
	}
	fde_time = evbuf->timestamp;
	p->slave_server_id = evbuf->server_id;

	offset = ybpi_next_after(evbuf);
	p->offset = offset;
	ybp_reset_event(evbuf);
	ybpi_read_event(p, offset, evbuf);

	p->master_server_id = evbuf->server_id;
	evt_time = evbuf->timestamp;
	ybp_dispose_event(evbuf);

	/*
	 * Another signal: events will all be after *either* the FDE time (which
	 * is the start of this server writing this binlog) or the first event
	 * time (which is the start of the master writing its binlog), unless
	 * you're in multi-master, which we don't particularly support.
	 */
	p->min_timestamp = min(fde_time, evt_time) - TIMESTAMP_FUDGE_FACTOR;

	lseek(fd, 4, SEEK_SET);
	Dprintf("Done reading FDE\n");
	p->has_read_fde = true;
	return 0;
}

int ybp_next_event(struct ybp_binlog_parser* restrict parser, struct ybp_event* restrict evbuf)
{
	int ret = 0;
	bool esi = parser->enforce_server_id;
	Dprintf("looking for next event, offset=%zd\n", parser->offset);
	if (!parser->has_read_fde) {
		ybpi_read_fde(parser);
	}
	parser->enforce_server_id = false;
	ret = ybpi_read_event(parser, parser->offset, evbuf);
	parser->enforce_server_id = esi;
	if (ret < 0) {
		Dprintf("error in ybp_next_event: %d\n", ret);
		return ret;
	} else {
		parser->offset = ybpi_next_after(evbuf);
		if ((parser->offset <= 0) || (evbuf->next_position == evbuf->offset) ||
			(evbuf->next_position >= parser->file_size) ||
			(parser->offset >= parser->file_size)) {
			Dprintf("Got to last event, parser->offset=%zd, evbuf->next_position=%u, parser->file_size=%zd\n",
					parser->offset,
					evbuf->next_position,
					parser->file_size);
			return 0;
		} else {
			return 1;
		}
	}
}

struct ybp_format_description_event* ybp_event_as_fde(struct ybp_event* restrict e)
{
	if (e->type_code != FORMAT_DESCRIPTION_EVENT) {
		fprintf(stderr, "Illegal conversion attempted: %d -> %d\n", e->type_code, FORMAT_DESCRIPTION_EVENT);
		return NULL;
	}
	else {
		return (struct ybp_format_description_event*)(e->data);
	}
}

struct ybp_query_event* ybp_event_as_qe(struct ybp_event* restrict e)
{
	if (e->type_code != QUERY_EVENT) {
		fprintf(stderr, "Illegal conversion attempted: %d -> %d\n", e->type_code, QUERY_EVENT);
		return NULL;
	} else {
		return (struct ybp_query_event*)(e->data);
	}
}

struct ybp_query_event_safe* ybp_event_to_safe_qe(struct ybp_event* restrict e) {
	struct ybp_query_event_safe* s;
	if (e->type_code != QUERY_EVENT) {
		fprintf(stderr, "Illegal conversion attempted: %d -> %d\n", e->type_code, QUERY_EVENT);
		return NULL;
	} else {
		assert(e->data != NULL);
		struct ybp_query_event* qe = (struct ybp_query_event*)(e->data);
		Dprintf("Constructing safe query event for 0x%p\n", (void*) e);
		s = malloc(sizeof(struct ybp_query_event_safe));
		if (s == NULL)
			return NULL;
		Dprintf("malloced 0x%p\n", (void*)s);
		s->thread_id = qe->thread_id;
		s->query_time = qe->query_time;
		s->db_name_len = qe->db_name_len;
		Dprintf("qe->db_name_len = %d\n", qe->db_name_len);
		s->error_code = qe->error_code;
		s->status_var_len = qe->status_var_len;
		if (s == NULL) {
			perror("malloc");
			return NULL;
		}
		s->statement_len = query_event_statement_len(e);
		s->statement = strndup((const char*)query_event_statement(e), s->statement_len);
		Dprintf("s->statement_len = %zd\n", s->statement_len);
		Dprintf("s->statement = %s\n", s->statement);
		if (s->statement == NULL) {
			perror("strndup");
			return NULL;
		}
		s->db_name = strndup((char*)query_event_db_name(e), s->db_name_len);
		s->status_var = strndup((char*)query_event_status_vars(e), s->status_var_len);
	}
	Dprintf("Returning s\n");
	return s;
}

void ybp_dispose_safe_qe(struct ybp_query_event_safe* s)
{
	if (s == NULL) {
		return;
	}
	if (s->statement != NULL)
		free(s->statement);
	if (s->db_name != NULL)
		free(s->db_name);
	if (s->status_var != NULL)
		free(s->status_var);
	free(s);
}

void ybp_dispose_safe_re(struct ybp_rotate_event_safe* s)
{
	if (s == NULL) {
		return;
	}
	if (s->file_name != NULL)
		free(s->file_name);
	free(s);
}

struct ybp_rotate_event_safe* ybp_event_to_safe_re(struct ybp_event* restrict e) {
	struct ybp_rotate_event_safe* s;
	if (e->type_code != ROTATE_EVENT) {
		fprintf(stderr, "Illegal conversion attempted: %d -> %d\n", e->type_code, ROTATE_EVENT);
	} else {
		assert(e->data != NULL);
		struct ybp_rotate_event* re = (struct ybp_rotate_event*)(e->data);
		s = malloc(sizeof(struct ybp_rotate_event_safe));
		s->next_position = re->next_position;
		s->file_name_len = rotate_event_file_name_len(e);
		s->file_name = strndup((char*)rotate_event_file_name(e), s->file_name_len);
	}
	return s;
}

struct ybp_xid_event* ybp_event_to_safe_xe(struct ybp_event* restrict e) {
	struct ybp_xid_event* s;
	if (e->type_code != XID_EVENT) {
		fprintf(stderr, "Illegal conversion attempted: %d -> %d\n", e->type_code, ROTATE_EVENT);
	} else {
		assert(e->data != NULL);
		struct ybp_xid_event* xe = (struct ybp_xid_event*)(e->data);
		s = malloc(sizeof(struct ybp_xid_event));
		if (s == NULL)
			return NULL;
		memcpy(s, xe, sizeof(struct ybp_xid_event));
	}
	return s;
}

void ybp_dispose_safe_xe(struct ybp_xid_event* xe)
{
	free(xe);
}

const char* ybp_event_type(struct ybp_event* restrict evbuf) {
	Dprintf("Looking up type string for %d\n", evbuf->type_code);
	return ybpi_event_types[evbuf->type_code];
}

void ybp_print_event_simple(struct ybp_event* restrict e,
		struct ybp_binlog_parser* restrict p,
		FILE* restrict stream)
{
	ybp_print_event(e, p, stream, 0, 0, NULL);
}

void ybp_print_event(struct ybp_event* restrict e,
		struct ybp_binlog_parser* restrict p,
		FILE* restrict stream,
		bool q_mode,
		bool v_mode,
		char* database_limit)
{
	(void) p;
	int i;
	const time_t t = e->timestamp;
	if (stream == NULL) {
		stream = stdout;
	}
	/* TODO: implement abbreviated parsing mode
	if (p->Q_mode) {
		print_statement_event(e);
		return;
	}
	*/
	fprintf(stream, "BYTE OFFSET %llu\n", (long long)e->offset);
	fprintf(stream, "------------------------\n");
	fprintf(stream, "timestamp:    		 %d = %s", e->timestamp, ctime(&t));
	fprintf(stream, "type_code:    		 %s\n", ybpi_event_types[e->type_code]);
	if (q_mode > 1)
		return;
	fprintf(stream, "server id:          %u\n", e->server_id);
	if (v_mode) {
		fprintf(stream, "length:              %d\n", e->length);
		fprintf(stream, "next pos:            %llu\n", (unsigned long long)e->next_position);
	}
	fprintf(stream, "flags:              ");
	for(i=16; i > 0; --i)
	{
		fprintf(stream, "%hhd", GET_BIT(e->flags, i));
	}
	fprintf(stream, "\n");
	for(i=16; i > 0; --i)
	{
		if (GET_BIT(e->flags, i))
			fprintf(stream, "                        %s\n", ybpi_flags[i-1]);
	}
	if (e->data == NULL) {
		return;
	}
	switch ((enum ybp_event_types)e->type_code) {
		case QUERY_EVENT:
			{
			struct ybp_query_event* q = ybp_event_as_qe(e);
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
			fprintf(stream, "thread id:          %d\n", q->thread_id);
			fprintf(stream, "query time (s):     %d\n", q->query_time);
			if (q->error_code == 0) {
				fprintf(stream, "error code:         %d\n", q->error_code);
			}
			else {
				fprintf(stream, "ERROR CODE:         %d\n", q->error_code);
			}
			fprintf(stream, "status var length:  %d\n", q->status_var_len);
			fprintf(stream, "db_name:            %s\n", db_name);
			if (v_mode) {
				fprintf(stream, "status var length:  %d\n", q->status_var_len);
			}
			if (q->status_var_len > 0) {
				char* status_var_start = query_event_status_vars(e);
				char* status_var_ptr = status_var_start;
				while((status_var_ptr - status_var_start) < q->status_var_len) {
					enum ybpi_e_status_var_types status_var_type = *status_var_ptr;
					status_var_ptr++;
					assert(status_var_type < 10);
					switch (status_var_type) {
						case Q_FLAGS2_CODE:
							{
							uint32_t val = *((uint32_t*)status_var_ptr);
							status_var_ptr += 4;
							fprintf(stream, "Q_FLAGS2:           ");
							for(i=32; i > 0; --i)
							{
								fprintf(stream, "%hhd", GET_BIT(val, i));
							}
							fprintf(stream, "\n");
							for(i=32; i > 0; --i)
							{
								if (GET_BIT(val, i))
									fprintf(stream, "                   %s\n", ybpi_flags2[i-1]);
							}
							break;
							}
						case Q_SQL_MODE_CODE:
							{
							uint64_t val = *((uint64_t*)status_var_ptr);
							status_var_ptr += 8;
							fprintf(stream, "Q_SQL_MODE:         0x%0llu\n", (unsigned long long)val);
							break;
							}
						case Q_CATALOG_CODE:
							{
							uint8_t size = *(status_var_ptr++);
							char* str = strndup(status_var_ptr, size+1);
							status_var_ptr += size + 1;
							fprintf(stream, "Q_CATALOG:          %s\n", str);
							free(str);
							break;
							}
						case Q_AUTO_INCREMENT:
							{
							uint16_t byte_1 = *(uint16_t*)status_var_ptr;
							status_var_ptr += 2;
							uint16_t byte_2 = *(uint16_t*)status_var_ptr;
							status_var_ptr += 2;
							fprintf(stream, "Q_AUTO_INCREMENT:   (%hu,%hu)\n", byte_1, byte_2);
							break;
							}
						case Q_CHARSET_CODE:
							{
							uint16_t byte_1 = *(uint16_t*)status_var_ptr;
							status_var_ptr += 2;
							uint16_t byte_2 = *(uint16_t*)status_var_ptr;
							status_var_ptr += 2;
							uint16_t byte_3 = *(uint16_t*)status_var_ptr;
							status_var_ptr += 2;
							fprintf(stream, "Q_CHARSET:          (%hu,%hu,%hu)\n", byte_1, byte_2, byte_3);
							break;
							}
						case Q_TIME_ZONE_CODE:
							{
							uint8_t size = *(status_var_ptr++);
							char* str = strndup(status_var_ptr, size);
							status_var_ptr += size;
							fprintf(stream, "Q_TIME_ZONE:        %s\n", str);
							free(str);
							break;
							}
						case Q_CATALOG_NZ_CODE:
							{
							uint8_t size = *(status_var_ptr++);
							char* str = strndup(status_var_ptr, size);
							status_var_ptr += size;
							fprintf(stream, "Q_CATALOG_NZ:       %s\n", str);
							free(str);
							break;
							}
						case Q_LC_TIME_NAMES_CODE:
							{
							uint16_t code = *(uint16_t*)status_var_ptr;
							status_var_ptr += 2;
							fprintf(stream, "Q_LC_TIME_NAMES:    %hu\n", code);
							break;
							}
						case Q_CHARSET_DATABASE_CODE:
							{
							uint16_t code = *(uint16_t*)status_var_ptr;
							status_var_ptr += 2;
							fprintf(stream, "Q_CHARSET_DATABASE: %hu\n", code);
							break;
							}
						default:
							{
							int incr = ybpi_status_var_data_len_by_type[status_var_type];
							fprintf(stream, "%s\n", ybpi_variable_types[status_var_type]);
							if (incr > 0) {
								status_var_ptr += incr;
							}
							else if (incr == -1) {
								uint8_t size = *status_var_ptr;
								status_var_ptr += size + 1;
							}
							else if (incr == -2) {
								uint8_t size = *status_var_ptr;
								status_var_ptr += size + 2;
							}
							else {
								assert(0);
							}
							fprintf(stream, "                        %s\n", ybpi_status_var_types[status_var_type]);
							break;
							}
					}
				}
			}
			fprintf(stream, "statement length:   %zd\n", statement_len);
			if (q_mode == 0)
				fprintf(stream, "statement:          %s\n", statement);
			free(statement);
			}
			break;
		case ROTATE_EVENT:
			{
			struct ybp_rotate_event *r = (struct ybp_rotate_event*)e->data;
			char *file_name = strndup((const char*)rotate_event_file_name(e), rotate_event_file_name_len(e));
			fprintf(stream, "next log position:  %llu\n", (unsigned long long)r->next_position);
			fprintf(stream, "next file name:     %s\n", file_name);
			free(file_name);
			}
			break;
		case INTVAR_EVENT:
			{
			struct ybp_intvar_event *i = (struct ybp_intvar_event*)e->data;
			fprintf(stream, "variable type:      %s\n", ybpi_intvar_types[i->type]);
			fprintf(stream, "value:              %llu\n", (unsigned long long) i->value);
			}
			break;
		case RAND_EVENT:
			{
			struct ybp_rand_event *r = (struct ybp_rand_event*)e->data;
			fprintf(stream, "seed 1:             %llu\n", (unsigned long long) r->seed_1);
			fprintf(stream, "seed 2:             %llu\n", (unsigned long long) r->seed_2);
			}
			break;
		case FORMAT_DESCRIPTION_EVENT:
			{
			struct ybp_format_description_event *f = ybp_event_as_fde(e);
			fprintf(stream, "binlog version:     %d\n", f->format_version);
			fprintf(stream, "server version:     %s\n", f->server_version);
			}
			break;
		case XID_EVENT:
			{
			struct ybp_xid_event *x = (struct ybp_xid_event*)e->data;
			fprintf(stream, "xid id:             %llu\n", (unsigned long long)x->id);
			}
			break;
		default:
			fprintf(stream, "event type:         %s\n", ybp_event_type(e));
			break;
	}
}

/* vim: set sts=0 sw=4 ts=4 noexpandtab: */
