ybinlogp - a fast mysql binlog parsing utility
==============================================
**ybinlogp** is a mysql utility for analyzing mysql binlogs. It provides a library,
libybinlogp, which has a really terrible build system, a little tool documented
below which uses this library, and a python-ctypes wrapper that exposes some
critical functionality (namely, opening a binlog, reading from it, and handling
query, xid, and rotate events).

Usage
-----
ybinlogp [options] binlog-file

Options:

 *  `-o OFFSET          Find events after a given offset`
 *  `-t TIME            Find events after a given unix timestamp`
 *  `-a NUMBER          Print N events after the given one (accepts 'all')`
 *  `-D DBNAME          Filter out query statements not on database DBNAME`
 *  `-q                 Be quieter (may be specified multiple times)`
 *  `-h                 Show help`


Why?
----
If you have a replicated MySQL instance, you're probably used to ocassionally seeing
it freak out. **ybinlogp** lets you just put in a time or offset and see exactly what
was going on around then. Compare this to the default mysql binlog parser, which uses
the linked list feature in the binlogs and so is uselessly slow when dealing with anything
past the first few events (and also doesn't have a time search feature; how often do
you actually know what the offset of an event is?)

Who?
----
**ybinlogp** was developed by some engineers at [Yelp](http://www.yelp.com) for use
with their MySQL installation. The initial development was done by James Brown (<jbrown@yelp.com>);
Evan Klitzke (<evan@yelp.com>) worked on some bugfixes and a partially-complete Python API, and
Eskil Olsen (<eskil@yelp.com>) has a branch that does some crazy stuff with Boost.

Contributing
-----------
It's Github... Fork away!

License
-------
This work is available under the ISC (OpenBSD) license. The full contents are available
as license.txt
