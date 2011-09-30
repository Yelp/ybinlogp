/*
 * binlogp: A mysql binary log parser and query tool
 *
 * (C) 2010-2011 Yelp, Inc.
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

struct ybp_binlog_parser {
	int			fd;
	off_t		file_size;
	ssize_t		offset;
	bool		enforce_server_id;
	bool		has_read_fde;
	uint32_t	slave_server_id;
	uint32_t	master_server_id;
	time_t		min_timestamp;
	time_t		max_timestamp;
};

enum ybp_event_types {
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

struct ybp_format_description_event {
	uint16_t	format_version;	/* ought to be 4 */
	char		server_version[50];
	uint32_t	timestamp;
	uint8_t		header_len;
	// random data
};

struct ybp_query_event {
	uint32_t	thread_id;
	uint32_t	query_time;
	uint8_t		db_name_len;
	uint16_t	error_code;
	uint16_t	status_var_len;
	// status variables (status_var_len)
	// database name    (db_name_len + 1, NUL)
	// statement        (the rest, not NUL)
};


struct ybp_rand_event {
	uint64_t	seed_1;
	uint64_t	seed_2;
};

struct ybp_xid_event {
	uint64_t	id;
};

struct ybp_intvar_event {
	uint8_t		type;
	uint64_t	value;
};

struct ybp_rotate_event {
	uint64_t	next_position;
	// file name of the next file (not NUL)
};
#pragma pack(pop)

/**
 * Use this to safely access the data portions of a query event. Note that
 * this involves copying things, so it's pretty slow.
 **/
struct ybp_query_event_safe {
	uint32_t	thread_id;
	uint32_t	query_time;
	uint8_t		db_name_len;
	uint16_t	error_code;
	uint16_t	status_var_len;
	char*		statement;
	size_t		statement_len;
	char*		status_var;
	char*		db_name;
};

struct ybp_rotate_event_safe {
	uint64_t	next_position;
	char*		file_name;
	size_t		file_name_len;
};

/**
 * Initialize a ybp_binlog_parser. Returns 0 on success, non-zero otherwise.
 *
 * Arguments:
 *    fd: A file descriptor open in reading mode to a binlog file
 **/
struct ybp_binlog_parser* ybp_get_binlog_parser(int);

/**
 * Update the ybp_binlog_parser.
 *
 * Call this any time you expect that the underlying file might've changed,
 * and want to be able to see those changes.
 **/
void ybp_update_bp(struct ybp_binlog_parser*);

/**
 * Get the offset in the bp
 **/
off64_t ybp_tell_bp(struct ybp_binlog_parser*);

/**
 * Rewind the ybp_binlog_parser to the given offset
 *
 * Call this any time you expect that the underlying file might've changed,
 * and want to be able to see those changes.
 **/
void ybp_rewind_bp(struct ybp_binlog_parser*, off_t);

/**
 * Clean up a ybp_binlog_parser
 **/
void ybp_dispose_binlog_parser(struct ybp_binlog_parser*);

/**
 * Advance a ybp_binlog_parser structure to the next event.
 *
 * Arguments:
 *    p: A binlog_parser
 *    evbuf: An event buffer (inited with ybp_init_event, or resetted with
 *    ybp_reset_event) which will be written to
 * Returns 0 if the current event is the last event, <0 on error, and >0
 * otherwise.
 */
int ybp_next_event(struct ybp_binlog_parser* restrict, struct ybp_event* restrict);

/**
 * Initialize an event object. Event objects must live on the heap
 * and must be destroyed with dispose_event().
 *
 * Just sets everything to 0 for now.
 **/
void ybp_init_event(struct ybp_event*);

/**
 * Get a clean event object. Like ybp_init_event, but it does the malloc for
 * you.
 **/
struct ybp_event* ybp_get_event(void);

/**
 * Reset an event object, making it re-fillable
 *
 * Deletes the extra data and re-inits the object
 */
void ybp_reset_event(struct ybp_event*);

/**
 * Destroy an event object and any associated data
 **/
void ybp_dispose_event(struct ybp_event*);

/**
 * Copy an event and attached data from source to dest. Both must already
 * exist and have been init'd
 **/
int ybp_copy_event(struct ybp_event* dest, struct ybp_event* source);

/**
 * Print event e to the given iostream.
 *
 * if the stream is null, print to stdout.
 **/
void ybp_print_event_simple(struct ybp_event* restrict, struct ybp_binlog_parser* restrict, FILE* restrict);

/**
 * Print event e to the given iostream
 *
 * Args:
 *   event
 *   binlog parser
 *   iostream
 *   q_mode
 *   v_mode
 *   database restriction
 **/
void ybp_print_event(struct ybp_event* restrict, struct ybp_binlog_parser* restrict, FILE* restrict, bool, bool, char*);

/**
 * Get the string type of an event
 **/
const char* ybp_event_type(struct ybp_event*);

/**
 * Interpret an event as an FDE. Returns either a pointer to the FDE, or
 * NULL.
 *
 * WARNING: The pointer returned will share memory space with the evbuf
 * argument passed in.
 */
struct ybp_format_description_event* ybp_event_as_fde(struct ybp_event* restrict);

/**
 * Get a safe-to-mess-with query event from an event
 **/
struct ybp_query_event_safe* ybp_event_to_safe_qe(struct ybp_event* restrict);

/**
 * Dispose a structure returned from ybp_event_to_safe_qe
 **/
void ybp_dispose_safe_qe(struct ybp_query_event_safe*);

/**
 * Get a safe-to-mess-with rotate event from an event
 **/
struct ybp_rotate_event_safe* ybp_event_to_safe_re(struct ybp_event* restrict);

/**
 * Dispose a structure returned from ybp_event_to_safe_qe
 **/
void ybp_dispose_safe_re(struct ybp_rotate_event_safe*);

/**
 * Get a safe-to-mess-with xid event from an event
 **/
struct ybp_xid_event* ybp_event_to_safe_xe(struct ybp_event* restrict);

/**
 * Dispose a structure returned from ybp_event_to_safe_xe
 **/
void ybp_dispose_safe_xe(struct ybp_xid_event*);

/**
 * Search tools!
 **/
off64_t ybp_nearest_offset(struct ybp_binlog_parser* restrict, off64_t);

off64_t ybp_nearest_time(struct ybp_binlog_parser* restrict, time_t target);

/* vim: set sts=0 sw=4 ts=4 noexpandtab: */

#endif /* _YBINLOGP_H_ */
