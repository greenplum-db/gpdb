#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Line too long - pylint: disable=C0301
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved. 
#

""" Unittesting for gplog module
"""
import os

from gppylib.gparray import GpArray, Segment, createSegmentRows, get_gparray_from_config
from gppylib import gplog
from gp_unittest import *
from mock import patch, Mock
from gppylib.system.configurationInterface import GpConfigurationProvider

logger = gplog.get_unittest_logger()

class GpArrayTestCase(GpTestCase):
    # todo move to top
    def setUp(self):
        self.gpConfigMock = Mock()
        self.apply_patches([
            patch('os.environ', new={}),
            patch('gppylib.system.configurationImplGpdb.GpConfigurationProviderUsingGpdbCatalog', return_value=self.gpConfigMock),
        ])

    def test_spreadmirror_layout(self):
        """ Basic spread mirroring """
        mirror_type = 'spread'
        primary_portbase = 5000
        mirror_portbase = 6000
        interface_list = [1]
        dir_prefix = 'gpseg'

        hostlist = ['host1']
        primary_list = ['/db1']
        mirror_list = ['/mir1']
        
        #need to have enough hosts otherwise we get exceptions
        with self.assertRaises(Exception):
            createSegmentRows(hostlist, interface_list, primary_list, primary_portbase, mirror_type, 
                              mirror_list, mirror_portbase, dir_prefix)
        primary_list.append('/db2')
        mirror_list.append('/mir2')
        with self.assertRaises(Exception):
            createSegmentRows(hostlist, interface_list, primary_list, primary_portbase, mirror_type, 
                              mirror_list, mirror_portbase, dir_prefix)
        hostlist.append('host2')
        with self.assertRaises(Exception):
            createSegmentRows(hostlist, interface_list, primary_list, primary_portbase, mirror_type, 
                              mirror_list, mirror_portbase, dir_prefix)
        
        #now we have enough
        hostlist.append('host3')
        self._validate_array(self._setup_gparray(hostlist, interface_list, primary_list, primary_portbase, mirror_type,
                                                 mirror_list, mirror_portbase, dir_prefix))
        
        
        #enough
        hostlist = ['host1', 'host2', 'host3']
        primary_list = ['/db1', '/db2']
        mirror_list = ['/mir1', '/mir2']
        self._validate_array(self._setup_gparray(hostlist, interface_list, primary_list, primary_portbase, mirror_type,
                                                 mirror_list, mirror_portbase, dir_prefix))

        #typical thumper
        hostlist = ['sdw1', 'sdw2', 'sdw3', 'sdw4', 'sdw5']
        primary_list = ['/dbfast1', '/dbfast2', '/dbfast3', '/dbfast4']
        mirror_list = ['/dbfast1/mirror', '/dbfast2/mirror', '/dbfast3/mirror', '/dbfast4/mirror']
        self._validate_array(self._setup_gparray(hostlist, interface_list, primary_list, primary_portbase, mirror_type,
                                                 mirror_list, mirror_portbase, dir_prefix))

        #typical Thor
        hostlist = ['sdw1', 'sdw2', 'sdw3', 'sdw4', 'sdw5', 'sdw6', 'sdw7', 'sdw8', 'sdw9']
        primary_list = ['/dbfast1', '/dbfast2', '/dbfast3', '/dbfast4', '/dbfast5', '/dbfast6', '/dbfast7', '/dbfast8']
        mirror_list = ['/dbfast1/mirror', '/dbfast2/mirror', '/dbfast3/mirror', '/dbfast4/mirror',
                       '/dbfast5/mirror', '/dbfast6/mirror', '/dbfast7/mirror', '/dbfast8/mirror']
        self._validate_array(self._setup_gparray(hostlist, interface_list, primary_list, primary_portbase, mirror_type,
                                                 mirror_list, mirror_portbase, dir_prefix))


    def test_groupmirror_layout(self):
        """ Basic group mirroring """
        mirror_type = 'grouped'
        primary_portbase = 5000
        mirror_portbase = 6000
        interface_list = [1]
        dir_prefix = 'gpseg'

        hostlist = ['host1']
        primary_list = ['/db1']
        mirror_list = ['/mir1']
        
        #not enough
        with self.assertRaises(Exception):
            createSegmentRows(hostlist, interface_list, primary_list, primary_portbase, mirror_type,
                              mirror_list, mirror_portbase, dir_prefix)

        primary_list.append('/db2')
        mirror_list.append('/mir2')
        with self.assertRaises(Exception):
            createSegmentRows(hostlist, interface_list, primary_list, primary_portbase, mirror_type,
                              mirror_list, mirror_portbase, dir_prefix)
        
        #enough
        hostlist = ['host1', 'host2']
        primary_list = ['/db1', '/db2']
        mirror_list = ['/mir1', '/mir2']
        
         #typical thumper
        hostlist = ['sdw1', 'sdw2', 'sdw3', 'sdw4', 'sdw5']
        primary_list = ['/dbfast1', '/dbfast2', '/dbfast3', '/dbfast4']
        mirror_list = ['/dbfast1/mirror', '/dbfast2/mirror', '/dbfast3/mirror', '/dbfast4/mirror']
        self._validate_array(self._setup_gparray(hostlist, interface_list, primary_list, primary_portbase, mirror_type,
                                                 mirror_list, mirror_portbase, dir_prefix))

        #typical Thor
        hostlist = ['sdw1', 'sdw2', 'sdw3', 'sdw4', 'sdw5', 'sdw6', 'sdw7', 'sdw8', 'sdw9']
        primary_list = ['/dbfast1', '/dbfast2', '/dbfast3', '/dbfast4', '/dbfast5', '/dbfast6', '/dbfast7', '/dbfast8']
        mirror_list = ['/dbfast1/mirror', '/dbfast2/mirror', '/dbfast3/mirror', '/dbfast4/mirror',
                       '/dbfast5/mirror', '/dbfast6/mirror', '/dbfast7/mirror', '/dbfast8/mirror']
        self._validate_array(self._setup_gparray(hostlist, interface_list, primary_list, primary_portbase, mirror_type,
                                                 mirror_list, mirror_portbase, dir_prefix))


    def test_get_segment_list(self):
        mirror_type = 'grouped'
        primary_portbase = 5000
        mirror_portbase = 6000
        interface_list = [1]
        dir_prefix = 'gpseg'

        hostlist = ['sdw1', 'sdw2', 'sdw3', 'sdw4', 'sdw5']
        primary_list = ['/dbfast1', '/dbfast2', '/dbfast3', '/dbfast4']
        mirror_list = ['/dbfast1/mirror', '/dbfast2/mirror', '/dbfast3/mirror', '/dbfast4/mirror']

        gparray = self._setup_gparray(hostlist, interface_list, primary_list, primary_portbase, mirror_type,
                                      mirror_list, mirror_portbase, dir_prefix)
        self._validate_array(gparray)

        # test without expansion segments
        expansion_hosts = []
        self._validate_get_segment_list(gparray, hostlist, expansion_hosts, primary_list)

        # test with expansion segments
        expansion_hosts = ['sdw6', 'sdw7']
        rows =  createSegmentRows(expansion_hosts, interface_list, primary_list, primary_portbase, mirror_type,
                                  mirror_list, mirror_portbase, dir_prefix)
        offset = len(hostlist) * len(primary_list) # need to continue numbering where the last createSegmentRows left off
        for row in rows:
            gparray.addExpansionSeg(row.content+offset, 'p' if convert_bool(row.isprimary) else 'm', row.dbid+offset,
                                    'p' if convert_bool(row.isprimary) else 'm', row.host, row.address, row.port, row.fulldir)
        self._validate_get_segment_list(gparray, hostlist, expansion_hosts, primary_list)


    @patch('gppylib.system.configurationInterface.getConfigurationProvider')
    @patch('gppylib.system.environment.GpMasterEnvironment', return_value=Mock(), autospec=True)
    def test_get_gparray_from_config(self, gpMasterEnvironmentMock, getConfigProviderFunctionMock):
        os.environ['MASTER_DATA_DIRECTORY'] = "MY_TEST_DIR"
        configProviderMock = Mock(spec=GpConfigurationProvider)
        getConfigProviderFunctionMock.return_value = configProviderMock
        configProviderMock.initializeProvider.return_value = configProviderMock
        gpArrayMock = Mock(spec=GpArray)
        gpArrayMock.hasMirrors = False
        configProviderMock.loadSystemConfig.return_value = gpArrayMock
        gpMasterEnvironmentMock.return_value.getMasterPort.return_value = 123456

        gpArray = get_gparray_from_config()

        self.assertEquals(gpArray.hasMirrors, False)
        gpMasterEnvironmentMock.assert_called_once_with("MY_TEST_DIR", False)
        getConfigProviderFunctionMock.assert_any_call()
        configProviderMock.initializeProvider.assert_called_once_with(123456)
        configProviderMock.loadSystemConfig.assert_called_once_with(useUtilityMode=True)


