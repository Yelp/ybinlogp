#!/usr/bin/env python

from distutils.core import setup, Extension

__version__ = '0.1.0'

#define_macros = [('MODULE_VERSION', '"%s"' % __version__), ('DEBUG', None)]
define_macros = [('MODULE_VERSION', '"%s"' % __version__)]

binlog_extension = Extension(
	name='binlog',
	sources=['ybinlogp.c', 'pybinlog.c'],
	define_macros=define_macros)

setup(
	name='binlog',
	version=__version__,
	author='Evan Klitzke',
	author_email='evan@eklitzke.org',
	description='MySQL binlgo parser',
	ext_modules=[binlog_extension])
