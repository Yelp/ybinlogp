import binlog
import datetime

def next_event(event):
	print 'event: 0x%x    type_code: %d, offset: %d, server_id: %d, timestamp: %s' % (id(event), event.type_code, event.offset, event.server_id, datetime.datetime.fromtimestamp(event.timestamp))
	print '    %r' % (event.data,)


file = open('mysql-bin.000001')
parser = binlog.BinlogParser(file)

for event in parser:
	next_event(event)
