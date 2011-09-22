/*
 * ybinlogp: A mysql binary log parser and query tool
 *
 * (C) 2010 Yelp, Inc.
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
#define MAX_EVENT_LENGTH 16*1048576  // Max statement len is generally 16MB
#define MAX_SERVER_ID 4294967295   // 0 <= server_id  <= 2**32

/******* various mappings ********/
static char* ybpi_event_types[27] = {
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

static char* ybpi_variable_types[10] = {
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

static char* ybpi_intvar_types[3] = {
	"",
	"LAST_INSERT_ID_EVENT",	       // 1
	"INSERT_ID_EVENT",             // 2
};

static char* ybpi_flags[16] = {
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

/* The mysterious FLAGS2 binlog code.
 * Seems to be a subset of mysql options.
 * A very small subset.
 */
static char* ybpi_flags2[32] = {
	"", // 0x01
	"", // 0x02
	"", // 0x04
	"", // 0x08
	"", // 0x10
	"", // 0x20
	"", // 0x40
	"", // 0x80
	"", // 0x100
	"", // 0x200
	"", // 0x400
	"", // 0x800
	"", // 0x1000
	"", // 0x2000
	"OPTION_AUTO_IS_NULL", // 0x4000
	"", // 0x8000
	"", // 0x10000
	"", // 0x20000
	"", // 0x40000
	"OPTION_NOT_AUTOCOMMIT", // 0x80000
	"", // 0x100000
	"", // 0x200000
	"", // 0x400000
	"", // 0x800000
	"", // 0x1000000
	"", // 0x2000000
	"OPTION_NO_FOREIGN_KEY_CHECKS", // 0x4000000
	"OPTION_RELAXED_UNIQUE_CHECKS", // 0x8000000
};

/* Map of the lengths of status var data.
 * -1 indicates variable (the first byte is a length byte)
 *  -2 indicates variable + 1 (the first byte is a length byte that is
 *  wrong)
 */
static int ybpi_status_var_data_len_by_type[10] = {
	4, // 0 = Q_FLAGS2_CODE
	8, // 1 = Q_SQL_MODE_CODE
	-2,// 2 = Q_CATALOG_CODE (length byte + string + NUL)
	4, // 3 = Q_AUTO_INCREMENT (2 2-byte ints)
	6, // 4 = Q_CHARSET_CODE (3 2-byte ints)
	-1,// 5 = Q_TIME_ZONE_CODE (length byte + string)
	-1,// 6 = Q_CATALOG_NZ_CODE (length byte + string)
	2, // 7 = Q_LC_TIME_NAMES_COE
	2, // 8 = Q_CHARSET_DATABASE_CODE
	8, // 9 = Q_TABLE_MAP_FOR_UPDATE_COE
};

enum ybpi_e_status_var_types {
	Q_FLAGS2_CODE=0,
	Q_SQL_MODE_CODE=1,
	Q_CATALOG_CODE=2,
	Q_AUTO_INCREMENT=3,
	Q_CHARSET_CODE=4,
	Q_TIME_ZONE_CODE=5,
	Q_CATALOG_NZ_CODE=6,
	Q_LC_TIME_NAMES_CODE=7,
	Q_CHARSET_DATABASE_CODE=8,
	Q_TABLE_MAP_FOR_UPDATE_CODE=9
};

static const char* ybpi_status_var_types[10] = {
	"Q_FLAGS2_CODE",
	"Q_SQL_MODE_CODE",
	"Q_CATALOG_CODE",
	"Q_AUTO_INCREMENT",
	"Q_CHARSET_CODE",
	"Q_TIME_ZONE_CODE",
	"Q_CATALOG_NZ_CODE",
	"Q_LC_TIME_NAMES_CODE",
	"Q_CHARSET_DATABASE_CODE",
	"Q_TABLE_MAP_FOR_UPDATE_CODE"
};

/******* predeclarations of ybpi functions *******/
static int ybpi_read_fde(struct ybp_binlog_parser*);
static int ybpi_read_event(struct ybp_binlog_parser*, off_t, struct ybp_event*);
static bool ybpi_check_event(struct ybp_event*, struct ybp_binlog_parser*);
static off64_t ybpi_next_after(struct ybp_event* restrict);

/******** implementation begins here ********/

int ybp_init_binlog_parser(int fd, struct ybp_binlog_parser** restrict out)
{
	struct ybp_binlog_parser* result;
	if ((result = malloc(sizeof(struct ybp_binlog_parser))) == NULL) {
		return -1;
	}
	if ((out == NULL) || (out == NULL)) {
		return -1;
	}
	result->fd = fd;
	result->offset = 4;
	result->enforce_server_id = 0;
	result->slave_server_id = 0;
	result->master_server_id = 0;
	result->min_timestamp = 0;
	result->max_timestamp = time(NULL);
	result->has_read_fde = false;
	*out = result;
	return 0;
}

void ybp_dispose_binlog_parser(struct ybp_binlog_parser* p)
{
	if (p != NULL)
		free(p);
}

void ybp_init_event(struct ybp_event* evbuf) {
	memset(evbuf, 0, sizeof(struct ybp_event*));
}

void ybp_dispose_event(struct ybp_event* evbuf)
{
	Dprintf("About to dispose_event 0x%p\n", (void*)evbuf);
	if (evbuf->data != 0) {
		Dprintf("Freeing data at 0x%p\n", (void*)evbuf->data);
		free(evbuf->data);
		evbuf->data = 0;
	}
	free(evbuf);
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
	if ((!p->enforce_server_id || (e->server_id == p->slave_server_id) || (e->server_id == p->master_server_id)) &&
			e->type_code > MIN_TYPE_CODE &&
			e->type_code < MAX_TYPE_CODE &&
			e->length > MIN_EVENT_LENGTH &&
			e->length < MAX_EVENT_LENGTH && 
			e->timestamp >= p->min_timestamp &&
			e->timestamp <= p->max_timestamp) {
		return true;
	} else {
		return false;
	}
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

/**
 * Read an event from the parser parser, at offset offet, storing it in
 * event evbuf (which should be already init'd)
 */
static int ybpi_read_event(struct ybp_binlog_parser* p, off_t offset, struct ybp_event* evbuf)
{
	ssize_t amt_read;
	p->max_timestamp = time(NULL);
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
		return -1;
	}
	if (ybpi_check_event(evbuf, p)) {
		Dprintf("mallocing %d bytes\n", evbuf->length - EVENT_HEADER_SIZE);
		if ((evbuf->data = malloc(evbuf->length - EVENT_HEADER_SIZE)) == NULL) {
			perror("malloc:");
			return -1;
		}
		Dprintf("malloced %d bytes at 0x%p for a %s\n", evbuf->length - EVENT_HEADER_SIZE, evbuf->data, ybpi_event_types[evbuf->type_code]);
		if (read(p->fd, evbuf->data, evbuf->length - EVENT_HEADER_SIZE) < 0) {
			perror("reading extra data:");
			return -1;
		}
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
	int esi = p->enforce_server_id;
	int fd = p->fd;

	if ((evbuf = malloc(sizeof(struct ybp_event))) == NULL) {
		return -1;
	}
	ybp_init_event(evbuf);

	if (ybpi_read_event(p, 4, evbuf) < 0) {
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
	p->min_timestamp = evbuf->timestamp;
	p->slave_server_id = evbuf->server_id;

	offset = ybpi_next_after(evbuf);
	p->offset = offset;
	ybp_reset_event(evbuf);
	ybpi_read_event(p, offset, evbuf);

	p->master_server_id = evbuf->server_id;
	ybp_dispose_event(evbuf);

	lseek(fd, 4, SEEK_SET);
	p->has_read_fde = true;
	return 0;
}

int ybp_next_event(struct ybp_binlog_parser* parser, struct ybp_event* evbuf)
{
	off64_t offset = 4;
	int ret = 0;
	if (!parser->has_read_fde) {
		ybpi_read_fde(parser);
		return ybpi_read_event(parser, offset, evbuf);
	}
	ret = ybpi_read_event(parser, parser->offset, evbuf);
	if (ret < 0) { 
		return ret;
	} else {
		parser->offset = ybpi_next_after(evbuf);
		if ((parser->offset <= 0) || (evbuf->next_position == evbuf->offset)) {
			return 0;
		} else {
			return 1;
		}
	}
}

struct ybp_format_description_event* ybp_event_as_fde(struct ybp_event* e)
{
	if (e->type_code != FORMAT_DESCRIPTION_EVENT) {
		return NULL;
	}
	else {
		return (struct ybp_format_description_event*)(e->data);
	}
}

char* ybp_event_type(struct ybp_event* evbuf) {
	Dprintf("Looking up type string for %d", evbuf->type_code);
	return ybpi_event_types[evbuf->type_code];
}
