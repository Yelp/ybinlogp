/*
 * binlogp: A mysql binary log parser and query tool
 *
 * (C) 2010 Yelp, Inc.
 */

#ifndef _YBINLOGP_H_
#define _YBINLOGP_H_

#include <stdint.h>
#include <sys/types.h>

#define BINLOG_VERSION 4

#define EVENT_HEADER_SIZE 19	/* we tack on extra stuff at the end */

#pragma pack(push)
#pragma pack(1)			/* force byte alignment */
struct event {
	uint32_t	timestamp;
	uint8_t		type_code;
	uint32_t	server_id;
	uint32_t	length;
	uint32_t	next_position;
	uint16_t	flags;
	char*		data;
	off64_t		offset;
};

#define format_description_event_data(e) (e->data + ((struct format_description_event*)e->data)->header_length)
#define format_description_event_data_len(e) (((struct format_description_event*)e->data)->header_len - EVENT_HEADER_SIZE)
struct format_description_event {
	uint16_t	format_version;	/* ought to be 4 */
	char		server_version[50];
	uint32_t	timestamp;
	uint8_t		header_len;
	// random data
};

#define query_event_statement(e) (e->data + sizeof(struct query_event) + ((struct query_event*)e->data)->status_var_len + ((struct query_event*)e->data)->db_name_len + 1)
#define query_event_statement_len(e) (e->length - EVENT_HEADER_SIZE - sizeof(struct query_event) - ((struct query_event*)e->data)->status_var_len - ((struct query_event*)e->data)->db_name_len - 1)
#define query_event_db_name(e) (e->data + sizeof(struct query_event) + ((struct query_event*)e->data)->status_var_len)
struct query_event {
	uint32_t	thread_id;
	uint32_t	query_time;
	uint8_t		db_name_len;
	uint16_t	error_code;
	uint16_t	status_var_len;
	// status variables (status_var_len)
	// database name    (db_name_len + 1, NUL)
	// statement        (the rest, not NUL)
};

struct rand_event {
	uint64_t	seed_1;
	uint64_t	seed_2;
};

struct xid_event {
	uint64_t	id;
};

struct intvar_event {
	uint8_t		type;
	uint64_t	value;
};

#define rotate_event_file_name(e) (e->data + 8)
#define rotate_event_file_name_len(e) (e->length - EVENT_HEADER_SIZE -8)
struct rotate_event {
	uint64_t	next_position;
	// file name of the next file (not NUL)
};
#pragma pack(pop)

/**
 * Initialize an event object. Event objects must live on the heap
 * and must be destroyed with dispose_event().
 *
 * Just sets everything to 0 for now.
 **/
void init_event(struct event*);

/**
 * Reset an event object, making it re-fillable
 *
 * Deletes the extra data and re-inits the object
 */
void reset_event(struct event *);

/**
 * Copy an event object, including any extra data
 */
int copy_event(struct event *, struct event *);

/**
 * Destroy an event object and any associated data
 **/
void dipose_event(struct event *);

/**
 * Print out an event
 **/
void print_event(struct event *e);

/**
 * Read an event from the specified offset into the given event buffer.
 *
 * Will also malloc() space for any dynamic portions of the event, if the 
 * event passes check_event.
 **/
int read_event(int, struct event *, off64_t);

/**
 * Check to see if an event looks valid.
 **/
int check_event(struct event *);

/**
 * Find the offset of the next event after the one passed in.
 * Uses the built-in event chaining.
 *
 * Usage:
 *  struct event *next;
 *  struct event *evbuf = ...
 *  off_t next_offset = next_after(evbuf);
 *  read_event(fd, next, next_offset);
 **/
off64_t next_after(struct event *evbuf);

#endif /* _YBINLOGP_H_ */
