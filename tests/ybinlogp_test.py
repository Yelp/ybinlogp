import datetime

from testify import TestCase, setup, assert_equal

from ybinlogp import YBinlogP, EventType


class YBinlogPAcceptanceTestCase(TestCase):

	_suites = ['acceptance']

	def test_default_path(self):
		filename = 'testing/data/mysql-bin.default-path'
		parser = YBinlogP(filename)
		events = list(parser)
		assert_equal(len(events), 38)

		last_event = events[-1]
		assert_equal(last_event.event_type, EventType.rotate)
		assert_equal(last_event.data.file_name, 'mysql-bin.000008')
		assert_equal(last_event.data.next_position, 4)

		last_commit = events[-2]
		assert_equal(last_commit.event_type, EventType.xid)

		last_query = events[-3]
		assert_equal(last_query.event_type, EventType.query)
		assert_equal(last_query.data.db_name, 'foobar')
		assert_equal(last_query.data.statement,
				'INSERT INTO test2(x) VALUES("Bananas r good")')

	def test_with_delayed_statement(self):
		filename = 'testing/data/mysql-bin.delayed-event'
		parser = YBinlogP(filename)
		events = list(parser)
		assert_equal(len(events), 38)
		# Event has a timestamp way in the past relative to FDE
		assert_equal(events[30].time, datetime.datetime(2013, 07, 30, 10, 2, 37))

