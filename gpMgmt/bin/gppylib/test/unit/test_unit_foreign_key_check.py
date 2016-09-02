import imp
import os
import sys
from gpcheckcat_modules.foreign_key_check import ForeignKeyCheck

from mock import *

from gp_unittest import *


class GpCheckCatTestCase(GpTestCase):
    def setUp(self):
        self.logger = Mock(spec=['log', 'info', 'debug', 'error'])
        self.db_connection = Mock(spec=['close', 'query'])
        self.autoCast = {'regproc': '::oid',
                         'regprocedure': '::oid',
                         'regoper': '::oid',
                         'regoperator': '::oid',
                         'regclass': '::oid',
                         'regtype': '::oid',
                         'int2vector': '::int2[]'}

        self.subject = ForeignKeyCheck(self.db_connection, self.logger, False, self.autoCast)

        self.full_join_cat_tables = set(['pg_attribute','gp_distribution_policy','pg_appendonly','pg_constraint','pg_index'])
        self.foreign_key_check= Mock(spec=['runCheck'])
        self.foreign_key_check.runCheck.return_value = []
        self.db_connection.query.return_value.ntuples.return_value = 2
        self.db_connection.query.return_value.listfields.return_value = ['pkey1', 'pkey2']
        self.db_connection.query.return_value.getresult.return_value = [('r1','r2'), ('r3','r4')]


    def test_get_fk_query_left_join_returns_the_correct_query(self):
        expected_query = """
              SELECT pkeys-1, pkeys-2, missing_catalog, present_key, pkcatname_pkeystr,
                     array_agg(gp_segment_id order by gp_segment_id) as segids
              FROM (
                    SELECT (case when cat1.fkeystr is not NULL then 'pkcatname' when cat2.pkeystr is not NULL then 'catname' end) as missing_catalog,
                    (case when cat1.fkeystr is not NULL then 'fkeystr' when cat2.pkeystr is not NULL then 'pkeystr' end) as present_key,
                    cat1.gp_segment_id, cat1pkeys-1, cat1pkeys-2, cat1.fkeystr as pkcatname_pkeystr
                    FROM
                        gp_dist_random('catname') cat1 LEFT OUTER JOIN
                        gp_dist_random('pkcatname') cat2
                        ON (cat1.gp_segment_id = cat2.gp_segment_id AND
                            cat1.fkeystr = cat2.pkeystr )
                    WHERE cat2.pkeystr is NULL
                      AND cat1.fkeystr != 0
                    UNION ALL
                    SELECT (case when cat1.fkeystr is not NULL then 'pkcatname' when cat2.pkeystr is not NULL then 'catname' end) as missing_catalog,
                    (case when cat1.fkeystr is not NULL then 'fkeystr' when cat2.pkeystr is not NULL then 'pkeystr' end) as present_key,
                    -1 as gp_segment_id, cat1pkeys-1, cat1pkeys-2, cat1.fkeystr as pkcatname_pkeystr
                    FROM
                        catname cat1 LEFT OUTER JOIN
                        pkcatname cat2
                        ON (cat1.gp_segment_id = cat2.gp_segment_id AND
                            cat1.fkeystr = cat2.pkeystr )
                    WHERE cat2.pkeystr is NULL
                      AND cat1.fkeystr != 0
                    ORDER BY pkeys-1, pkeys-2, gp_segment_id
              ) allresults
              GROUP BY pkeys-1, pkeys-2, pkcatname_pkeystr, missing_catalog, present_key
              """

        result_query = self.subject.get_fk_query_left_join("catname", "pkcatname", "fkeystr", "pkeystr", ["pkeys-1", "pkeys-2"], ["cat1pkeys-1", "cat1pkeys-2"])
        self.assertEquals(expected_query, result_query)

    def test_get_fk_query_full_join_returns_the_correct_query(self):
        expected_query = """
              SELECT pkeys-1, pkeys-2, missing_catalog, present_key, pkcatname_pkeystr,
                     array_agg(gp_segment_id order by gp_segment_id) as segids
              FROM (
                    SELECT (case when cat1.fkeystr is not NULL then 'pkcatname' when cat2.pkeystr is not NULL then 'catname' end) as missing_catalog,
                    (case when cat1.fkeystr is not NULL then 'fkeystr' when cat2.pkeystr is not NULL then 'pkeystr' end) as present_key,
                    COALESCE(cat1.gp_segment_id,cat2.gp_segment_id) as gp_segment_id , cat1pkeys-1, cat1pkeys-2, COALESCE(cat1.fkeystr, cat2.pkeystr) as pkcatname_pkeystr
                    FROM
                        gp_dist_random('catname') cat1 FULL OUTER JOIN
                        gp_dist_random('pkcatname') cat2
                        ON (cat1.gp_segment_id = cat2.gp_segment_id AND
                            cat1.fkeystr = cat2.pkeystr )
                    WHERE (cat2.pkeystr is NULL or cat1.fkeystr is NULL)
                    AND filter
                    UNION ALL
                    SELECT (case when cat1.fkeystr is not NULL then 'pkcatname' when cat2.pkeystr is not NULL then 'catname' end) as missing_catalog,
                    (case when cat1.fkeystr is not NULL then 'fkeystr' when cat2.pkeystr is not NULL then 'pkeystr' end) as present_key,
                    -1, cat1pkeys-1, cat1pkeys-2, COALESCE(cat1.fkeystr, cat2.pkeystr) as pkcatname_pkeystr
                    FROM
                        catname cat1 FULL OUTER JOIN
                        pkcatname cat2
                        ON (cat1.gp_segment_id = cat2.gp_segment_id AND
                            cat1.fkeystr = cat2.pkeystr )
                    WHERE (cat2.pkeystr is NULL or cat1.fkeystr is NULL)
                    AND filter
                    ORDER BY pkeys-1, pkeys-2, gp_segment_id
              ) allresults
              GROUP BY pkeys-1, pkeys-2, pkcatname_pkeystr, missing_catalog, present_key
              """
        result_query = self.subject.get_fk_query_full_join("catname", "pkcatname", "fkeystr", "pkeystr", ["pkeys-1", "pkeys-2"], ["cat1pkeys-1", "cat1pkeys-2"], "filter")
        self.assertEquals(expected_query, result_query)

    @patch('gpcheckcat_modules.foreign_key_check.ForeignKeyCheck.checkTableForeignKey')
    def test_runCheck(self, mock):
        tables = [self._get_mock_for_catalog_table("table1"), self._get_mock_for_catalog_table("table2")]
        self.subject.runCheck(tables)

        self.assertEquals(len(self.subject.checkTableForeignKey.call_args_list), len(tables))

        for table in tables:
            self.assertIn(call(table), self.subject.checkTableForeignKey.call_args_list)

    @patch('gpcheckcat_modules.foreign_key_check.ForeignKeyCheck.get_fk_query_full_join')
    @patch('gpcheckcat_modules.foreign_key_check.ForeignKeyCheck.get_fk_query_left_join')
    @patch('gpcheckcat_modules.foreign_key_check.log_literal')
    def test_checkTableForeignKey__returns_correct_join_query(self, log_literal_mock, fk_query_left_join_mock, fk_query_full_join_mock):
        #cat_tables_to_validate = set(['pg_attribute','gp_distribution_policy','pg_appendonly','pg_constraint','pg_index','pg_type','pg_window'])
        cat_tables_to_validate = set(['pg_attribute', 'pg_appendonly', 'pg_index','pg_type','pg_window'])

        foreign_key_mock_calls = []
        foreign_key_mock_calls_left = []
        for table_name in cat_tables_to_validate:
            foreign_key_mock_1  = self._get_mock_for_foreign_key(pkey_tablename="pg_class", cat_table_name=table_name)
            foreign_key_mock_2 = self._get_mock_for_foreign_key(pkey_tablename="arbitrary_catalog_table", cat_table_name=table_name)
            catalog_table_mock = self._get_mock_for_catalog_table(table_name, [foreign_key_mock_1, foreign_key_mock_2])
            col_type = self._get_col_types(table_name)

            issue_list = self.subject.checkTableForeignKey(catalog_table_mock)

            self.assertEquals(len(issue_list) , 2)
            self.assertEquals(issue_list[0], ('pg_class', ['pkey1', 'pkey2'], [('r1', 'r2'), ('r3', 'r4')]))
            self.assertEquals(issue_list[1], ('arbitrary_catalog_table', ['pkey1', 'pkey2'], [('r1', 'r2'), ('r3', 'r4')]))
            self.assertEquals(self.db_connection.query.call_count, 2)

            def __generate_pg_class_call(table, primary_key_cat_name, col_type, with_filter=True):
                if with_filter:
                    return call(table_name, '%s' % primary_key_cat_name, '%s' % col_type.keys()[0], 'oid',
                                ['%s_pkey1' % (table_name), '%s_pkey2' % (table_name)],
                                filter=self._get_filter(table_name),
                                cat1pkeys=['cat1.pkey1 as %s_pkey1' % (table_name),
                                        'cat1.pkey2 as %s_pkey2' % (table_name)])
                else:
                    return call(table_name, '%s' % primary_key_cat_name, '%s' % col_type.keys()[0], 'oid',
                                ['%s_pkey1' % (table_name), '%s_pkey2' % (table_name)],
                                ['cat1.pkey1 as %s_pkey1' % (table_name),
                                 'cat1.pkey2 as %s_pkey2' % (table_name)])

            # make sure that the correct pg_class_call is used depending on the
            # foreign key
            # XXX: if we know that it's the fake catalog, then we can assume
            # that it will do a left join... we need to figure that out
            if table_name in self.full_join_cat_tables:
                pg_class_call = __generate_pg_class_call(table_name, 'pg_class', col_type, with_filter=True)
                foreign_key_mock_calls.append(pg_class_call)

                pg_class_call = __generate_pg_class_call(table_name, 'arbitrary_catalog_table', col_type, with_filter=False)
                foreign_key_mock_calls_left.append(pg_class_call)
            else:
                pg_class_call = __generate_pg_class_call(table_name, 'pg_class', col_type, with_filter=False)
                foreign_key_mock_calls_left.append(pg_class_call)

            if table_name in self.full_join_cat_tables:
                self.assertEquals(fk_query_full_join_mock.call_count, 1)
                self.assertEquals(fk_query_left_join_mock.call_count, 1)
                fk_query_full_join_mock.assert_has_calls(foreign_key_mock_calls, any_order=False)
            else:
                arbitrary_catalog_table_call = __generate_pg_class_call(table_name, 'arbitrary_catalog_table', col_type, with_filter=False)
                foreign_key_mock_calls_left.append(arbitrary_catalog_table_call)

                self.assertEquals(fk_query_left_join_mock.call_count, 2)
                self.assertEquals(fk_query_full_join_mock.call_count, 0)
                fk_query_left_join_mock.assert_has_calls(foreign_key_mock_calls_left, any_order=False)

            self.db_connection.query.call_count = 0
            fk_query_full_join_mock.call_count = 0
            fk_query_left_join_mock.call_count = 0

    ####################### PRIVATE METHODS #######################
    def _get_filter(self, table_name):
        query_filters = {}
        query_filters['pg_appendonly'] = "(relstorage='a' or relstorage='c')"
        query_filters['pg_attribute'] = "true"
        query_filters['pg_constraint'] = "((relchecks>0 or relhaspkey='t') and relkind = 'r')"
        query_filters['gp_distribution_policy'] = """(relnamespace not in(select oid from pg_namespace where nspname ~ 'pg_')
                                                           and relnamespace not in(select oid from pg_namespace where nspname ~ 'gp_')
                                                           and relnamespace!=(select oid from pg_namespace where nspname='information_schema')
                                                           and relkind='r' and (relstorage='a' or relstorage='h' or relstorage='c'))"""
        query_filters["pg_index"] = "(relkind='i')"
        if table_name in query_filters:
            return query_filters[table_name]
        else:
            return []

    def _get_col_types(self, table_name):
        table_col_types = {'pg_attribute': {'attlen': 'int2', 'atthasdef': 'bool', 'attndims': 'int4',
                                            'attnum': 'int2', 'attname': 'name', 'attalign': 'char',
                                            'attnotnull': 'bool', 'atttypid': 'oid', 'attrelid': 'oid',
                                            'attinhcount': 'int4', 'attcacheoff': 'int4',
                                            'attislocal': 'bool', 'attstattarget': 'int4',
                                            'attstorage': 'char', 'attbyval': 'bool',
                                            'atttypmod': 'int4', 'attisdropped': 'bool'},
                           'pg_appendonly': {'relid': 'oid'},
                           'pg_attribute': {'attrelid': 'oid'},
                           'pg_constraint': {'conrelid': 'oid'},
                           'pg_index': {'indexrelid': 'oid'},
                           'gp_distribution_policy': {}
                          }

        if table_name not in table_col_types.keys():
            table_name = 'pg_attribute'

        return table_col_types[table_name]

    def _get_mock_for_catalog_table(self, table_name, foreign_keys=None):
        catalog_table_mock = Mock(spec=['getTableName','isShared','getForeignKeys','getPrimaryKey','getTableColtypes'])

        catalog_table_mock.getTableName.return_value = table_name
        catalog_table_mock.isShared.return_value = True
        catalog_table_mock.getForeignKeys.return_value = foreign_keys
        catalog_table_mock.getPrimaryKey.return_value = ["pkey1", "pkey2"]
        catalog_table_mock.getTableColtypes.return_value = self._get_col_types(table_name)

        return catalog_table_mock

    def _get_mock_for_foreign_key(self, pkey_tablename, cat_table_name):

        spec_list = ['getPKey', 'getPkeyTableName', 'getColumns']

        filter_mapping = {'pg_appendonly': ['relid'],
                          'pg_attribute': ['attrelid'],
                          'pg_constraint': ['conrelid'],
                          'pg_index': ['indexrelid'],
                          'gp_distribution_policy': []
                          }

        attribute_foreign_key_mock = Mock(spec=spec_list)
        attribute_foreign_key_mock.getPKey.return_value = ['oid']
        attribute_foreign_key_mock.getPkeyTableName.return_value = pkey_tablename
        query_filter = ['attrelid']
        if cat_table_name in filter_mapping:
            query_filter =  filter_mapping[cat_table_name]
        attribute_foreign_key_mock.getColumns.return_value = query_filter

        return attribute_foreign_key_mock


if __name__ == '__main__':
    run_tests()
