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

#include <stdint.h>
#include <sys/types.h>

#define BINLOG_VERSION 4
#define MIN_TYPE_CODE 0
#define MAX_TYPE_CODE 27
#define NUM_EVENT_TYPES 27
#define NUM_VARIABLE_TYPES 10
#define NUM_INTVAR_TYPES 3
#define NUM_FLAGS 16

#define EVENT_HEADER_SIZE 19	/* we tack on extra stuff at the end */

enum e_event_type {
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

enum e_variable_type {
	Q_FLAGS2_CODE=0,
	Q_SQL_MODE_CODE=1,
	Q_CATALOG_CODE=2,
	Q_AUTO_INCREMENT=3,
	Q_CHARSET_CODE=4,
	Q_TIME_ZONE_CODE=5,
	Q_CATALOG_NZ_CODE=6,
	Q_LC_TIME_NAMES_CODE=7,
	Q_CHARSET_DATABASE_CODE=8,
	Q_TABLE_MAP_FOR_UPDATE_CODE=9,
};

enum e_intvar_type {
	LAST_INSERT_ID_EVENT=1,
	INSERT_ID_EVENT=2,
};

enum e_flag {
	LOG_EVENT_BINLOG_IN_USE=0x01,
	LOG_EVENT_FORCED_ROTATE=0x02,
	LOG_EVENT_THREAD_SPECIFIC=0x04,
	LOG_EVENT_SUPPRESS_USE=0x08,
	LOG_EVENT_UPDATE_TABLE_MAP_VERSION=0x10,
	LOG_EVENT_ARTIFICIAL=0x20,
	LOG_EVENT_RELAY_LOG=0x40,
};

const char* event_type(enum e_event_type);
const char* variable_type(enum e_variable_type);
const char* intvar_type(enum e_intvar_type);
const char* flag_name(enum e_flag);

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

#define format_description_event_data(e) ((e)->data + ((struct format_description_event*) (e)->data)->header_len)
#define format_description_event_data_len(e) (((struct format_description_event*) (e)->data)->header_len - EVENT_HEADER_SIZE)
struct format_description_event {
	uint16_t	format_version;	/* ought to be 4 */
	char		server_version[50];
	uint32_t	timestamp;
	uint8_t		header_len;
	// random data
};

#define query_event_statement(e) ((e)->data + sizeof(struct query_event) + ((struct query_event*)(e)->data)->status_var_len + ((struct query_event*)(e)->data)->db_name_len + 1)
#define query_event_statement_len(e) ((e)->length - EVENT_HEADER_SIZE - sizeof(struct query_event) - ((struct query_event*)(e)->data)->status_var_len - ((struct query_event*)(e)->data)->db_name_len - 1)
#define query_event_db_name(e) ((e)->data + sizeof(struct query_event) + ((struct query_event*)(e)->data)->status_var_len)
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

#define rotate_event_file_name(e) ((e)->data + 8)
#define rotate_event_file_name_len(e) ((e)->length - EVENT_HEADER_SIZE - 8)
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
void dispose_event(struct event *);

/**
 * Print out only statement events, and only the statement
 **/
void print_statement_event(struct event *e);

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
 * Read an event from the specified offset into the given event buffer.
 *
 * Will also malloc() space for any dynamic portions of the event.
 *
 * Does not check the validity of the record
 **/
int read_event_unsafe(int, struct event *, off64_t);

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

/* XXX: lol, undocumented */
int read_fde(int, struct event *);
off64_t nearest_offset(int, off64_t, struct event *, int);
int init_ybinlogp(void);

#endif /* _YBINLOGP_H_ */
