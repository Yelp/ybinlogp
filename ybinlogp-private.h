/*
 * binlogp: A mysql binary log parser and query tool
 *
 * (C) 2010-2011 Yelp, Inc.
 *
 * This work is licensed under the ISC/OpenBSD License. The full
 * contents of that license can be found under license.txt
 */

#ifndef _YBINLOGP_PRIVATE_H_
#define _YBINLOGP_PRIVATE_H_

/******* various mappings ********/
static const char* ybpi_event_types[27] = {
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
	"HEARTBEAT_LOG_EVENT",      // 26
};

static const char* ybpi_variable_types[10] = {
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

static const char* ybpi_intvar_types[3] = {
	"",
	"LAST_INSERT_ID_EVENT",         // 1
	"INSERT_ID_EVENT",              // 2
};

static const char* ybpi_flags[16] = {
	"LOG_EVENT_BINLOG_IN_USE",     // 0x01
	"LOG_EVENT_FORCED_ROTATE",     // 0x02 (deprecated)
	"LOG_EVENT_THREAD_SPECIFIC",   // 0x04
	"LOG_EVENT_SUPPRESS_USE",      // 0x08
	"LOG_EVENT_UPDATE_TABLE_MAP_VERSION", // 0x10
	"LOG_EVENT_ARTIFICIAL",        // 0x20
	"LOG_EVENT_RELAY_LOG",         // 0x40
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
};

/* The mysterious FLAGS2 binlog code.
 * Seems to be a subset of mysql options.
 * A very small subset.
 */
static const char* ybpi_flags2[32] = {
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


/** Macros to do things with event data 
 *
 * These macros are horribly unsafe. Only use them of you know EXACTLY what
 * you are doing. Otherwise, use ybp_query_event_safe_data and
 * ybp_event_to_qes
 **/
#define query_event_statement(e) (e->data + sizeof(struct ybp_query_event) + ((struct ybp_query_event*)e->data)->status_var_len + ((struct ybp_query_event*)e->data)->db_name_len + 1)
#define query_event_status_vars(e) (e->data + sizeof(struct ybp_query_event))
#define query_event_statement_len(e) (e->length - EVENT_HEADER_SIZE - sizeof(struct ybp_query_event) - ((struct ybp_query_event*)e->data)->status_var_len - ((struct ybp_query_event*)e->data)->db_name_len - 1)
#define query_event_db_name(e) (e->data + sizeof(struct ybp_query_event) + ((struct ybp_query_event*)e->data)->status_var_len)
#define rotate_event_file_name(e) (e->data + 8)
#define rotate_event_file_name_len(e) ((size_t)(e->length - EVENT_HEADER_SIZE - sizeof(uint64_t)))

#endif /* _YBINLOGP_PRIVATE_H */
