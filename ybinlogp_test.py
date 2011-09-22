from ybinlogp import YBinlogP
import sys

bp = YBinlogP(sys.argv[1], always_update=True)
for i,event in enumerate(bp):
	print bp.tell()
	print event
for i,event in enumerate(bp):
	print bp.tell()
	print event
bp.clean_up()
