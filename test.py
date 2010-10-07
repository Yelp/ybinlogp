import binlog
import datetime

def next_event(parser):
	event = parser.next()
	if event is None:
		print event
	else:
		print 'event: 0x%x    type_code: %d, offset: %d, server_id: %d, timestamp: %s' % (id(event), event.type_code, event.offset, event.server_id, datetime.datetime.fromtimestamp(event.timestamp))
		if type(event.data) is binlog.FormatDescriptionEvent:
			print '    format_version: %d, server_version: %d, timestamp: %s' % (event.data.format_version, event.data.server_version, event.data.timestamp)
		else:
			print '    %r' % (event.data,)

file = open('/home/evan/mysql-bin.000001')
parser = binlog.BinlogParser(file)

for x in xrange(5):
	next_event(parser)
