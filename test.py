import binlog
import datetime

file = open('mysql-bin.000001')
parser = binlog.BinlogParser(file)

for event in parser:
	print event
