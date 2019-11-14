from mock import *
import os
from gp_unittest import *
from gpcheckcat_modules.repair import Repair
import tempfile
import shutil

class RepairTestCase(GpTestCase):
    def setUp(self):
        self.repair_dir_path = tempfile.mkdtemp()
        self.context = Mock()
        self.context.timestamp = 'timestamp'
        self.context.opt = dict()
        self.context.opt["-g"] = self.repair_dir_path
        self.context.report_cfg = {0: {'segname': 'seg1', 'hostname': 'somehost', 'port': 25432},
                                   1: {'segname': 'seg2', 'hostname': 'somehost', 'port': 25433},
                                   2: {'segname': 'seg3', 'hostname': 'somehost', 'port': 25434},
                                   -1: {'segname': 'master', 'hostname': 'somehost', 'port': 15432}}
        self.context.cfg = {0: {'content': 0, 'dbid': 0, 'segname': 'seg1', 'hostname': 'somehost', 'port': 25432},
                            1: {'content': 1, 'dbid': 1, 'segname': 'seg2', 'hostname': 'somehost', 'port': 25433},
                            2: {'content': 2, 'dbid': 2, 'segname': 'seg3', 'hostname': 'somehost', 'port': 25434},
                            3: {'content': -1, 'dbid': 3, 'segname': 'master', 'hostname': 'somehost', 'port': 15432}}
        self.context.dbname = "somedb"

        self.subject = Repair(self.context, "issuetype", "some desc")
        self.repair_sql_contents = ["some sql1", "some sql2"]

    def test_create_repair__normal(self):
        repair_dir = self.subject.create_repair(self.repair_sql_contents)
        self.assertEqual(repair_dir, self.repair_dir_path)
        sql_contents = ["some sql1\n",
                        "some sql2\n"]

        bash_contents = ['#!/bin/bash\n',
                         'set -o errexit\n',
                         'cd $(dirname $0)\n',
                         '\n',
                         'echo "some desc"\n',
                         'psql -X -v ON_ERROR_STOP=1 -a -h somehost -p 15432 -f "somedb_issuetype_timestamp.sql" '
                         '"somedb" >> somedb_issuetype_timestamp.out 2>&1\n']

        self.verify_repair_dir_contents("somedb_issuetype_timestamp.sql", sql_contents)
        self.verify_repair_dir_contents("runsql_timestamp.sh", bash_contents)

    @patch('gpcheckcat_modules.repair.RepairMissingExtraneous', autospec=True)
    def test_create_repair_extra__normal(self, mock_repair):
        self.subject = Repair(self.context, "extra", "some desc")
        catalog_table_obj = Mock()
        catalog_table_name = "catalog"
        catalog_table_obj.getTableName.return_value = catalog_table_name

        # Mock out RepairMissingExtraneous. Set up the dbid-to-OID map, and
        # return a hardcoded string for the DELETE SQL.
        extra_missing_repair_obj = Mock(spec=['get_segment_to_oid_mapping', 'get_delete_sql'])
        extra_missing_repair_obj.get_segment_to_oid_mapping.return_value = {
            -1: set([49401, 49402]),
            0: set([49403]),
            1: set([49403, 49404])
        }
        extra_missing_repair_obj.get_delete_sql.return_value = "delete_sql"
        mock_repair.return_value = extra_missing_repair_obj

        repair_dir = self.subject.create_repair_for_extra_missing(catalog_table_obj, issues=None, pk_name=None, segments=self.context.cfg)
        self.assertEqual(repair_dir, self.repair_dir_path)
        bash_contents =[
                    "#!/bin/bash\n",
                    "set -o errexit\n",
                    "cd $(dirname $0)\n",
                    "\n",
                    "echo \"some desc\"\n",
                    "PGOPTIONS='-c gp_role=utility' psql -X -v ON_ERROR_STOP=1 -a -h somehost -p 25432 -c \"delete_sql\" \"somedb\" >> somedb_extra_timestamp.out 2>&1\n",
                    "\n",
                    "echo \"some desc\"\n",
                    "PGOPTIONS='-c gp_role=utility' psql -X -v ON_ERROR_STOP=1 -a -h somehost -p 25433 -c \"delete_sql\" \"somedb\" >> somedb_extra_timestamp.out 2>&1\n",
                    "\n",
                    "echo \"some desc\"\n",
                    "psql -X -v ON_ERROR_STOP=1 -a -h somehost -p 15432 -c \"delete_sql\" \"somedb\" >> somedb_extra_timestamp.out 2>&1\n"]

        self.verify_repair_dir_contents("run_somedb_extra_{}_timestamp.sh".format(catalog_table_name),
                                        bash_contents)

    def test_create_segment_repair_scripts(self):
        self.subject = Repair(self.context, "orphan_toast_tables", "some desc")

        segments_with_repair_statements = []
        for segment in self.context.cfg.values():
            if segment['content'] != -1:
                segment['repair_statements'] = ['UPDATE pg_class']
                segments_with_repair_statements.append(segment)

        repair_dir = self.subject.create_segment_repair_scripts(segments=segments_with_repair_statements)
        self.assertEqual(repair_dir, self.repair_dir_path)
        bash_contents =[
            "#!/bin/bash\n",
            "set -o errexit\n",
            "cd $(dirname $0)\n",
            "\n",
            "echo \"some desc\"\n",
            "PGOPTIONS='-c gp_role=utility' psql -X -v ON_ERROR_STOP=1 -a -h somehost -p 25432 -f \"0.somehost.25432.somedb.timestamp.sql\" \"somedb\" >> somedb_orphan_toast_tables_timestamp.out 2>&1\n",
            "\n",
            "echo \"some desc\"\n",
            "PGOPTIONS='-c gp_role=utility' psql -X -v ON_ERROR_STOP=1 -a -h somehost -p 25433 -f \"1.somehost.25433.somedb.timestamp.sql\" \"somedb\" >> somedb_orphan_toast_tables_timestamp.out 2>&1\n",
            "\n",
            "echo \"some desc\"\n",
            "PGOPTIONS='-c gp_role=utility' psql -X -v ON_ERROR_STOP=1 -a -h somehost -p 25434 -f \"2.somehost.25434.somedb.timestamp.sql\" \"somedb\" >> somedb_orphan_toast_tables_timestamp.out 2>&1\n"]

        self.verify_repair_dir_contents("runsql_timestamp.sh", bash_contents)


    def test_append_content_to_bash_script__with_catalog_table(self):
        catalog_table_name = "catalog"
        script = "some script here"
        self.subject.append_content_to_bash_script(self.repair_dir_path, script, catalog_table_name)
        bash_contents =[
            "#!/bin/bash\n",
            "set -o errexit\n",
            "cd $(dirname $0)\n",
            "some script here"]

        self.verify_repair_dir_contents("run_somedb_issuetype_{}_timestamp.sh".format(catalog_table_name),
                                        bash_contents)

    def test_append_content_to_bash_script__without_catalog_table(self):
        script = "some script here"
        self.subject.append_content_to_bash_script(self.repair_dir_path, script)
        bash_contents =[
            "#!/bin/bash\n",
            "set -o errexit\n",
            "cd $(dirname $0)\n",
            "some script here"]

        self.verify_repair_dir_contents("runsql_timestamp.sh", bash_contents)

    def verify_repair_dir_contents(self, repair_script, contents):
        file_path = os.path.join(self.repair_dir_path, repair_script)
        with open(file_path) as f:
            file_contents = f.readlines()

        self.assertEqual(file_contents, contents)

    def tearDown(self):
        shutil.rmtree(self.repair_dir_path)
        super(RepairTestCase, self).tearDown()

if __name__ == '__main__':
    run_tests()
