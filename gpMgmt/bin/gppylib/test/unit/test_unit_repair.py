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
        self.context.opt = dict()
        self.context.opt["-g"] = self.repair_dir_path
        self.context.report_cfg = {0: {'segname': 'seg1', 'hostname': 'somehost', 'port': 25432},
                                   1: {'segname': 'seg2', 'hostname': 'somehost', 'port': 25433},
                                   2: {'segname': 'seg3', 'hostname': 'somehost', 'port': 25434},
                                   -1: {'segname': 'master', 'hostname': 'somehost', 'port': 15432}}
        self.context.dbname = "somedb"

        self.subject = Repair(self.context, "issuetype", "some desc")
        self.subject.TIMESTAMP = "timestamp"
        self.subject._repair_script = "runsql_timestamp.sh"
        self.repair_sql_contents = ["some sql1", "some sql2"]
        extra_missing_repair_obj = Mock(spec=['get_segment_to_oid_mapping', 'get_delete_sql'])
        extra_missing_repair_obj.get_segment_to_oid_mapping.return_value = {
            -1: set([49401, 49402]),
             0: set([49403]),
             1: set([49403, 49404])
        }

        extra_missing_repair_obj.get_delete_sql.return_value = "delete_sql"
        self.apply_patches([
            patch("gpcheckcat_modules.repair.RepairMissingExtraneous", return_value=extra_missing_repair_obj)
        ])

    def test_create_repair__normal(self):
        repair_dir = self.subject.create_repair(self.repair_sql_contents)
        self.assertEqual(repair_dir, self.repair_dir_path)
        sql_contents = ["some sql1\n",
                        "some sql2\n"]

        bash_contents = ['#!/bin/bash\n',
                         'cd $(dirname $0)\n',
                         '\n',
                         'echo "some desc"\n',
                         'psql -X -a -h somehost -p 15432 -f "somedb_issuetype_timestamp.sql" '
                         '"somedb" >> somedb_issuetype_timestamp.out 2>&1\n']

        self.verify_repair_dir_contents(os.path.join(repair_dir, "somedb_issuetype_timestamp.sql"), sql_contents)
        self.verify_repair_dir_contents(os.path.join(repair_dir, "runsql_timestamp.sh"), bash_contents)

    def test_create_repair_extra__normal(self):
        self.subject = Repair(self.context, "extra", "some desc")
        self.subject.TIMESTAMP = "timestamp"
        catalog_table_obj = Mock()
        catalog_table_name = "catalog"
        catalog_table_obj.getTableName.return_value = catalog_table_name
        repair_dir = self.subject.create_repair_for_extra_missing(catalog_table_obj, issues=None, pk_name=None)
        self.assertEqual(repair_dir, self.repair_dir_path)
        bash_contents =[
                    "#!/bin/bash\n",
                    "cd $(dirname $0)\n",
                    "\n",
                    "echo \"some desc\"\n",
                    "PGOPTIONS='-c gp_session_role=utility' psql -X -a -h somehost -p 25432 -c \"delete_sql\" \"somedb\" >> somedb_extra_timestamp.out 2>&1\n",
                    "\n",
                    "echo \"some desc\"\n",
                    "PGOPTIONS='-c gp_session_role=utility' psql -X -a -h somehost -p 25433 -c \"delete_sql\" \"somedb\" >> somedb_extra_timestamp.out 2>&1\n",
                    "\n",
                    "echo \"some desc\"\n",
                    "psql -X -a -h somehost -p 15432 -c \"delete_sql\" \"somedb\" >> somedb_extra_timestamp.out 2>&1\n"]

        self.verify_repair_dir_contents(os.path.join(repair_dir, "run_somedb_extra_{0}_timestamp.sh".
                                                         format(catalog_table_name)), bash_contents)

    def test_append_content_to_bash_script__with_catalog_table(self):
        catalog_table_name = "catalog"
        script = "some script here"
        self.subject.append_content_to_bash_script(self.repair_dir_path, script, catalog_table_name)
        bash_contents =[
            "#!/bin/bash\n",
            "cd $(dirname $0)\n",
            "some script here"]

        self.verify_repair_dir_contents(os.path.join(self.repair_dir_path, "run_somedb_issuetype_catalog_timestamp.sh".
                                                     format(catalog_table_name)), bash_contents)

    def test_append_content_to_bash_script__without_catalog_table(self):
        script = "some script here"
        self.subject.append_content_to_bash_script(self.repair_dir_path, script)
        bash_contents =[
            "#!/bin/bash\n",
            "cd $(dirname $0)\n",
            "some script here"]

        self.verify_repair_dir_contents(os.path.join(self.repair_dir_path, "runsql_timestamp.sh"), bash_contents)

    def verify_repair_dir_contents(self, file_path, contents):
        with open(file_path) as f:
            file_contents = f.readlines()

        self.assertEqual(file_contents, contents)

    def tearDown(self):
        shutil.rmtree(self.repair_dir_path)
        super(RepairTestCase, self).tearDown()

if __name__ == '__main__':
    run_tests()
