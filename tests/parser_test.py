import tempfile

import mock
from testify import TestCase, setup, assert_equal

from ybinlogp.parser import YBinlogP, EventType


class YBinlogPTestCase(TestCase):

	@setup
	def setup_parser(self):
		self.binlog_file = tempfile.NamedTemporaryFile()
		self.parser = YBinlogP(self.binlog_file.name)


