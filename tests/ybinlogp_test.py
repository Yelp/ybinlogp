from testify import TestCase, setup, assert_equal

from ybinlogp import YBinlogP


class YBinlogPTestCase(TestCase):

	@setup
	def setup_parser(self):
		self.filename = 'the_filename'
		self.parser = YBinlogP(self.filename)

	def test__init__(self):
		pass

	def test_what(self):
		pass
