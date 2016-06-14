from mock import *

from gp_unittest import *
from gpcheckcat_modules.leaked_schema_dropper import LeakedSchemaDropper


class LeakedSchemaDropperTestCase(GpTestCase):
    def setUp(self):
        self.db_connection = Mock(spec=['query'])

        two_leaked_schemas = Mock()
        two_leaked_schemas.getresult.return_value = [
            ('fake_leak_1', 'something_else'),
            ('some"test"special_#;character--schema', 'something_else')
        ]
        self.db_connection.query.return_value = two_leaked_schemas

        self.subject = LeakedSchemaDropper()

    def test_drop_leaked_schemas__returns_a_list_of_leaked_schemas(self):
        self.assertEqual(self.subject.drop_leaked_schemas(self.db_connection), ['fake_leak_1', 'some"test"special_#;character--schema'])

    def test_drop_leaked_schemas__when_there_are_no_leaked_schemas__returns_an_empty_list(self):
        no_leaked_schemas = Mock()
        no_leaked_schemas.getresult.return_value = []
        self.db_connection.query.return_value = no_leaked_schemas

        self.assertEqual(self.subject.drop_leaked_schemas(self.db_connection), [])

    def test_drop_leaked_schemas__when_query_returns_null_schema__returns_an_empty_list(self):
        null_leaked_schema = Mock()
        null_leaked_schema.getresult.return_value = [(None, 'something_else')]
        self.db_connection.query.return_value = null_leaked_schema

        self.assertEqual(self.subject.drop_leaked_schemas(self.db_connection), [])

    def test_drop_leaked_schemas__when_query_returns_null__returns_an_empty_list(self):
        self.db_connection.query.return_value = None

        self.assertEqual(self.subject.drop_leaked_schemas(self.db_connection), [])

    def test_drop_leaked_schemas__drops_orphaned_and_leaked_schemas(self):
        self.subject.drop_leaked_schemas(self.db_connection)

        drop_query_expected_list = [call("DROP SCHEMA IF EXISTS \"fake_leak_1\" CASCADE;"),
                                    call("DROP SCHEMA IF EXISTS \"some\"\"test\"\"special_#;character--schema\" CASCADE;")]
        self.db_connection.query.assert_has_calls(drop_query_expected_list)


if __name__ == '__main__':
    run_tests()
