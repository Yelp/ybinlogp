import sys
import binlog

binlog_file = open(sys.argv[1])
for event in binlog.BinlogParser(binlog_file):
	print event
