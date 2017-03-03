from mock import *
from gp_unittest import *
from gpcheckcat_modules.repair_missing_extraneous import RepairMissingExtraneous

class RepairMissingExtraneousTestCase(GpTestCase):
    def setUp(self):
        self.all_seg_ids = [-1,0,1,2,3]

        self.table_name = 'pg_attribut"e'
        self.catalog_table_obj = Mock(spec=['getTableName',
                                            'tableHasConsistentOids',
                                            'getPrimaryKey'])
        self.catalog_table_obj.getTableName.return_value = self.table_name

    def test_get_segment_to_oid_mapping_with_both_extra_and_missing(self):
        issues = [(49401, "extra", '{1,2}'),
                  (49401, "extra", '{1,2}'),
                  (49402, "missing", '{2,3}'),
                  (49403, "extra", '{2,3}'),
                  (49404, "missing", '{1}'),
                  (49405, "extra", '{2,3}'),
                  (49406, "missing", '{2}')]

        self.subject = RepairMissingExtraneous(self.catalog_table_obj, issues, "attrelid")
        repair_sql_contents = self.subject.get_segment_to_oid_mapping(self.all_seg_ids)

        self.assertEqual(len(repair_sql_contents), 5)
        self.assertEqual(repair_sql_contents[-1], set([49402, 49404, 49406]))
        self.assertEqual(repair_sql_contents[0], set([49402, 49404, 49406]))
        self.assertEqual(repair_sql_contents[1], set([49401, 49402,  49406]))
        self.assertEqual(repair_sql_contents[2], set([49401, 49403, 49404, 49405]))
        self.assertEqual(repair_sql_contents[3], set([49403, 49404, 49405, 49406]))

    def test_get_segment_to_oid_mapping_with_only_extra(self):
        issues = [(49401, 'cmax', "extra", '{1,2}'),
                  (49401, 'cmax', "extra", '{1,2}'),
                  (49403, 'cmax', "extra", '{2,3}'),
                  (49405, 'cmax', "extra", '{2,3}')]

        self.subject = RepairMissingExtraneous(self.catalog_table_obj, issues, "attrelid")
        repair_sql_contents = self.subject.get_segment_to_oid_mapping(self.all_seg_ids)

        self.assertEqual(len(repair_sql_contents), 3)
        self.assertEqual(repair_sql_contents[1], set([49401]))
        self.assertEqual(repair_sql_contents[2], set([49401, 49403, 49405]))
        self.assertEqual(repair_sql_contents[3], set([49403, 49405]))

    def test_get_segment_to_oid_mapping_with_only_missing(self):
        issues = [(49401, 'cmax', "missing", '{1,2}'),
                  (49401, 'cmax', "missing", '{1,2}'),
                  (49403, 'cmax', "missing", '{2,3}'),
                  (49405, 'cmax', "missing", '{2,3}')]

        self.subject = RepairMissingExtraneous(self.catalog_table_obj, issues, "attrelid")
        repair_sql_contents = self.subject.get_segment_to_oid_mapping(self.all_seg_ids)

        self.assertEqual(len(repair_sql_contents), 4)
        self.assertEqual(repair_sql_contents[-1], set([49401, 49403, 49405]))
        self.assertEqual(repair_sql_contents[0], set([49401, 49403, 49405]))
        self.assertEqual(repair_sql_contents[1], set([49403, 49405]))
        self.assertEqual(repair_sql_contents[3], set([49401]))

    def test_get_delete_sql__with_multiple_oids(self):
        self.subject = RepairMissingExtraneous(self.catalog_table_obj, None, "attrelid")
        oids = [1,3,4]
        delete_sql = self.subject.get_delete_sql(oids)
        self.assertEqual(delete_sql, 'BEGIN;set allow_system_table_mods="dml";'
                                      'delete from "pg_attribut""e" where "attrelid" in (1,3,4);COMMIT;')

    def test_get_delete_sql__with_one_oid(self):
        self.subject = RepairMissingExtraneous(self.catalog_table_obj, None, "attrelid")
        oids = [5]
        delete_sql = self.subject.get_delete_sql(oids)
        self.assertEqual(delete_sql, 'BEGIN;set allow_system_table_mods="dml";'
                                     'delete from "pg_attribut""e" where "attrelid" in (5);COMMIT;')

    def test_get_delete_sql__with_one_pkey_one_issue(self):
        issues = [('!!', 'cmax', "extra", '{1,2}'),]
        self.catalog_table_obj.tableHasConsistentOids.return_value = False
        self.catalog_table_obj.getPrimaryKey.return_value = ["oprname"]
        self.subject = RepairMissingExtraneous(self.catalog_table_obj, issues, None)
        oids = None
        delete_sql = self.subject.get_delete_sql(oids)
        self.assertEqual(delete_sql, 'BEGIN;set allow_system_table_mods="dml";'
                                     'delete from "pg_attribut""e" where oprname = \'!!\';COMMIT;')

    def test_get_delete_sql__with_one_pkey_mult_issues(self):
        issues = [('!!', 'cmax', "missing", '{1,2}'),
                  ('8!', 'cmax', "extra", '{1,2}'),
                  ('*!', 'cmax', "missing", '{2,3}'),
                  ('!!', 'cmax', "extra", '{2,3}')]
        self.catalog_table_obj.tableHasConsistentOids.return_value = False
        self.catalog_table_obj.getPrimaryKey.return_value = ["oprname"]
        self.subject = RepairMissingExtraneous(self.catalog_table_obj, issues, None)
        oids = None
        delete_sql = self.subject.get_delete_sql(oids)
        self.assertEqual(delete_sql, 'BEGIN;set allow_system_table_mods="dml";'
                                     'delete from "pg_attribut""e" where oprname = \'!!\';'
                                     'delete from "pg_attribut""e" where oprname = \'8!\';'
                                     'delete from "pg_attribut""e" where oprname = \'*!\';'
                                     'delete from "pg_attribut""e" where oprname = \'!!\';COMMIT;')

    def test_get_delete_sql__with_multiple_pkey_mult_issue(self):
        issues = [('!!', 48920, 0, 1, 'cmax', "missing", '{1,2}'),
                  ('8!', 15, 1, 3, 'cmax', "extra", '{1,2}'),
                  ('*!', 48920, 2, 3, 'cmax', "missing", '{2,3}'),
                  ('!!', 11, 2, 3, 'cmax', "extra", '{2,3}')]
        self.catalog_table_obj.tableHasConsistentOids.return_value = False
        self.catalog_table_obj.getPrimaryKey.return_value = ["oprname",
                                                             "oprnamespace",
                                                             "oprleft",
                                                             "oprright"]
        self.subject = RepairMissingExtraneous(self.catalog_table_obj, issues, None)
        oids = None
        delete_sql = self.subject.get_delete_sql(oids)
        self.assertEqual(delete_sql, 'BEGIN;set allow_system_table_mods="dml";'
                                     'delete from "pg_attribut""e" where oprname = \'!!\' and oprnamespace = \'48920\''
                                                                         ' and oprleft = \'0\' and oprright = \'1\';'
                                     'delete from "pg_attribut""e" where oprname = \'8!\' and oprnamespace = \'15\''
                                                                         ' and oprleft = \'1\' and oprright = \'3\';'
                                     'delete from "pg_attribut""e" where oprname = \'*!\' and oprnamespace = \'48920\''
                                                                         ' and oprleft = \'2\' and oprright = \'3\';'
                                     'delete from "pg_attribut""e" where oprname = \'!!\' and oprnamespace = \'11\''
                                                                         ' and oprleft = \'2\' and oprright = \'3\';'
                                     'COMMIT;')


if __name__ == '__main__':
    run_tests()
