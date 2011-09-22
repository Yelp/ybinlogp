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

/* binlog parameters */
#define MIN_TYPE_CODE 0
#define MAX_TYPE_CODE 27
#define MIN_EVENT_LENGTH 19
#define MAX_EVENT_LENGTH 16*1048576  // Max statement len is generally 16MB
#define MAX_SERVER_ID 4294967295   // 0 <= server_id  <= 2**32

static int ybpi_read_fde(struct ybp_binlog_parser*);
static int ybpi_read_event(struct ybp_binlog_parser*, off_t, struct ybp_event*);
static bool ybpi_check_event(struct ybp_event*, struct ybp_binlog_parser*);
static off64_t ybpi_next_after(struct ybp_event* restrict);

int ybp_init_binlog_parser(int fd, struct ybp_binlog_parser** restrict out) {
	struct ybp_binlog_parser* result;
	if ((result = malloc(sizeof(struct binlog_parser*))) == NULL) {
		return -1;
	}
	if (*out == NULL) {
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

void ybp_init_event(struct ybp_event* evbuf) {
	memset(evbuf, 0, sizeof(struct ybp_event*));
}

void ybp_dispose_event(struct ybp_event* evbuf)
{
	Dprintf(stderr, "About to dispose_event 0x%p\n", evbuf);
	if (evbuf->data != 0) {
		free(evbuf->data);
		evbuf->data = 0;
	}
	free(evbuf);
}

void ybp_reset_event(struct ybp_event* evbuf)
{
	Dprintf(stderr, "Resetting event\n");
	if (evbuf->data != 0) {
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
		return -1;
	}
	p->enforce_server_id = esi;
	struct format_description_event *f = (struct format_description_event*) evbuf->data;
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

int ybp_next_event(struct ybp_binlog_parser* parser) {
	if (!parser->has_read_fde) {
		ybpi_read_fde(parser);
	}
	(void) parser;
	return 0;
}
