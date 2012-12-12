0.5.8.4
-------
* More bugfixes when attempting to use ybinlogp to "tail" a log

0.5.8.3
-------
* Minor bugfix in Python bindings

0.5.8.2
------
* Limit the number of retries in a row

0.5.8.1
-------
* Minor bugfix

0.5.8
-----
* Add some retry logic around reading the binlog

0.5.7
-----
* Change Makefile ordering

0.5.6
----
* misc bugs

0.5.5
-----
* 0.5.4 lost some commas, sorry

0.5.4
-----
* Some code cleanup
* Fixes a bug where the library might return a partial event
* Improves the python example script
* Adds a TODO file

0.5.3
-----
* indentation bug

0.5.2
-----
* Fix a memory leak. Thanks to Evan Klitzke <evan@eklitzke.org> for pointing it
  out.

0.5.1
-----
* Remove a print statement that snuck into the python bindings

0.5.0
-----
Big update!

* Adds python bindings with ctypes!
* Refactors into a library!
* A new and differently-terrible Makefile!

0.3.1
-----
* Bump NEWS.md
* Remove debian packaging (maintaining internally only now)

0.3
---
* Adds Server-ID to the match heuristics (might break if you reparent
  without doing a `FLUSH LOGS`, use `-S` to disable
* Supports parsing the status variables in the binlog (which, as far as
  I can tell, `mysqlbinlog` doesn't do correctly
* Lots of bugfixes

0.2
---
* Fixes a bug in 0.1 which caused an infinite loop if a binlog ended with a
  `STOP_EVENT` instead of a `ROTATE_EVENT`
* Added `-Q` mode to only print queries
* Added `-D` option to limit query printing to specific databases
* Added `-v` option to be more verbose

0.1
---
* Initial release. Includes debian packaging.
