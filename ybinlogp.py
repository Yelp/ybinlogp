"""
 ybinlogp: A mysql binary log parser and query tool

 (C) 2010-2011 Yelp, Inc.

 This work is licensed under the ISC/OpenBSD License. The full
 contents of that license can be found under license.txt
"""

import ctypes
import datetime
import errno

library = ctypes.CDLL("libybinlogp.so.1", use_errno=True)

class EventStruct(ctypes.Structure):
	"""Internal data structure for Events"""
	_fields_ = [("timestamp", ctypes.c_uint32),
			("type_code", ctypes.c_uint8),
			("server_id", ctypes.c_uint32),
			("length", ctypes.c_uint32),
			("next_position", ctypes.c_uint32),
			("flags", ctypes.c_uint16),
			("data", ctypes.c_void_p),
			("offset", ctypes.c_uint64)]

	_pack_ = 1

class Event(object):
	"""User-facing data structure for Events"""
	def __init__(self, event_type, offset, timestamp):
		self.event_type = event_type
		self.offset = offset
		self.time = datetime.datetime.fromtimestamp(timestamp)
		self.data = None

	def __str__(self):
		return "%s at %s%s" % (self.event_type, self.time, ": " + str(self.data) if self.data is not None else "")

class QueryEventStruct(ctypes.Structure):
	"""Internal data structure for query events"""
	_fields_ = [("thread_id", ctypes.c_uint32),
			("query_time", ctypes.c_uint32),
			("db_name_len", ctypes.c_uint8),
			("error_code", ctypes.c_uint16),
			("status_var_len", ctypes.c_uint16),
			("statement", ctypes.c_char_p),
			("statement_len", ctypes.c_size_t),
			("status_var", ctypes.c_char_p),
			("db_name", ctypes.c_char_p)]

class QueryEvent(object):
	"""User-facing data structure for query events"""
	def __init__(self, db_name, statement, query_time):
		self.db_name = db_name
		self.statement = statement
		self.query_time = query_time

	def __str__(self):
		return "Query(db='%s', statement='%s', query_time=%d)" % (self.db_name, self.statement, self.query_time)

class RotateEventStruct(ctypes.Structure):
	"""Internal data structure for rotatation events"""
	_fields_ = [("next_position", ctypes.c_uint64),
			("file_name", ctypes.c_char_p),
			("file_name_len", ctypes.c_size_t)]

class RotateEvent(object):
	"""User-facing data structure for rotatation events"""
	def __init__(self, next_position, file_name):
		self.next_position = next_position
		self.file_name = file_name

	def __str__(self):
		return "Rotate(next file=%s, next_position=%d)" % (self.file_name, self.next_position)

class XIDEventStruct(ctypes.Structure):
	"""Internal data structure for XID events"""
	_fields_ = [("id", ctypes.c_uint64)]

class XIDEvent(object):
	"""User-facing data structure for XID events, which seem to all represent COMMITs"""
	def __init__(self, xid):
		self.xid = xid

	def __str__(self):
		return "COMMIT xid %d" % self.xid

_init_bp = library.ybp_get_binlog_parser
_init_bp.argtypes = [ctypes.c_int]
_init_bp.restype = ctypes.c_void_p

_get_event = library.ybp_get_event
_get_event.argtypes = []
_get_event.restype = ctypes.POINTER(EventStruct)

_next_event = library.ybp_next_event
_next_event.argtypes = [ctypes.c_void_p, ctypes.POINTER(EventStruct)]
_next_event.restype = ctypes.c_int

_reset_event = library.ybp_reset_event
_reset_event.argtypes = [ctypes.POINTER(EventStruct)]
_reset_event.restype = None

_dispose_event = library.ybp_dispose_event
_dispose_event.argtypes = [ctypes.POINTER(EventStruct)]
_dispose_event.restype = None

_dispose_bp = library.ybp_dispose_binlog_parser
_dispose_bp.argtypes = [ctypes.c_void_p]
_dispose_bp.restype = None

_update_bp = library.ybp_update_bp
_update_bp.argtypes = [ctypes.c_void_p]
_update_bp.restype = None

_event_type = library.ybp_event_type
_event_type.argtypes = [ctypes.POINTER(EventStruct)]
_event_type.restype = ctypes.c_char_p

_event_to_safe_qe = library.ybp_event_to_safe_qe
_event_to_safe_qe.argtypes = [ctypes.POINTER(EventStruct)]
_event_to_safe_qe.restype = ctypes.POINTER(QueryEventStruct)

_dispose_safe_qe = library.ybp_dispose_safe_qe
_dispose_safe_qe.argtype = [ctypes.POINTER(QueryEventStruct)]
_dispose_safe_qe.qestype = None

_event_to_safe_re = library.ybp_event_to_safe_re
_event_to_safe_re.argtypes = [ctypes.POINTER(EventStruct)]
_event_to_safe_re.restype = ctypes.POINTER(RotateEventStruct)

_dispose_safe_re = library.ybp_dispose_safe_re
_dispose_safe_re.argtype = [ctypes.POINTER(RotateEventStruct)]
_dispose_safe_re.restype = None

_event_to_safe_xe = library.ybp_event_to_safe_xe
_event_to_safe_xe.argtypes = [ctypes.POINTER(EventStruct)]
_event_to_safe_xe.restype = ctypes.POINTER(XIDEventStruct)

