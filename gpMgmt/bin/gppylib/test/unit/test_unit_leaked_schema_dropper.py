from mock import *

from .gp_unittest import *
from gpcheckcat_modules.leaked_schema_dropper import LeakedSchemaDropper


class LeakedSchemaDropperTestCase(GpTestCase):
    def setUp(self):
        self.db_connection = Mock(spec=['cursor'])
        self.db_connection.cursor.return_value.fetchall.return_value = [
            ('fake_leak_1', 'something_else'),
            ('some"test"special_#;character--schema', 'something_else')
        ]
        self.subject = LeakedSchemaDropper()

    def test_drop_leaked_schemas__returns_a_list_of_leaked_schemas(self):
        self.db_connection.cursor.return_value = Mock()
        self.db_connection.cursor.return_value.__enter__ = Mock(return_value=Mock(spec=['fetchall', 'execute']))
        self.db_connection.cursor.return_value.__exit__ = Mock(return_value=False)
        self.db_connection.cursor.return_value.__enter__.return_value.fetchall.return_value = [
            ('fake_leak_1', 'something_else'),
            ('some"test"special_#;character--schema', 'something_else')
        ]
        self.assertEqual(self.subject.drop_leaked_schemas(self.db_connection), ['fake_leak_1', 'some"test"special_#;character--schema'])

    def test_drop_leaked_schemas__when_there_are_no_leaked_schemas__returns_an_empty_list(self):
        self.db_connection.cursor.return_value = Mock()
        self.db_connection.cursor.return_value.__enter__ = Mock(return_value=Mock(spec=['fetchall', 'execute']))
        self.db_connection.cursor.return_value.__exit__ = Mock(return_value=False)
        self.db_connection.cursor.return_value.__enter__.return_value.fetchall.return_value = []
        self.assertEqual(self.subject.drop_leaked_schemas(self.db_connection), [])

    def test_drop_leaked_schemas__when_query_returns_null_schema__returns_an_empty_list(self):
        self.db_connection.cursor.return_value = Mock()
        self.db_connection.cursor.return_value.__enter__ = Mock(return_value=Mock(spec=['fetchall', 'execute']))
        self.db_connection.cursor.return_value.__exit__ = Mock(return_value=False)
        self.db_connection.cursor.return_value.__enter__.return_value.fetchall.return_value = [(None, 'something_else')]
        self.assertEqual(self.subject.drop_leaked_schemas(self.db_connection), [])

    def test_drop_leaked_schemas__when_query_returns_null__returns_an_empty_list(self):
        self.db_connection.cursor.return_value = Mock()
        self.db_connection.cursor.return_value.__enter__ = Mock(return_value=Mock(spec=['fetchall', 'execute']))
        self.db_connection.cursor.return_value.__exit__ = Mock(return_value=False)
        self.db_connection.cursor.return_value.__enter__.return_value.fetchall.return_value = []
        self.assertEqual(self.subject.drop_leaked_schemas(self.db_connection), [])

    def test_drop_leaked_schemas__drops_orphaned_and_leaked_schemas(self):
        self.db_connection.cursor.return_value = Mock()
        self.db_connection.cursor.return_value.__enter__ = Mock(return_value=Mock(spec=['fetchall', 'execute']))
        self.db_connection.cursor.return_value.__exit__ = Mock(return_value=False)
        self.db_connection.cursor.return_value.__enter__.return_value.fetchall.return_value = [
            ('fake_leak_1', 'something_else'),
            ('some"test"special_#;character--schema', 'something_else')
        ]
        self.subject.drop_leaked_schemas(self.db_connection)

        drop_query_expected_list = [call("DROP SCHEMA IF EXISTS \"fake_leak_1\" CASCADE;"),
                                    call("DROP SCHEMA IF EXISTS \"some\"\"test\"\"special_#;character--schema\" CASCADE;")]
        self.db_connection.cursor.return_value.__enter__.return_value.execute.assert_has_calls(drop_query_expected_list)


if __name__ == '__main__':
    run_tests()
