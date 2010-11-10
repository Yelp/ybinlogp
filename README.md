ybinlogp - a fast mysql binlog parsing utility
==============================================
**ybinlogp** is a mysql utility for analyzing mysql binlogs. It currently
only has a command-line interface; there might at some point in the future
be a working Python interface.

Usage
-----
ybinlogp [options] binlog-file

Options:
 * `-o OFFSET          Find events after a given offset`
 * `-t TIME            Find events after a given unix timestamp`
 * `-a NUMBER          Print N events after the given one (accepts 'all')`
 * `-q                 Be slightly quieter`
 * `-Q                 Be much quieter`

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
with their MySQL installation. The initial development was done by James Brown <jbrown@yelp.com>;
Evan Klitzke <evan@yelp.com> worked on some bugfixes and a partially-complete Python API, and
Eskil Olsen <eskil@yelp.com> has a branch that does some crazy stuff with Boost.

Contributing
-----------
It's Github... Fork away!