_dispose_safe_xe = library.ybp_dispose_safe_xe
_dispose_safe_xe.argtype = [ctypes.POINTER(XIDEventStruct)]
_dispose_safe_xe.restype = None

# no c_off in ctypes, using c_longlong instead
_rewind_bp = library.ybp_rewind_bp
_rewind_bp.argtypes = [ctypes.c_void_p, ctypes.c_longlong]
_rewind_bp.restype = None

_tell_bp = library.ybp_tell_bp
_tell_bp.argtypes = [ctypes.c_void_p]
_tell_bp.restype = ctypes.c_longlong

_nearest_offset = library.ybp_nearest_offset
_nearest_offset.argtypes = [ctypes.c_void_p, ctypes.c_longlong]
_nearest_offset.restype = ctypes.c_longlong

_nearest_time = library.ybp_nearest_time
_nearest_time.argtypes = [ctypes.c_void_p, ctypes.c_long]
_nearest_time.restype = ctypes.c_longlong

class YBinlogPError(Exception):
	pass

class YBinlogPSysError(YBinlogPError):
	def __init__(self, errno):
		self.errno = errno

	def __repr__(self):
		return "NextEventError(%s)" % errno.errorcode.get(self.errno, "Unknown")

	def __str__(self):
		return repr(self)

class NextEventError(YBinlogPSysError):
	pass

class NoEventsAfterTime(YBinlogPError):
	pass

class NoEventsAfterOffset(YBinlogPError):
	pass

class YBinlogP(object):
	"""Python interface to ybinlogp, the fast mysql binlog parser.

	Example usage:

	bp = YBinlogP('/path/to/binlog/file')
	for query in bp:
		if event.event_type == "QUERY_EVENT":
		    print event.data.statement
	bp.clean_up()
	"""
	def __init__(self, filename, always_update=False):
		"""Construct a YBinlogP.

		Arguments:
			filename: A binlog filename
			always_update: if True, stat the file before doing anything
				interesting
		"""
		self.filename = filename
		self._file = open(self.filename, 'r')
		self.binlog_parser_handle = _init_bp(self._file.fileno())
		self.event_buffer = _get_event();
		self.always_update = always_update

	def _get_next_event(self):
		last = _next_event(self.binlog_parser_handle, self.event_buffer)
		if last < 0:
			raise NextEventError(ctypes.get_errno())
		et = _event_type(self.event_buffer)
		base_event = Event(et, self.event_buffer.contents.offset, self.event_buffer.contents.timestamp)
		if et == "QUERY_EVENT":
			query_event = _event_to_safe_qe(self.event_buffer)
			base_event.data = QueryEvent(query_event.contents.db_name, query_event.contents.statement, query_event.contents.query_time)
			_dispose_safe_qe(query_event)

		elif et == "ROTATE_EVENT":
			rotate_event = _event_to_safe_re(self.event_buffer)
			base_event.data = RotateEvent(rotate_event.contents.next_position, rotate_event.contents.file_name)
			_dispose_safe_re(rotate_event)

		elif et == "XID_EVENT":
			xid_event = _event_to_safe_xe(self.event_buffer)
			base_event.data = XIDEvent(xid_event.contents.id)
			_dispose_safe_xe(xid_event)

		return (base_event, last == 0)

	def clean_up(self):
		"""Clean up some things that are allocated in C-land. Attempting to
		use this object after calling this method will break."""
		# TODO: should this be a __del__?
		_dispose_bp(self.binlog_parser_handle)
		self.binlog_parser_handle = None
		_dispose_event(self.event_buffer)
		self.event_buffer = None
		self._file.close()

	def tell(self):
		return (self.filename, _tell_bp(self.binlog_parser_handle))

	def update(self):
		"""Update the binlog parser. This just re-stats the underlying file descriptor.
		Call this if you have reason to believe that the underlying file has changed size
		(or set always_update to be true on the Python wrapper object)."""
		_update_bp(self.binlog_parser_handle)

	def __iter__(self):
		last = False
		while not last:
			if self.always_update:
				self.update()
			try:
				(event, last) = self._get_next_event()
			except NextEventError, e:
				if e.errno == 0:
					return
				else:
					raise
			yield event

	def rewind(self, offset):
		"""Reset this bp to point to the beginning of the file"""
		_rewind_bp(self.binlog_parser_handle, offset)

	def first_offset_after_time(self, t):
		"""Find the first offset after the given unix timestamp. Usage:

		bp = YBinlogP('/path/to/binlog')
		offset = bp.first_offset_after_time(1293868800) # jan 1 2011
		bp.rewind(offset)
		for record in bp:
		    # ...
		"""
		offset = _nearest_time(self.binlog_parser_handle, t)
		if offset == -1:
			raise NextEventError(ctypes.get_errno())
		elif offset == -2:
			raise NoEventsAfterTime()
		else:
			return offset

	def first_offset_after_offset(self, t):
		"""Find the first valid offset after the given offset. Usage:

		bp = YBinlogP('/path/to/binlog')
		offset = bp.first_offset_after_offset(1048576) # skip the first 1 MB
		bp.rewind(offset)
		for record in bp:
		    # ...
		"""
		offset = _nearest_offset(self.binlog_parser_handle, t)
		if offset == -1:
			raise NextEventError(ctypes.get_errno())
		elif offset == -2:
			raise NoEventsAfterOffset()
		else:
			return offset
