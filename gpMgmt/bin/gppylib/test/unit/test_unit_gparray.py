#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Line too long - pylint: disable=C0301
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved. 
#

""" Unittesting for gplog module
"""
import os

from gppylib.gparray import GpArray, Segment, createSegmentRows
from gppylib import gplog
from .gp_unittest import *
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


#------------------------------- non-test helpers --------------------------------
    def _setup_gparray(self, hostlist, interface_list, primary_list, primary_portbase, mirror_type,
                       mirror_list, mirror_portbase, dir_prefix):
        coordinator = Segment(content = -1,
                    preferred_role = 'p',
                    dbid = 0,
                    role = 'p',
                    mode = 's',
                    status = 'u',
                    hostname = 'coordinatorhost',
                    address = 'coordinatorhost-1',
                    port = 5432,
                    datadir = '/coordinatordir')
        allrows = []
        allrows.append(coordinator)                 
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
            
        for count in list(portdict.values()):
            self.assertEqual(expected_count, count)

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
        self.assertEqual(len(expected), len(actual))

        expected = sorted(expected)
        actual = sorted(actual)
        for i in range(len(expected)):
            self.assertEqual(expected[i], actual[i])

    @patch('gppylib.db.dbconn.querySingleton', return_value='PostgreSQL 8.3.23 (Greenplum Database 5.0.0 build dev) on x86_64-pc-linux-gnu, compiled by GCC gcc (GCC) 4.4.7 20120313 (Red Hat 4.4.7-17) compiled on Feb  9 2017 23:06:31')
    @patch('gppylib.db.dbconn.connect', autospec=True)
    def test_initFromCatalog_mismatched_versions(self, mock_connect, mock_query):
        with self.assertRaisesRegex(Exception, 'Cannot connect to GPDB version 5 from installed version 7'):
            GpArray.initFromCatalog(None)

    def test_get_min_primary_port_when_cluster_balanced(self):
        self.gpArray = self._createBalancedGpArrayWith2Primary2Mirrors()
        expected = 6000
        actual = GpArray.get_min_primary_port(self.gpArray)
        self.assertEqual(expected, actual)

    def test_get_min_primary_port_when_cluster_not_balanced(self):
        self.gpArray = self._createUnBalancedGpArrayWith2Primary2Mirrors()
        expected = 6000
        actual = GpArray.get_min_primary_port(self.gpArray)
        self.assertEqual(expected, actual)

    def test_get_max_primary_port_when_cluster_balanced(self):
        self.gpArray = self._createBalancedGpArrayWith2Primary2Mirrors()
        expected = 6001
        actual = GpArray.get_max_primary_port(self.gpArray)
        self.assertEqual(expected, actual)

    def test_get_max_primary_port_when_cluster_not_balanced(self):
        self.gpArray = self._createUnBalancedGpArrayWith2Primary2Mirrors()
        expected = 6001
        actual = GpArray.get_max_primary_port(self.gpArray)
        self.assertEqual(expected, actual)

    def test_get_min_mirror_port_when_cluster_balanced(self):
        self.gpArray = self._createBalancedGpArrayWith2Primary2Mirrors()
        expected = 7000
        actual = GpArray.get_min_mirror_port(self.gpArray)
        self.assertEqual(expected, actual)

    def test_get_min_mirror_port_when_cluster_not_balanced(self):
        self.gpArray = self._createUnBalancedGpArrayWith2Primary2Mirrors()
        expected = 7000
        actual = GpArray.get_min_mirror_port(self.gpArray)
        self.assertEqual(expected, actual)

    def test_get_max_mirror_port_when_cluster_balanced(self):
        self.gpArray = self._createBalancedGpArrayWith2Primary2Mirrors()
        expected = 7001
        actual = GpArray.get_max_mirror_port(self.gpArray)
        self.assertEqual(expected, actual)

    def test_get_max_mirror_port_when_cluster_not_balanced(self):
        self.gpArray = self._createUnBalancedGpArrayWith2Primary2Mirrors()
        expected = 7001
        actual = GpArray.get_max_mirror_port(self.gpArray)
        self.assertEqual(expected, actual)

    def test_get_primary_port_list_when_cluster_balanced(self):
        self.gpArray = self._createBalancedGpArrayWith2Primary2Mirrors()
        expected = [6000, 6001]
        actual = GpArray.get_primary_port_list(self.gpArray)
        self.assertEqual(expected, actual)

    def test_get_primary_port_list_when_cluster_not_balanced(self):
        self.gpArray = self._createUnBalancedGpArrayWith2Primary2Mirrors()
        expected = [6000, 6001]
        actual = GpArray.get_primary_port_list(self.gpArray)
        self.assertEqual(expected, actual)

    def test_get_primary_port_list_len_exception(self):
        self.gpArray = self._createBalancedGpArrayWith2Primary2Mirrors()
        self.gpArray.segmentPairs = []
        with self.assertRaisesRegex(Exception, 'No primary ports found in array.'):
            GpArray.get_primary_port_list(self.gpArray)

    def test_get_mirror_port_list_when_cluster_balanced(self):
        self.gpArray = self._createBalancedGpArrayWith2Primary2Mirrors()
        expected = [7000, 7001]
        actual = GpArray.get_mirror_port_list(self.gpArray)
        self.assertEqual(expected, actual)

    def test_get_mirror_port_list_when_cluster_not_balanced(self):
        self.gpArray = self._createUnBalancedGpArrayWith2Primary2Mirrors()
        expected = [7000, 7001]
        actual = GpArray.get_mirror_port_list(self.gpArray)
        self.assertEqual(expected, actual)

    @patch('gppylib.gparray.GpArray.get_mirroring_enabled', return_value=False)
    def test_get_mirror_port_list_no_mirror_exception(self,mock1):
        self.gpArray = self._createBalancedGpArrayWith2Primary2Mirrors()
        with self.assertRaisesRegex(Exception, 'Mirroring is not enabled'):
            GpArray.get_mirror_port_list(self.gpArray)

    @patch('gppylib.gparray.GpArray.get_mirroring_enabled', return_value=True)
    def test_get_mirror_port_list_len_exception(self, mock1):
        self.gpArray = self._createBalancedGpArrayWith2Primary2Mirrors()
        self.gpArray.segmentPairs = []
        with self.assertRaisesRegex(Exception, 'No mirror ports found in array.'):
            GpArray.get_mirror_port_list(self.gpArray)

    def _createBalancedGpArrayWith2Primary2Mirrors(self):
        self.coordinator = Segment.initFromString(
            "1|-1|p|p|s|u|cdw|cdw|6432|/data/coordinator")
        self.primary0 = Segment.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|6000|/data/primary0")
        self.primary1 = Segment.initFromString(
            "3|1|p|p|s|u|sdw2|sdw2|6001|/data/primary1")
        self.mirror0 = Segment.initFromString(
            "4|0|m|m|s|u|sdw2|sdw2|7000|/data/mirror0")
        self.mirror1 = Segment.initFromString(
            "5|1|m|m|s|u|sdw1|sdw1|7001|/data/mirror1")
        self.standby = Segment.initFromString(
            "6|-1|m|m|s|u|sdw3|sdw3|6432|/data/standby")
        return GpArray([self.coordinator, self.standby, self.primary0, self.primary1, self.mirror0, self.mirror1])

    def _createUnBalancedGpArrayWith2Primary2Mirrors(self):
        self.coordinator = Segment.initFromString(
            "1|-1|p|p|s|u|cdw|cdw|6432|/data/coordinator")
        self.primary0 = Segment.initFromString(
            "2|0|m|p|s|u|sdw1|sdw1|6000|/data/primary0")
        self.primary1 = Segment.initFromString(
            "3|1|m|p|s|u|sdw2|sdw2|6001|/data/primary1")
        self.mirror0 = Segment.initFromString(
            "4|0|p|m|s|u|sdw2|sdw2|7000|/data/mirror0")
        self.mirror1 = Segment.initFromString(
            "5|1|p|m|s|u|sdw1|sdw1|7001|/data/mirror1")
        self.standby = Segment.initFromString(
            "6|-1|m|m|s|u|sdw3|sdw3|6432|/data/standby")
        return GpArray([self.coordinator, self.standby, self.primary0, self.primary1, self.mirror0, self.mirror1])

def convert_bool(val):
    if type(val) is bool:
        return val
    else:
        if val == 't':
            return True
    return False

#------------------------------- Mainline --------------------------------
if __name__ == '__main__':
    run_tests()
