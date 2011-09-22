/*
 * binlogp: A mysql binary log parser and query tool
 *
 * (C) 2010 Yelp, Inc.
 *
 * This work is licensed under the ISC/OpenBSD License. The full
 * contents of that license can be found under license.txt
 */

#ifndef _YBINLOGP_H_
#define _YBINLOGP_H_

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#define BINLOG_VERSION 4

#define EVENT_HEADER_SIZE 19	/* we tack on extra stuff at the end */

struct ybp_binlog_parser;
struct ybp_event;
struct format_description_event;
struct query_event;

struct ybp_binlog_parser {
	int			fd;
	ssize_t		offset;
	bool		enforce_server_id;
	bool		has_read_fde;
	uint32_t	slave_server_id;
	uint32_t	master_server_id;
	time_t		min_timestamp;
	time_t		max_timestamp;
};

#pragma pack(push)
#pragma pack(1)			/* force byte alignment */
struct ybp_event {
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
#define query_event_status_vars(e) (e->data + sizeof(struct query_event))
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
 * Initialize a ybp_binlog_parser. Returns 0 on success, non-zero otherwise.
 *
 * Arguments:
 *    fd: A file descriptor open in reading mode to a binlog file
 *    out: a pointer to a *ybp_binlog_parser. will be filled with a malloced
 *    ybp_binlog_parser
 **/
int ybp_init_binlog_parser(int fd, struct ybp_binlog_parser** out);

/**
 * Advance a ybp_binlog_parser structure to the next event.
 *
 * Returns 0 if the current event is the last event, <0 on error, and >0
 * otherwise.
 */
int ybp_next_event(struct ybp_binlog_parser* parser);

/**
 * Initialize an event object. Event objects must live on the heap
 * and must be destroyed with dispose_event().
 *
 * Just sets everything to 0 for now.
 **/
void ybp_init_event(struct ybp_event*);

/**
 * Reset an event object, making it re-fillable
 *
 * Deletes the extra data and re-inits the object
 */
void ybp_reset_event(struct ybp_event *);

/**
 * Destroy an event object and any associated data
 **/
void ybp_dispose_event(struct ybp_event *);


#endif /* _YBINLOGP_H_ */