#------------------------------- non-test helpers --------------------------------
    def _setup_gparray(self, hostlist, interface_list, primary_list, primary_portbase, mirror_type,
                       mirror_list, mirror_portbase, dir_prefix):
        master = Segment(content = -1,
                    preferred_role = 'p',
                    dbid = 0,
                    role = 'p',
                    mode = 's',
                    status = 'u',
                    hostname = 'masterhost',
                    address = 'masterhost-1',
                    port = 5432,
                    datadir = '/masterdir')
        allrows = []
        allrows.append(master)                 
        rows =  createSegmentRows(hostlist, interface_list, primary_list, primary_portbase, mirror_type,
                                  mirror_list, mirror_portbase, dir_prefix)
        
        
        for row in rows:
            newrow = Segment(content = row.content,
                          preferred_role = 'p' if convert_bool(row.isprimary) else 'm',
                          dbid = row.dbid,
                          role = 'p' if convert_bool(row.isprimary) else 'm',
                          mode = 's',
                          status = 'u',
                          hostname = row.host,
                          address = row.address,
                          port = row.port,
                          datadir = row.fulldir)
            allrows.append(newrow)
        
        gparray = GpArray(allrows)
        return gparray
        
    def _validate_array(self, gparray): 
        portdict = {}
        lastport = 0
        for seg in gparray.segmentPairs:
            prim = seg.primaryDB            
            mir = seg.mirrorDB
            self.assertNotEqual(prim.hostname, mir.hostname)
            if prim.port not in portdict:
                portdict[prim.port] = 1
            else:
                portdict[prim.port] = 1 + portdict[prim.port]
            lastport = prim.port
            
        expected_count = portdict[lastport]
            
        for count in portdict.values():
            self.assertEquals(expected_count, count)

    def _validate_get_segment_list(self, gparray, hostlist, expansion_hosts, primary_list):
        hostlist.extend(expansion_hosts)
        expected = []
        for host in hostlist:
            for primary in primary_list:
                expected.append("host %s, primary %s" % (host, primary))

        has_expansion_hosts = (len(expansion_hosts) > 0)
        segments = gparray.getSegmentList(has_expansion_hosts)
        actual = []
        for segment in segments:
            primary = segment.primaryDB
            datadir = primary.datadir[0:primary.datadir.rindex("/")] # strip off the "/gpseg##" portion of the primary name for comparison
            actual.append("host %s, primary %s" % (primary.hostname, datadir))
        self.assertEquals(len(expected), len(actual))

        expected = sorted(expected)
        actual = sorted(actual)
        for i in range(len(expected)):
            self.assertEquals(expected[i], actual[i])

    @patch('gppylib.db.dbconn.execSQL', return_value=[['PostgreSQL 8.3.23 (Greenplum Database 5.0.0 build dev) on x86_64-pc-linux-gnu, compiled by GCC gcc (GCC) 4.4.7 20120313 (Red Hat 4.4.7-17) compiled on Feb  9 2017 23:06:31']])
    @patch('gppylib.db.dbconn.connect', autospec=True)
    def test_initFromCatalog_mismatched_versions(self, mock_connect, mock_execSQL):
        with self.assertRaisesRegexp(Exception, 'Cannot connect to GPDB version 5 from installed version 6'):
            GpArray.initFromCatalog(None)

def convert_bool(val):
    if val == 't':
        return True
    else:
        return False   

#------------------------------- Mainline --------------------------------
if __name__ == '__main__':
    run_tests()
