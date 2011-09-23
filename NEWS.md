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
