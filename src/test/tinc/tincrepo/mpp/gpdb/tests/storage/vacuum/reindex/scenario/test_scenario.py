"""
Copyright (c) 2004-Present Pivotal Software, Inc.

This program and the accompanying materials are made available under
the terms of the under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

from mpp.models import SQLTestCase
from mpp.gpdb.tests.storage.vacuum.reindex import Reindex
from mpp.gpdb.tests.storage.GPDBStorageBaseTestCase import *

@tinctest.skipLoading("Test model. No tests loaded.")
class reindex_stp(SQLTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    sql_dir = 'reindex_stp/sql/'
    ans_dir = 'reindex_stp/expected/'
    out_dir = 'reindex_stp/output/'

@tinctest.skipLoading("Test model. No tests loaded.")
class reindex_db(SQLTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    sql_dir = 'reindex_db/sql/'
    ans_dir = 'reindex_db/expected/'
    out_dir = 'reindex_db/output/'

    def setUp(self):
        super(reindex_db, self).setUp()
        self.util = Reindex()
        self.util.inject_fault(fault_ = "reindex_db", mode_ = "async", operation_ = "suspend", prim_mirr_ = "primary", seg_id_ = 1)

@tinctest.skipLoading("Test model. No tests loaded.")
class drop_obj(SQLTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    sql_dir = 'drop_obj/sql/'
    ans_dir = 'drop_obj/expected/'
    out_dir = 'drop_obj/output/'

    def setUp(self):
        super(drop_obj, self).setUp()
        self.util = Reindex()
        self.util.check_fault_status(fault_name_ = "reindex_db", status_ = "triggered", seg_id_ = 1,  max_cycle_ = 20)

    def tearDown(self):
        self.util.inject_fault(operation_ = "reset", fault_ = "reindex_db", mode_ = "async", prim_mirr_ = "primary", seg_id_ = 1)

@tinctest.skipLoading("Test model. No tests loaded.")
class reindex_rel(SQLTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    sql_dir = 'reindex_rel/sql/'
    ans_dir = 'reindex_rel/expected/'
    out_dir = 'reindex_rel/output/'

    def setUp(self):
        super(reindex_rel, self).setUp()
        self.util = Reindex()
        self.util.inject_fault(fault_ = "reindex_relation", mode_ = "async", operation_ = "suspend", prim_mirr_ = "primary", seg_id_ = 1)

@tinctest.skipLoading("Test model. No tests loaded.")
class add_index(SQLTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    sql_dir = 'add_index/sql/'
    ans_dir = 'add_index/expected/'
    out_dir = 'add_index/output/'

    def setUp(self):
        super(add_index, self).setUp()
        self.util = Reindex()
        self.util.check_fault_status(fault_name_ = "reindex_relation", status_ = "triggered", seg_id_ = 1,  max_cycle_ = 20)

    def tearDown(self):
        self.util.inject_fault(operation_ = "reset", fault_ = "reindex_relation", mode_ = "async", prim_mirr_ = "primary", seg_id_ = 1)

@tinctest.skipLoading("Test model. No tests loaded.")
class reindex_verify(SQLTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    sql_dir = 'reindex_verify/sql/'
    ans_dir = 'reindex_verify/expected/'
    out_dir = 'reindex_verify/output/'

    def setUp(self):
        super(reindex_verify, self).setUp()
        self.util = Reindex()

    def tearDown(self):
        tinctest.logger.info("Starting gpcheckcat...")
        self.util.do_gpcheckcat()

@tinctest.skipLoading("Test model. No tests loaded.")
class setup_gpfastseq(SQLTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    sql_dir = 'gp_fastsequence/setup/sql/'
    ans_dir = 'gp_fastsequence/setup/expected/'
    out_dir = 'gp_fastsequence/setup/output/'

@tinctest.skipLoading("Test model. No tests loaded.")
class reindex_gpfastseq(SQLTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    sql_dir = 'gp_fastsequence/reindex_gpfastseq/sql/'
    ans_dir = 'gp_fastsequence/reindex_gpfastseq/expected/'
    out_dir = 'gp_fastsequence/reindex_gpfastseq/output/'

    def setUp(self):
        super(reindex_gpfastseq, self).setUp()
        self.util = Reindex()
        self.util.inject_fault(fault_ = "reindex_relation", mode_ = "async", operation_ = "suspend", prim_mirr_ = "primary", seg_id_ = 1)

@tinctest.skipLoading("Test model. No tests loaded.")
class insert_tup_gpfastseq(SQLTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    sql_dir = 'gp_fastsequence/insert_tuple/sql/'
    ans_dir = 'gp_fastsequence/insert_tuple/expected/'
    out_dir = 'gp_fastsequence/insert_tuple/output/'

    def setUp(self):
        super(insert_tup_gpfastseq, self).setUp()
        self.util = Reindex()
        self.util.check_fault_status(fault_name_ = "reindex_relation", status_ = "triggered", seg_id_ = 1,  max_cycle_ = 20)

    def tearDown(self):
        self.util.inject_fault(operation_ = "reset", fault_ = "reindex_relation", mode_ = "async", prim_mirr_ = "primary", seg_id_ = 1)

@tinctest.skipLoading("Test model. No tests loaded.")
class verify_gpfastseq(SQLTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    sql_dir = 'gp_fastsequence/verify/sql/'
    ans_dir = 'gp_fastsequence/verify/expected/'
    out_dir = 'gp_fastsequence/verify/output/'

    def setUp(self):
        super(verify_gpfastseq, self).setUp()
        self.util = Reindex()

    def tearDown(self):
        tinctest.logger.info("Starting gpcheckcat...")
        self.util.do_gpcheckcat()

@tinctest.skipLoading("Test model. No tests loaded.")
class reset_fault(SQLTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    sql_dir = 'gp_fastsequence/reset_fault/sql/'
    ans_dir = 'gp_fastsequence/reset_fault/expected/'
    out_dir = 'gp_fastsequence/reset_fault/output/'

    def setUp(self):
        super(reset_fault, self).setUp()
        self.util = Reindex()

        ctr=0
        ans = False
        while ctr < 10:
            ctr += 1
            ans = self.util.check_fault_status(fault_name_ = "reindex_relation", status_ = "triggered", seg_id_ = 1,  max_cycle_ = 20)
            if ans == True:
                ans2=self.util.inject_fault(operation_ = "reset", fault_ = "reindex_relation", mode_ = "async", prim_mirr_ = "primary", seg_id_ = 1)
                if ans2[0] == True:
                    break
            else:
                sleep(30)
        if ctr == 10: 
            self.assertFalse(ans,"fault reindex_relation not triggered!!")
