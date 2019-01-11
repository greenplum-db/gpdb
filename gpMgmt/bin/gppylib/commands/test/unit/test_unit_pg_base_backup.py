import unittest
from gppylib.commands import pg


class TestUnitPgBaseBackup(unittest.TestCase):
    def test_replication_slot_not_passed_when_not_given_slot_name(self):
        base_backup = pg.PgBaseBackup(
            replication_slot_name = None,
            pgdata = "foo",
            host = "bar",
            port = "baz",
            )

        tokens = base_backup.command_tokens

        self.assertIn("--xlog", tokens)
        self.assertNotIn("--slot", tokens)
        self.assertNotIn("some-replication-slot-name", tokens)
        self.assertNotIn("--xlog-method", tokens)
        self.assertNotIn("stream", tokens)

    def test_base_backup_passes_parameters_necessary_to_create_replication_slot_when_given_slotname(self):
        base_backup = pg.PgBaseBackup(
            replication_slot_name = 'some-replication-slot-name',
            pgdata = "foo",
            host = "bar",
            port = "baz",
            )

        self.assertIn("--slot", base_backup.command_tokens)
        self.assertIn("some-replication-slot-name", base_backup.command_tokens)
        self.assertIn("--xlog-method", base_backup.command_tokens)
        self.assertIn("stream", base_backup.command_tokens)

    def test_base_backup_does_not_pass_conflicting_xlog_method_argument_when_given_replication_slot(self):
        base_backup = pg.PgBaseBackup(
            replication_slot_name = 'some-replication-slot-name',
            pgdata = "foo",
            host = "bar",
            port = "baz",
            )
        self.assertNotIn("-x", base_backup.command_tokens)
        self.assertNotIn("--xlog", base_backup.command_tokens)


if __name__ == '__main__':
    unittest.main()
