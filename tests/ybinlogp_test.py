import tempfile

import mock
from testify import TestCase, setup, assert_equal

from ybinlogp import YBinlogP


class YBinlogPTestCase(TestCase):

	@setup
	def setup_parser(self):
		self.binlog_file = tempfile.NamedTemporaryFile()
		self.parser = YBinlogP(self.binlog_file.name)

	def test__init__(self):
		pass

	def test_what(self):
		pass
