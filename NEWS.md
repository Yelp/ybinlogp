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
