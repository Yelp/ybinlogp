"""
Print events from a mysql binlog.
"""
import logging
import os.path
import sys

from ybinlogp import YBinlogP


def get_filename():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: %s <filename>" % sys.argv[0])
    return sys.argv[1]


def main(filename):
    dirname = os.path.dirname(filename)
    parser = YBinlogP(filename, always_update=True)
    while True:
        for i, event in enumerate(parser):
            print event
            if event.event_type == "ROTATE_EVENT":
                next_file = os.path.join(dirname, event.data.file_name)
                parser.close()
                parser = YBinlogP(next_file, always_update=True)
        else:
            print "Got to end at %r" % (parser.tell(),)
            break
    parser.close()


if __name__ == "__main__":
    logging.basicConfig()
    filename = get_filename()
    main(filename)
