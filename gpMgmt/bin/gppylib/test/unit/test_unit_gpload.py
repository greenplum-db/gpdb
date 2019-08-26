#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2013. All Rights Reserved. 
#
""" 
Unit testing for gpload module
"""
import unittest
import os

from gpload import *
from mock import Mock

class GpLoadTestCase(unittest.TestCase):
    """
    Test class for gpload module.
    """

    def setUp(self):
        self.begin = False
        self.commit = False

    def mockQuery(self, sql):
        if sql.lower() == 'begin':
            self.begin = True
        elif sql.lower() == 'commit':
            self.commit = True

    def mockDoNothing(self):
        pass

    def help_test_with_config(self, gpload_param, expected_begin_value, expected_commit_value):
        print gpload_param
        gploader = gpload(gpload_param)
        gploader.read_config()
        gploader.db = self
        gploader.db.query = Mock(side_effect=self.mockQuery)
        gploader.do_method_merge = Mock(side_effect=self.mockDoNothing)
        gploader.do_method_update = Mock(side_effect=self.mockDoNothing)
        gploader.do_method_insert = Mock(side_effect=self.mockDoNothing)
        gploader.do_method()
        self.assertEqual(self.begin, expected_begin_value)
        self.assertEqual(self.commit, expected_commit_value)

    def test_reuse_exttbl_with_logerrors_and_limit(self):
        gploader = gpload(['-f', os.path.join(os.path.dirname(__file__), 'gpload_merge.yml')])
        gploader.read_config()
        gploader.locations = [ 'gpfdist://localhost:8080/test' ]
        rejectLimit = '9999'
        gploader.gpdb_version = "6.0.0"

        sql = gploader.get_reuse_exttable_query('csv', 'header', None, {}, None, False, None)
        self.assertTrue('NOT pgext.logerrors' in sql)
        self.assertTrue('pgext.rejectlimit IS NULL' in sql)

    def test_case_merge_transaction(self):
        self.help_test_with_config(['-f', os.path.join(os.path.dirname(__file__), 'gpload_merge.yml')],
                              True,
                              True)

    def test_case_merge_transaction_t(self):
        self.help_test_with_config(['-f', os.path.join(os.path.dirname(__file__), 'gpload_merge.yml')],
                              True,
                              True)

    def test_case_merge_without_transaction(self):
        self.help_test_with_config(['--no_auto_trans', '-f', os.path.join(os.path.dirname(__file__), 'gpload_merge.yml')],
                              False,
                              False)

    def test_case_insert_transaction(self):
        self.help_test_with_config(['-f', os.path.join(os.path.dirname(__file__), 'gpload_insert.yml')],
                              False,
                              False)

    def test_case_insert_transaction_t(self):
        self.help_test_with_config(['-f', os.path.join(os.path.dirname(__file__), 'gpload_insert.yml')],
                              False,
                              False)

    def test_case_insert_without_transaction(self):
        self.help_test_with_config(['--no_auto_trans', '-f', os.path.join(os.path.dirname(__file__), 'gpload_insert.yml')],
                              False,
                              False)

    def test_case_update_with_transaction(self):
         self.help_test_with_config(['-f', os.path.join(os.path.dirname(__file__), 'gpload_update.yml')],
                              True,
                              True)

    def test_case_update_without_transaction(self):
         self.help_test_with_config(['--no_auto_trans', '-f', os.path.join(os.path.dirname(__file__), 'gpload_update.yml')],
                              False,
                              False)

    def test_case_configvalue(self):
        gploader = gpload(['-f', os.path.join(os.path.dirname(__file__), 'allconfig.yml')])
        self.assertEqual(u'test', gploader.getconfig('gpload:output:table'))
        self.assertEqual(1981, gploader.getconfig('gpload:input:source:port', int))
        self.assertEqual(True, gploader.getconfig('gpload:preload:reuse_tables', bool))
        self.assertEqual(False, gploader.getconfig('gpload:input:log_errors', bool, False))
        self.assertEqual(True, gploader.getconfig('gpload:input:fully_qualified_domain_name', bool))


#------------------------------- Mainline --------------------------------
if __name__ == '__main__':
    unittest.main()
