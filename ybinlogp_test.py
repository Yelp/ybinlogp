from ybinlogp import YBinlogP
import sys

bp = YBinlogP(sys.argv[1], always_update=True)
while True:
    for i,event in enumerate(bp):
        if event.event_type == "ROTATE_EVENT":
            next_file = event.data.file_name
            bp.clean_up()
            bp = YBinlogP(next_file, always_update=True)
            break
    else:
        print "Got to end at %r" % (bp.tell(),)
        break
bp.clean_up()
