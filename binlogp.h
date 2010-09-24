/*
 * binlogp: A mysql binary log parser and query tool
 *
 * (C) 2010 Yelp, Inc.
 */

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
	off_t		offset;
};

struct format_description_event {
	uint16_t	format_version;	/* ought to be 4 */
	char[50]	server_version;
	uint32_t	timestamp;
	uint8_t		header_length;
};

struct query_event {
	uint32_t	thread_id;
	uint32_t	query_time;
	uint8_t		db_name_len;
	uint16_t	error_code;
	uint16_t	status_var_len;
};

struct rand_event {
	uint64_t	seed_1;
	uint64_t	seed_2;
};
#pragma pack(pop)

void print_event(struct event *e);
int check_event(struct event *e);
int read_data(int fd, struct event *evbuf, off_t offset);
int try_free_data(struct event *evbuf);
long long nearest_offset(int fd, off_t starting_offset, struct stat *stbuf, struct event *evbuf, int dir);
