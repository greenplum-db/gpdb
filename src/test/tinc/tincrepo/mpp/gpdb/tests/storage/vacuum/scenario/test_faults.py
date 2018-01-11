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

import tinctest
from tinctest.models.scenario import ScenarioTestCase
from tinctest.lib.system import TINCSystem
from mpp.models import MPPTestCase
from mpp.models import SQLTestCase
from mpp.lib.filerep_util import Filerepe2e_Util
from mpp.lib.PSQL import PSQL

import os
import shutil
import time
from xml.dom import minidom

def getText(node, recursive=True):
    L = []
    for n in node.childNodes:
        if n.nodeType in (n.TEXT_NODE, n.CDATA_SECTION_NODE):
            L.append(n.data)
        else:
            if recursive:
                L.append( get_text(n) )
    return ''.join(L)

@tinctest.skipLoading('scenario')
class scenario_sql(SQLTestCase):
    '''
    @gucs client_min_messages=WARNING
    '''

    sql_dir = 'output/sql'
    ans_dir = 'output/sql'
    out_dir = 'output/out'

@tinctest.skipLoading('scenario')
class scenario_fault(MPPTestCase):

    def setUp(self):
        self.filereputil = Filerepe2e_Util()

    def inject(self, faultname, faulttype, segid):
        self.filereputil.inject_fault(f=faultname, y=faulttype, seg_id=segid)

    def check_connection(self):
        '''
        Wait till the system is up, as master/segments may take some time
        to come back after FI crash.
        '''
        PSQL.wait_for_database_up()

class vacuumFaultsTest(MPPTestCase, ScenarioTestCase):
    '''
    @product_version gpdb: [4.3.3.0-]
    '''

    xml_dir = 'xml'

    @classmethod
    def get_xml_dir(cls):
        source_dir = cls.get_source_dir()
        return os.path.join(source_dir, cls.xml_dir)

    def loadXML(self, case_name):
        xml_dir = self.get_xml_dir()
        sql_output_dir = os.path.join(self.get_source_dir(), 'output/sql')
        TINCSystem.make_dirs(sql_output_dir, ignore_exists_error=True)

        # copy init_file from xml dir
        init_file_src = os.path.join(xml_dir, 'init_file')
        init_file_dst = os.path.join(sql_output_dir, 'init_file')
        shutil.copyfile(init_file_src, init_file_dst)

        filename = case_name + '.xml'
        xml_file = os.path.join(xml_dir, filename)

        doc = minidom.parse(xml_file)
        case_name = os.path.splitext(filename)[0]

        # extract sql and put them as files
        sql_elem = doc.getElementsByTagName('sql')
        assert(len(sql_elem) == 1)
        sql_elem = sql_elem[0]
        self.write_file(sql_elem, sql_output_dir, case_name, 'pre')
        self.write_file(sql_elem, sql_output_dir, case_name, 'trigger')
        self.write_file(sql_elem, sql_output_dir, case_name, 'post')

        ans_elem = doc.getElementsByTagName('ans')
        assert(len(ans_elem) == 1)
        ans_elem = ans_elem[0]
        self.write_file(ans_elem, sql_output_dir, case_name, 'pre')
        self.write_file(ans_elem, sql_output_dir, case_name, 'trigger')
        self.write_file(ans_elem, sql_output_dir, case_name, 'post')

    def write_file(self, parent, sql_output_dir, case_name, step):
        elem = parent.getElementsByTagName(step)
        ext = parent.localName
        output_file = os.path.join(sql_output_dir, case_name + '_' + step + '.' + ext)
        with open(output_file, 'w') as f:
            f.write(getText(elem[0]))

    # --- test methods
    def setUp(self):
        self.loadXML(self.method_name[len('test_'):])
        super(vacuumFaultsTest, self).setUp()

    def tearDown(self):
        try:
            Filerepe2e_Util().inject_fault(f='all', y='reset', H='ALL', r='primary')
            Filerepe2e_Util().inject_fault(f='all', y='reset', seg_id='1')
        except:
            pass
        super(vacuumFaultsTest, self).tearDown()

    def common_test_body(self, sqlname, faultname, faulttype, segid):
        prefix = self.package_name + '.test_faults'
        self.test_case_scenario.append([prefix + '.scenario_sql.test_' + sqlname + '_pre'])
        self.test_case_scenario.append([(prefix + '.scenario_fault.inject',  [faultname, faulttype, segid])])
        self.test_case_scenario.append([prefix + '.scenario_sql.test_' + sqlname + '_trigger'])
        self.test_case_scenario.append([prefix + '.scenario_fault.check_connection'])
        self.test_case_scenario.append([prefix + '.scenario_sql.test_' + sqlname + '_post'])

    def test_catalog_master_error(self):
        '''
        @description create and delete many entries in catalog, let vacuum full
                     cancel from master, make sure after all everything looks good.
        '''
        self.common_test_body('catalog_master_error', 'repair_frag_end', 'error', '1')

    def test_heap_crash_before_truncate(self):
        self.common_test_body('heap_crash_before_truncate', 'vacuum_full_before_truncate', 'panic', '2')

    def test_heap_crash_after_truncate(self):
        self.common_test_body('heap_crash_after_truncate', 'vacuum_full_after_truncate', 'checkpoint_and_panic', '2')

    def test_heap_update_crash_before_truncate(self):
        self.common_test_body('heap_update_crash_before_truncate', 'vacuum_full_before_truncate', 'panic', '2')

    def test_heap_mastercrash_before_truncate(self):
        self.common_test_body('heap_mastercrash_before_truncate', 'vacuum_full_before_truncate', 'panic', '1')

    def test_heap_mastererror_before_truncate(self):
        self.common_test_body('heap_mastererror_before_truncate', 'vacuum_full_before_truncate', 'error', '1')
