#!/usr/bin/env python
#
# Copyright (c) 2014-Present Pivotal Software, Inc.
#

import os
import re
import shutil
import unittest
from collections import defaultdict
from gppylib.gpversion import GpVersion
from gppylib.commands.base import Command, CommandResult, ExecutionError
from mock import patch, MagicMock, Mock, mock_open
from gppylib.operations.persistent_rebuild import ValidateContentID, DbIdInfo, GetDbIdInfo, BackupPersistentTableFiles,\
                                                  RebuildTable, RebuildPersistentTables, ValidatePersistentBackup,\
                                                  RunBackupRestore, ValidateMD5Sum

remove_per_db_pt_entry = False
remove_global_pt_entry = False

def pt_query_side_effect(*args, **kwargs):
    # missing the global persistent table entry
    GET_ALL_DATABASES = """select oid, datname from pg_database"""
    PER_DATABASE_PT_FILES_QUERY = """SELECT relfilenode FROM pg_class WHERE oid IN (5094, 5095)"""
    GLOBAL_PT_FILES_QUERY = """SELECT relfilenode FROM pg_class WHERE oid IN (5090, 5091, 5092, 5093)"""

    if args[1] == GET_ALL_DATABASES:
        return [[123, 'db1']]
    elif args[1] == PER_DATABASE_PT_FILES_QUERY:
        if remove_per_db_pt_entry:
            return [[5095]]
        else:
            return [[5094], [5095]]
    else:
        if remove_global_pt_entry:
            return [[5091], [5092], [5093]]
        else:
            return [[5090], [5091], [5092], [5093]]

class ValidateContentIDTestCase(unittest.TestCase):
    def setUp(self):
        self.contentid_validator = ValidateContentID(content_id=None,
                                                     contentid_file=None,
                                                     gparray=None)

    @patch('os.path.isfile', return_value=True)
    def test_validate_contentid_file_with_valid_content_ids(self, mock1):
        expected = [1, 2, 3]
        file_contents = '1\n2\n3'
        self.contentid_validator.contentid_file = '/tmp/contentid_file'
        m = MagicMock()
        m.return_value.__enter__.return_value.__iter__.return_value = iter(file_contents.split())
        with patch('__builtin__.open', m, create=True):
            self.assertEqual(expected, self.contentid_validator._validate_contentid_file())

    @patch('os.path.isfile', return_value=True)
    def test_validate_contentid_file_with_spaces_content_ids(self, mock1):
        expected = [1, 2, 3]
        file_contents = '    1\n2   \n3   \n'
        self.contentid_validator.contentid_file = '/tmp/contentid_file'
        m = MagicMock()
        m.return_value.__enter__.return_value.__iter__.return_value = iter(file_contents.split())
        with patch('__builtin__.open', m, create=True):
            self.assertEqual(expected, self.contentid_validator._validate_contentid_file())

    @patch('os.path.isfile', return_value=True)
    def test_validate_contentid_file_with_invalid_content_ids(self, mock1):
        expected = [1, 2, 3]
        file_contents = '1\nb\n3'
        self.contentid_validator.contentid_file = '/tmp/contentid_file'
        m = MagicMock()
        m.return_value.__enter__.return_value.__iter__.return_value = iter(file_contents.split())
        with patch('__builtin__.open', m, create=True):
            with self.assertRaisesRegexp(Exception, 'Found non integer content id "b" in contentid file "/tmp/contentid_file"'):
                self.contentid_validator._validate_contentid_file()

    @patch('os.path.isfile', return_value=True)
    def test_validate_contentid_file_with_empty_file(self, mock1):
        file_contents = ''
        self.contentid_validator.contentid_file = '/tmp/contentid_file'
        m = MagicMock()
        m.return_value.__enter__.return_value.__iter__.return_value = iter(file_contents.split())
        with patch('__builtin__.open', m, create=True):
            with self.assertRaisesRegexp(Exception, 'Please make sure there is atleast one integer content ID in the file'):
                self.contentid_validator._validate_contentid_file()

    @patch('os.path.isfile', return_value=False)
    def test_validate_contentid_file_with_non_existent_file(self, mock1):
        expected = [1, 2, 3]
        file_contents = '1\nb\n3'
        self.contentid_validator.contentid_file = '/tmp/contentid_file'
        with self.assertRaisesRegexp(Exception, 'Unable to find contentid file "/tmp/contentid_file"'):
            self.contentid_validator._validate_contentid_file()

    @patch('os.path.isfile', return_value=True)
    def test_validate_contentid_file_with_blank_lines(self, mock1):
        expected = [1, 2]
        file_contents = '1\n\n\n2'
        self.contentid_validator.contentid_file = '/tmp/contentid_file'
        m = MagicMock()
        m.return_value.__enter__.return_value.__iter__.return_value = iter(file_contents.split())
        with patch('__builtin__.open', m, create=True):
            self.assertEqual(expected, self.contentid_validator._validate_contentid_file())

    @patch('os.path.isfile', return_value=True)
    def test_validate_contentid_file_with_negative_integers(self, mock1):
        expected = [-1, 2]
        file_contents = '-1\n2'
        self.contentid_validator.contentid_file = '/tmp/contentid_file'
        m = MagicMock()
        m.return_value.__enter__.return_value.__iter__.return_value = iter(file_contents.split())
        with patch('__builtin__.open', m, create=True):
            self.assertEqual(expected, self.contentid_validator._validate_contentid_file())

    def test_validate_content_id_with_valid_segments(self):
        expected = [1, 2, 3]
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId = Mock()
            m.getSegmentContentId.return_value = (i % 3) + 1
            mock_segs.append(m)
        gparray = Mock()
        gparray.getDbList = Mock()
        gparray.getDbList.return_value = mock_segs
        self.contentid_validator.gparray = gparray
        self.contentid_validator.content_id = [1, 2, 3]
        self.assertEqual(expected, self.contentid_validator._validate_content_id())

    def test_validate_content_id_with_invalid_segments(self):
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId = Mock()
            m.getSegmentContentId.return_value = i + 10
            mock_segs.append(m)
        gparray = Mock()
        gparray.getDbList = Mock()
        gparray.getDbList.return_value = mock_segs
        self.contentid_validator.gparray = gparray
        self.contentid_validator.content_id = [1, 2, 3]
        with self.assertRaisesRegexp(Exception, 'The following content ids are not present in gp_segment_configuration: 1, 2, 3'):
            self.contentid_validator._validate_content_id()

    def test_validate_content_id_with_primary_segment_down(self):
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId = Mock()
            m.getSegmentContentId.return_value = (i % 3) + 1
            if i == 1:
                m.getSegmentStatus = Mock()
                m.getSegmentStatus.return_value = 'd'
            mock_segs.append(m)
        gparray = Mock()
        gparray.getDbList = Mock()
        gparray.getDbList.return_value = mock_segs
        self.contentid_validator.gparray = gparray
        self.contentid_validator.content_id = [1, 2, 3]
        self.contentid_validator._validate_content_id()

    def test_validate_content_id_with_resync(self):
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId.return_value = (i % 3) + 1
            m.getSegmentStatus.return_value = 'u'
            if i == 1:
                m.getSegmentMode.return_value = 'r'
            else:
                m.getSegmentMode.return_value = 's'
            mock_segs.append(m)
        gparray = Mock()
        gparray.getDbList = Mock()
        gparray.getDbList.return_value = mock_segs
        self.contentid_validator.gparray = gparray
        self.contentid_validator.content_id = [1, 2, 3]
        with self.assertRaisesRegexp(Exception, 'Can not rebuild persistent tables for content ids that are in resync mode'):
            self.contentid_validator._validate_content_id()

    @patch('gppylib.operations.persistent_rebuild.ask_yesno', return_value=False)
    def test_validate_content_id_with_some_others_resync(self, mock1):
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId.return_value = (i % 3) + 1
            m.getSegmentStatus.return_value = 'u'
            if m.getSegmentContentId.return_value in (1, 2):
                m.getSegmentMode.return_value = 'r'
            else:
                m.getSegmentMode.return_value = 's'
            mock_segs.append(m)
        gparray = Mock()
        gparray.getDbList = Mock()
        gparray.getDbList.return_value = mock_segs
        self.contentid_validator.gparray = gparray
        self.contentid_validator.content_id = [3]
        with self.assertRaisesRegexp(Exception, 'Aborting rebuild due to user request'):
            self.contentid_validator._validate_content_id()

    def test_validate_content_id_with_change_tracking_segments(self):
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId = Mock()
            m.getSegmentContentId.return_value = (i % 3) + 1
            if i == 1:
                m.getSegmentStatus = Mock()
                m.getSegmentStatus.return_value = 'c'
            mock_segs.append(m)
        gparray = Mock()
        gparray.getDbList = Mock()
        gparray.getDbList.return_value = mock_segs
        self.contentid_validator.gparray = gparray
        self.contentid_validator.content_id = [1, 2, 3]
        self.assertEqual([1, 2, 3], self.contentid_validator._validate_content_id())

    def test_parse_content_id(self):
        self.contentid_validator.content_id = '1, 2, 3'
        self.assertEqual([1, 2, 3], self.contentid_validator._parse_content_id())

    def test_parse_content_id_valid_single_content_id(self):
        self.contentid_validator.content_id = '-1'
        self.assertEqual([-1], self.contentid_validator._parse_content_id())

    def test_parse_content_id_invalid_comma_separated_list(self):
        self.contentid_validator.content_id = '1, 2, 3,,'
        with self.assertRaisesRegexp(Exception, 'Some content ids are not integers:'):
            self.contentid_validator._parse_content_id()

    def test_parse_content_id_invalid_integers(self):
        self.contentid_validator.content_id = '1, 2, a, x,'
        with self.assertRaisesRegexp(Exception, 'Some content ids are not integers:'):
            self.contentid_validator._parse_content_id()

    @patch('gppylib.operations.persistent_rebuild.ValidateContentID._validate_content_id', return_value=[1, 2, 3])
    def test_validate_with_only_content_id(self, mock1):
        self.contentid_validator.content_id = '1, 2, 3'
        self.contentid_validator.contentid_file = None
        self.assertEqual([1, 2, 3], self.contentid_validator.validate())

    @patch('gppylib.operations.persistent_rebuild.ValidateContentID._validate_content_id', side_effect=Exception('ERROR'))
    def test_validate_with_only_content_id_with_error(self, mock1):
        self.contentid_validator.content_id = '1, 2, 3'
        self.contentid_validator.contentid_file = None
        with self.assertRaisesRegexp(Exception, 'ERROR'):
            self.contentid_validator.validate()

    @patch('gppylib.operations.persistent_rebuild.ValidateContentID._validate_contentid_file', return_value=[1, 2, 3])
    @patch('gppylib.operations.persistent_rebuild.ValidateContentID._validate_content_id', return_value=[1, 2, 3])
    def test_validate_with_only_content_id_file(self, mock1, mock2):
        self.contentid_validator.contentid_file = '/tmp/f1'
        self.contentid_validator.content_id = None
        self.assertEqual([1, 2, 3], self.contentid_validator.validate())

    @patch('gppylib.operations.persistent_rebuild.ValidateContentID._validate_contentid_file', side_effect=Exception('ERROR'))
    def test_validate_with_only_content_id_file_with_error(self, mock1):
        self.contentid_validator.contentid_file = '/tmp/f1'
        self.contentid_validator.content_id = None
        with self.assertRaisesRegexp(Exception, 'ERROR'):
            self.contentid_validator.validate()

class GetDbIdInfoTestCase(unittest.TestCase):
    def setUp(self):
        self.dbid_info = GetDbIdInfo(gparray=None, content_id=None)

    @patch('gppylib.operations.persistent_rebuild.dbconn.execSQL', return_value=[(1000, '2000'), (1001, '2001 2002')])
    @patch('gppylib.operations.persistent_rebuild.dbconn.connect')
    @patch('gppylib.operations.persistent_rebuild.dbconn.DbURL')
    def test_get_filespace_to_tablespace_map(self, mock1, mock2, mock3):
        m = Mock()
        m.getSegmentFilespaces.return_value = {1000: '/tmp/fs1', 1001: '/tmp/fs2'}
        self.assertEqual({1000: [2000], 1001: [2001, 2002]}, self.dbid_info._get_filespace_to_tablespace_map(m))

    @patch('gppylib.operations.persistent_rebuild.dbconn.execSQL', return_value=[])
    @patch('gppylib.operations.persistent_rebuild.dbconn.connect')
    @patch('gppylib.operations.persistent_rebuild.dbconn.DbURL')
    def test_get_filespace_to_tablespace_map_empty_filespaces(self, mock1, mock2, mock3):
        m = Mock()
        m.getSegmentFilespaces.return_value = {}
        self.assertEqual({}, self.dbid_info._get_filespace_to_tablespace_map(m))

    @patch('gppylib.operations.persistent_rebuild.dbconn.execSQL', return_value=[(1000, '2000'), (1001, '2001 2002')])
    @patch('gppylib.operations.persistent_rebuild.dbconn.connect')
    @patch('gppylib.operations.persistent_rebuild.dbconn.DbURL')
    def test_get_tablespace_to_dboid_map(self, mock1, mock2, mock3):
        ts_oids = [1000, 1001]
        self.assertEqual({1000: [2000], 1001: [2001, 2002]}, self.dbid_info._get_tablespace_to_dboid_map(ts_oids))

    @patch('gppylib.operations.persistent_rebuild.dbconn.execSQL', return_value=[])
    @patch('gppylib.operations.persistent_rebuild.dbconn.connect')
    @patch('gppylib.operations.persistent_rebuild.dbconn.DbURL')
    def test_get_tablespace_to_dboid_map_empty_tablespaces(self, mock1, mock2, mock3):
        ts_oids = []
        self.assertEqual({}, self.dbid_info._get_tablespace_to_dboid_map(ts_oids))

    @patch('gppylib.operations.persistent_rebuild.GetDbIdInfo._get_filespace_to_tablespace_map', return_value={})
    @patch('gppylib.operations.persistent_rebuild.GetDbIdInfo._get_tablespace_to_dboid_map', return_value={})
    def test_get_info_with_no_matching_content_id(self, mock1, mock2):
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId.return_value = i + 1
            m.getSegmentRole.return_value = 'p' if i < 3 else 'm'
            m.getSegmentStatus.return_value = 'u'
            mock_segs.append(m)
        gparray = Mock()
        gparray.getDbList = Mock()
        gparray.getDbList.return_value = mock_segs
        self.dbid_info.gparray = gparray
        self.dbid_info.content_id = [11, 12]
        expected = []
        self.assertEqual(expected, self.dbid_info.get_info())

    @patch('gppylib.operations.persistent_rebuild.GetDbIdInfo._get_filespace_to_tablespace_map', return_value={1000: [2000, 2002], 1001: [2001, 2003]})
    @patch('gppylib.operations.persistent_rebuild.GetDbIdInfo._get_tablespace_to_dboid_map',
            return_value={2000: [12345], 2001: [2345, 4567], 2002: [8765, 4634], 2003: [3456]})
    def test_get_info_with_single_matching_content_id(self, mock1, mock2):
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId.return_value = i + 1
            m.getSegmentDbId.return_value = i + 2
            m.getSegmentRole.return_value = 'p' if i < 3 else 'm'
            m.getSegmentStatus.return_value = 'u'
            m.getSegmentHostName.return_value = 'mdw1'
            m.getSegmentPort.return_value = 5001 + i
            m.getSegmentFilespaces.return_value = {1000: '/tmp/f1', 1001: '/tmp/f2'}
            m.isSegmentDown.return_value = False
            mock_segs.append(m)
        gparray = Mock()
        gparray.getDbList = Mock()
        gparray.getDbList.return_value = mock_segs
        self.dbid_info.gparray = gparray
        self.dbid_info.content_id = [1, 10]
        expected = [DbIdInfo(1, 'p', 2, 5001, 'mdw1', {1000: '/tmp/f1', 1001: '/tmp/f2'}, {1000: [2000, 2002], 1001: [2001, 2003]},
                    {2000: [12345], 2001: [2345, 4567], 2002: [8765, 4634], 2003: [3456]}, False)]
        self.assertEqual(expected, self.dbid_info.get_info())

    @patch('gppylib.operations.persistent_rebuild.GetDbIdInfo._get_filespace_to_tablespace_map', return_value={})
    @patch('gppylib.operations.persistent_rebuild.GetDbIdInfo._get_tablespace_to_dboid_map', return_value={})
    def test_get_info_with_single_matching_content_id_and_no_filespaces(self, mock1, mock2):
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId.return_value = i + 1
            m.getSegmentDbId.return_value = i + 2
            m.getSegmentRole.return_value = 'p' if i < 3 else 'm'
            m.getSegmentStatus.return_value = 'u'
            m.getSegmentHostName.return_value = 'mdw1'
            m.getSegmentPort.return_value = 5001 + i
            m.getSegmentFilespaces.return_value = {}
            m.isSegmentDown.return_value = False
            mock_segs.append(m)
        gparray = Mock()
        gparray.getDbList = Mock()
        gparray.getDbList.return_value = mock_segs
        self.dbid_info.gparray = gparray
        self.dbid_info.content_id = [1, 10]
        expected = [DbIdInfo(1, 'p', 2, 5001, 'mdw1', {}, {}, {}, False)]
        self.assertEqual(expected, self.dbid_info.get_info())

    @patch('gppylib.operations.persistent_rebuild.GetDbIdInfo._get_filespace_to_tablespace_map', return_value={})
    @patch('gppylib.operations.persistent_rebuild.GetDbIdInfo._get_tablespace_to_dboid_map', return_value={})
    def test_get_info_with_single_matching_content_id_and_no_tablespaces(self, mock1, mock2):
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId.return_value = i + 1
            m.getSegmentDbId.return_value = i + 2
            m.getSegmentRole.return_value = 'p' if i < 3 else 'm'
            m.getSegmentStatus.return_value = 'u'
            m.getSegmentHostName.return_value = 'mdw1'
            m.getSegmentPort.return_value = 5001 + i
            m.getSegmentFilespaces.return_value = {1000: '/tmp/f1', 1001: '/tmp/f2'}
            m.isSegmentDown.return_value = False
            mock_segs.append(m)
        gparray = Mock()
        gparray.getDbList = Mock()
        gparray.getDbList.return_value = mock_segs
        self.dbid_info.gparray = gparray
        self.dbid_info.content_id = [1, 10]
        expected = [DbIdInfo(1, 'p', 2, 5001, 'mdw1', {1000: '/tmp/f1', 1001: '/tmp/f2'}, {}, {}, False)]
        self.assertEqual(expected, self.dbid_info.get_info())

    @patch('gppylib.operations.persistent_rebuild.GetDbIdInfo._get_filespace_to_tablespace_map', return_value={})
    @patch('gppylib.operations.persistent_rebuild.GetDbIdInfo._get_tablespace_to_dboid_map', return_value={})
    def test_get_info_with_single_matching_content_id_and_down_segments(self, mock1, mock2):
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId.return_value = i + 1
            m.getSegmentDbId.return_value = i + 2
            m.getSegmentRole.return_value = 'p' if i < 3 else 'm'
            m.getSegmentStatus.return_value = 'd' if i == 3 else 'u'
            m.getSegmentHostName.return_value = 'mdw1'
            m.getSegmentPort.return_value = 5001 + i
            m.getSegmentFilespaces.return_value = {1000: '/tmp/f1', 1001: '/tmp/f2'}
            m.isSegmentDown.return_value = False
            mock_segs.append(m)
        gparray = Mock()
        gparray.getDbList = Mock()
        gparray.getDbList.return_value = mock_segs
        self.dbid_info.gparray = gparray
        self.dbid_info.content_id = [1, 10]
        expected = [DbIdInfo(1, 'p', 2, 5001, 'mdw1', {1000: '/tmp/f1', 1001: '/tmp/f2'}, {}, {}, False)]
        self.assertEqual(expected, self.dbid_info.get_info())

    @patch('gppylib.operations.persistent_rebuild.GetDbIdInfo._get_filespace_to_tablespace_map', return_value={})
    @patch('gppylib.operations.persistent_rebuild.GetDbIdInfo._get_tablespace_to_dboid_map', return_value={})
    def test_get_info_with_single_matching_content_id_and_segment_in_ct(self, mock1, mock2):
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId.return_value = i + 1
            m.getSegmentDbId.return_value = i + 2
            m.getSegmentRole.return_value = 'p' if i < 3 else 'm'
            m.getSegmentStatus.return_value = 'c' if i == 3 else 'u'
            m.getSegmentHostName.return_value = 'mdw1'
            m.getSegmentPort.return_value = 5001 + i
            m.getSegmentFilespaces.return_value = {1000: '/tmp/f1', 1001: '/tmp/f2'}
            m.isSegmentDown.return_value = False
            mock_segs.append(m)
        gparray = Mock()
        gparray.getDbList = Mock()
        gparray.getDbList.return_value = mock_segs
        self.dbid_info.gparray = gparray
        gparray.isSegmentDown = Mock()
        gparray.isSegmentDown.return_value = False

        self.dbid_info.content_id = [1, 10]
        expected = [DbIdInfo(1, 'p', 2, 5001, 'mdw1', {1000: '/tmp/f1', 1001: '/tmp/f2'}, {}, {}, False)]
        self.assertEqual(expected, self.dbid_info.get_info())

    @patch('gppylib.operations.persistent_rebuild.GetDbIdInfo._get_filespace_to_tablespace_map', return_value={})
    @patch('gppylib.operations.persistent_rebuild.GetDbIdInfo._get_tablespace_to_dboid_map', return_value={})
    def test_get_info_with_single_matching_content_id_and_content_down(self, mock1, mock2):
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId.return_value = i + 1
            m.getSegmentDbId.return_value = i + 2
            m.getSegmentRole.return_value = 'p' if i < 3 else 'm'
            m.getSegmentStatus.return_value = 'd' if i == 3 or i == 0 else 'u'
            m.getSegmentHostName.return_value = 'mdw1'
            m.getSegmentPort.return_value = 5001 + i
            m.getSegmentFilespaces.return_value = {1000: '/tmp/f1', 1001: '/tmp/f2'}
            m.isSegmentDown.return_value = False
            mock_segs.append(m)
        gparray = Mock()
        gparray.getDbList = Mock()
        gparray.getDbList.return_value = mock_segs
        self.dbid_info.gparray = gparray
        self.dbid_info.content_id = [1, 10]
        expected = [DbIdInfo(1, 'p', 2, 5001, 'mdw1', {1000: '/tmp/f1', 1001: '/tmp/f2'}, {}, {}, False)]
        self.assertEqual(expected, self.dbid_info.get_info())

    @patch('gppylib.operations.persistent_rebuild.GetDbIdInfo._get_filespace_to_tablespace_map', return_value={})
    @patch('gppylib.operations.persistent_rebuild.GetDbIdInfo._get_tablespace_to_dboid_map', return_value={})
    def test_get_info_with_single_matching_content_id_and_mirror_down(self, mock1, mock2):
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId.return_value = (i + 1) % 3
            m.getSegmentDbId.return_value = i + 2
            m.getSegmentRole.return_value = 'p' if i < 3 else 'm'
            m.getSegmentStatus.return_value = 'd' if i >= 3 else 'u'
            m.getSegmentHostName.return_value = 'mdw1'
            m.getSegmentPort.return_value = 5001 + i
            m.getSegmentFilespaces.return_value = {1000: '/tmp/f1', 1001: '/tmp/f2'}
            # We want to compare from the content ID
            m.isSegmentDown.return_value = True if i >= 3 else False
            mock_segs.append(m)
        gparray = Mock()
        gparray.getDbList = Mock()
        gparray.getDbList.return_value = mock_segs
        self.dbid_info.gparray = gparray
        self.dbid_info.content_id = [2,10]
        expected = [DbIdInfo(2, 'p', 3, 5002, 'mdw1', {1000: '/tmp/f1', 1001: '/tmp/f2'}, {}, {}, False)]
        self.assertEqual(expected, self.dbid_info.get_info())

class BackupPersistentTableFilesTestCase(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        # create persistent table files under new filespace/tablespace/database,
        # and also the default filespace, tablespace/database

        # timestamp: 20140604101010
        try:
            # source files
            os.makedirs(os.path.join('/tmp/p1', '2000', '123'))
            os.makedirs(os.path.join('/tmp/p2', 'base', '234'))
            os.makedirs(os.path.join('/tmp/p2', 'global'))
            os.makedirs(os.path.join('/tmp/p2', 'pg_xlog'))
            os.makedirs(os.path.join('/tmp/p2', 'pg_clog'))
            os.makedirs(os.path.join('/tmp/p2', 'pg_distributedlog'))
            os.makedirs(os.path.join('/tmp/p1', 'empty'))

            open('/tmp/p1/2000/123/5094', 'w').close()
            open('/tmp/p1/2000/123/5094.1', 'w').close()
            open('/tmp/p1/2000/123/5095', 'w').close()
            open('/tmp/p2/base/234/5094', 'w').close()
            open('/tmp/p2/base/234/5095', 'w').close()

            open('/tmp/p2/global/pg_control', 'w').close()
            open('/tmp/p2/global/5090', 'w').close()
            open('/tmp/p2/global/5091', 'w').close()
            open('/tmp/p2/global/5092', 'w').close()
            open('/tmp/p2/global/5093', 'w').close()
            open('/tmp/p2/pg_xlog/0000', 'w').close()
            open('/tmp/p2/pg_clog/0000', 'w').close()
            open('/tmp/p2/pg_distributedlog/000', 'w').close()

            # Backup files
            os.makedirs(os.path.join('/tmp/p1', 'pt_rebuild_bk_20140604101010','2000', '123'))
            os.makedirs(os.path.join('/tmp/p2', 'pt_rebuild_bk_20140604101010', 'base', '234'))
            os.makedirs(os.path.join('/tmp/p2', 'pt_rebuild_bk_20140604101010', 'global'))
            os.makedirs(os.path.join('/tmp/p2', 'pt_rebuild_bk_20140604101010', 'pg_xlog'))
            os.makedirs(os.path.join('/tmp/p2', 'pt_rebuild_bk_20140604101010', 'pg_clog'))
            os.makedirs(os.path.join('/tmp/p2', 'pt_rebuild_bk_20140604101010', 'pg_distributedlog'))

            open('/tmp/p1/pt_rebuild_bk_20140604101010/2000/123/5094', 'w').close()
            open('/tmp/p1/pt_rebuild_bk_20140604101010/2000/123/5094.1', 'w').close()
            open('/tmp/p1/pt_rebuild_bk_20140604101010/2000/123/5095', 'w').close()
            open('/tmp/p2/pt_rebuild_bk_20140604101010/base/234/5094', 'w').close()
            open('/tmp/p2/pt_rebuild_bk_20140604101010/base/234/5095', 'w').close()

            open('/tmp/p2/pt_rebuild_bk_20140604101010/global/pg_control', 'w').close()
            open('/tmp/p2/pt_rebuild_bk_20140604101010/global/5090', 'w').close()
            open('/tmp/p2/pt_rebuild_bk_20140604101010/global/5091', 'w').close()
            open('/tmp/p2/pt_rebuild_bk_20140604101010/global/5092', 'w').close()
            open('/tmp/p2/pt_rebuild_bk_20140604101010/global/5093', 'w').close()
            open('/tmp/p2/pt_rebuild_bk_20140604101010/pg_xlog/0000', 'w').close()
            open('/tmp/p2/pt_rebuild_bk_20140604101010/pg_clog/0000', 'w').close()
            open('/tmp/p2/pt_rebuild_bk_20140604101010/pg_distributedlog/000', 'w').close()

        except OSError:
            pass

    @classmethod
    def tearDownClass(cls):
        try:
            shutil.rmtree('/tmp/p1')
            shutil.rmtree('/tmp/p2')
        except Exception:
            pass

    def setUp(self):
        self.backup_persistent_files = BackupPersistentTableFiles(dbid_info=None,
                                                                  perdb_pt_filenames={2:{17088L:['5094', '5095'],1L: [5094L, 5095L]},
                                                                                      3:{17088L:['5094', '5095'],1L: [5094L, 5095L]}},
                                                                  global_pt_filenames={2: ['5090', '5091', '5092', '5093'],
                                                                                       3: ['5090', '5091', '5092', '5093']},
                                                                  timestamp='20140604101010')

    @patch('os.makedirs')
    def test_copy_files(self, mock1):
        src_ptfiles = ['/tmp/global/5090', '/tmp/global/5091']
        dst_ptfiles = ['/tmp1/global/5090', '/tmp1/global/5091']
        self.backup_persistent_files.pool = Mock()
        content = -1
        actionType = 'backup'
        m = Mock()
        m.validate.return_value = {'/tmp/global/5090': 'abdfe', '/tmp/global/5091': 'abdfe',
                                  '/tmp1/global/5090': 'abdfe', '/tmp1/global/5091': 'abdfe'}
        self.backup_persistent_files.md5_validator = m
        self.backup_persistent_files._copy_files(src_ptfiles, dst_ptfiles, content, actionType)

    @patch('os.makedirs')
    @patch('gppylib.operations.persistent_rebuild.Command.run')
    def test_copy_files_with_restore(self, mock1, mock2):
        src_ptfiles = ['/tmp/global/5090', '/tmp/global/5091']
        dst_ptfiles = ['/tmp1/global/5090', '/tmp1/global/5091']
        self.backup_persistent_files.pool = Mock()
        m = Mock()
        content = -1
        actionType = 'restore'
        m.validate.return_value = {'/tmp/global/5090': 'abdfe', '/tmp/global/5091': 'abdfe',
                                  '/tmp1/global/5090': 'abdfe', '/tmp1/global/5091': 'abdfe'}
        self.backup_persistent_files.md5_validator = m
        self.backup_persistent_files._copy_files(src_ptfiles, dst_ptfiles, content, actionType)

    @patch('os.makedirs')
    def test_copy_files_without_errors_with_no_files(self, mock1):
        src_ptfiles = []
        dst_ptfiles = []
        self.backup_persistent_files.pool = Mock()
        m = Mock()
        content = -1
        actionType = 'backup'
        m.validate.side_effect = [{}, {}]
        self.backup_persistent_files.md5_validator = m
        self.backup_persistent_files._copy_files(src_ptfiles, dst_ptfiles, content, actionType)

    @patch('os.makedirs')
    @patch('gppylib.operations.persistent_rebuild.Command.run')
    def test_copy_files_without_errors_with_no_files_with_restore(self, mock1, mock2):
        src_ptfiles = []
        dst_ptfiles = []
        self.backup_persistent_files.pool = Mock()
        m = Mock()
        content = -1
        actionType = 'restore'
        m.validate.side_effect = [{}, {}]
        self.backup_persistent_files.md5_validator = m
        self.backup_persistent_files.restore=True
        self.backup_persistent_files._copy_files(src_ptfiles, dst_ptfiles, content, actionType)

    @patch('os.makedirs')
    def test_copy_files_with_md5_mismatch(self, mock1):
        src_ptfiles = ['/tmp/global/5090', '/tmp/global/5091']
        dst_ptfiles = ['/tmp1/global/5090', '/tmp1/global/5091']
        self.backup_persistent_files.pool = Mock()
        m = Mock()
        content = -1
        actionType = 'backup'
        m.validate.return_value = {'/tmp/global/5090': 'asdfads', '/tmp/global/5091': 'abdfe',
                                  '/tmp1/global/5090': 'asdfadsf', '/tmp1/global/5091': 'abdfe'}
        self.backup_persistent_files.md5_validator = m
        with self.assertRaisesRegexp(Exception, 'MD5 sums do not match! Expected md5 = "{\'/tmp/global/5090\': \'asdfads\'}",\
 but actual md5 = "{\'/tmp1/global/5090\': \'asdfadsf\'}"'):
            self.backup_persistent_files._copy_files(src_ptfiles, dst_ptfiles, content, actionType)

    @patch('os.makedirs')
    @patch('gppylib.operations.persistent_rebuild.ValidateMD5Sum.validate', return_value={'5090': 'sdfadsf', '5091': 'sdfadsf'})
    def test_copy_files_with_errors(self, mock1, mock2):
        src_ptfiles = ['/tmp/global/5090', '/tmp/global/5091']
        dst_ptfiles = ['/tmp1/global/5090', '/tmp1/global/5091']
        m = Mock()
        content = -1
        actionType = 'backup'
        m.check_results.side_effect = ExecutionError('Error !!!', Mock())
        self.backup_persistent_files.pool = m
        m.validate.return_value = {'5090': 'sdfadsf', '5091': 'sdfadsf'}
        self.backup_persistent_files.md5_validator = m
        with self.assertRaisesRegexp(ExecutionError, 'Error !!!'):
            self.backup_persistent_files._copy_files(src_ptfiles, dst_ptfiles, content, actionType)

    def test_build_PT_src_dest_pairs_filelist_None(self):
        src_dir = ''
        dest_dir = ''
        file_list = None
        self.assertEqual((None, None), self.backup_persistent_files.build_PT_src_dest_pairs(src_dir, dest_dir, file_list))

    def test_build_PT_src_dest_pairs_filelist_Empty(self):
        src_dir = ''
        dest_dir = ''
        file_list = []
        self.assertEqual((None, None), self.backup_persistent_files.build_PT_src_dest_pairs(src_dir, dest_dir, file_list))

    def test_build_PT_src_dest_pairs_non_exist_src_dir(self):
        src_dir = 'tmp'
        dest_dir = '/tmp'
        file_list = ['5090']
        self.assertEqual((None, None), self.backup_persistent_files.build_PT_src_dest_pairs(src_dir, dest_dir, file_list))

    def test_build_PT_src_dest_pairs_empty_src_dir(self):
        src_dir = '/tmp/p1/empty'
        dest_dir = '/tmp/p1/empty'
        file_list = ['5090']
        self.assertEqual((None, None), self.backup_persistent_files.build_PT_src_dest_pairs(src_dir, dest_dir, file_list))

    def test_build_PT_src_dest_pairs_with_file_missed(self):
        src_dir = '/tmp/p1/'
        dest_dir = '/tmp/p1/'
        file_list = ['5555']
        self.assertEqual((None, None), self.backup_persistent_files.build_PT_src_dest_pairs(src_dir, dest_dir, file_list))

    def test_build_PT_src_dest_pairs_with_extended_file_exist(self):
        src_dir = '/tmp/p1/2000/123'
        dest_dir = '/tmp/p1/pt_rebuild_bk_20140604101010/2000/123'
        file_list = ['5094']
        src_files = ['/tmp/p1/2000/123/5094', '/tmp/p1/2000/123/5094.1']
        dest_files = ['/tmp/p1/pt_rebuild_bk_20140604101010/2000/123/5094', '/tmp/p1/pt_rebuild_bk_20140604101010/2000/123/5094.1']
        self.assertEqual((src_files, dest_files), self.backup_persistent_files.build_PT_src_dest_pairs(src_dir, dest_dir, file_list))

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_files')
    def test_copy_global_pt_files(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        self.assertEqual(None, self.backup_persistent_files._copy_global_pt_files())

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles.build_PT_src_dest_pairs', return_value=[None, None])
    def test_copy_global_pt_files_with_restore_with_failure(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'Missing global persistent files from source directory.'):
            self.backup_persistent_files._copy_global_pt_files(restore=True)

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles.build_PT_src_dest_pairs', return_value=[None, None])
    def test_copy_global_pt_files_without_restore_with_failure(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'Missing global persistent files from source directory.'):
            self.backup_persistent_files._copy_global_pt_files()

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_files',
            side_effect=[Mock(), Exception('Error while backing up files')])
    def test_copy_global_pt_files_with_errors(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'Backup of global persistent files failed'):
            self.backup_persistent_files._copy_global_pt_files()

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_files')
    def test_copy_global_pt_files_without_errors(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        self.assertEqual(None, self.backup_persistent_files._copy_global_pt_files())

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_files')
    def test_copy_global_pt_files_with_restore_without_errors(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        self.assertEqual(None, self.backup_persistent_files._copy_global_pt_files(restore=True))

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_files',
            side_effect=[Mock(), Exception('Error while backing up files')])
    def test_copy_global_pt_files_with_restore_with_errors(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'Restore of global persistent files failed'):
            self.backup_persistent_files._copy_global_pt_files(restore=True)

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_files')
    def test_copy_per_db_pt_files(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        self.assertEqual(None, self.backup_persistent_files._copy_per_db_pt_files())

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles.build_PT_src_dest_pairs', return_value=[None, None])
    def test_copy_per_db_pt_files_with_restore_with_failure(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'Missing per-database persistent files from source directory.'):
            self.backup_persistent_files._copy_per_db_pt_files(restore=True)

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles.build_PT_src_dest_pairs', return_value=[None, None])
    def test_copy_per_db_pt_files_without_restore_with_failure(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'Missing per-database persistent files from source directory.'):
            self.backup_persistent_files._copy_per_db_pt_files()

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_files',
            side_effect=[Mock(), Exception('Error while backing up files')])
    def test_copy_per_db_pt_files_with_errors(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'Backup of per database persistent files failed'):
            self.backup_persistent_files._copy_per_db_pt_files()

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_files')
    def test_copy_per_db_pt_files_without_errors(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        self.assertEqual(None, self.backup_persistent_files._copy_per_db_pt_files())

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_files')
    def test_copy_per_db_pt_files_with_unused_filespace(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        self.assertEqual(None, self.backup_persistent_files._copy_per_db_pt_files())

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_files')
    def test_copy_per_db_pt_files_with_unused_tablespace(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        self.assertEqual(None, self.backup_persistent_files._copy_per_db_pt_files())

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_files')
    def test_copy_per_db_pt_files_with_restore_without_errors(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        self.assertEqual(None, self.backup_persistent_files._copy_per_db_pt_files(restore=True))

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_files')
    def test_copy_Xactlog_files_without_restore_without_errors(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        self.assertEqual(None, self.backup_persistent_files._copy_Xactlog_files())

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles.build_Xactlog_src_dest_pairs', return_value=[[],[]])
    def test_copy_Xactlog_files_without_restore_with_failure(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'should not be empty'):
            self.backup_persistent_files._copy_Xactlog_files()

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles.build_Xactlog_src_dest_pairs', return_value=[[],[]])
    def test_copy_Xactlog_files_with_restore_with_failure(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'should not be empty'):
            self.backup_persistent_files._copy_Xactlog_files(restore=True)

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_files')
    def test_copy_Xactlog_files_with_restore_without_errors(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        self.assertEqual(None, self.backup_persistent_files._copy_Xactlog_files(restore=True))

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_files')
    def test_copy_pg_control_files_without_restore_without_errors(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        self.assertEqual(None, self.backup_persistent_files._copy_pg_control_file())

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_files')
    def test_copy_pg_control_files_with_restore_without_errors(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        self.assertEqual(None, self.backup_persistent_files._copy_pg_control_file(restore=True))

    @patch('os.path.isfile', return_value=False)
    def test_copy_pg_control_files_without_restore_with_failure(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'Global pg_control file is missing from source directory'):
            self.backup_persistent_files._copy_pg_control_file()

    @patch('os.path.isfile', return_value=False)
    def test_copy_pg_control_files_with_restore_with_failure(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'Global pg_control file is missing from backup directory'):
            self.backup_persistent_files._copy_pg_control_file(restore=True)

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_files',
            side_effect=[Mock(), Mock(), Mock(), Exception('Error while backing up files')])
    def test_copy_per_db_pt_files_with_restore_with_errors(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'Restore of per database persistent files failed'):
            self.backup_persistent_files._copy_per_db_pt_files(restore=True)

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_files')
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_global_pt_files', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_per_db_pt_files', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_Xactlog_files', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_pg_control_file', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.WorkerPool')
    @patch('gppylib.operations.persistent_rebuild.Command')
    @patch('gppylib.operations.persistent_rebuild.ValidateMD5Sum.init')
    def test_restore_without_errors(self, mock1, mock2, mock3, mock4, mock5, mock6, mock7, mock8):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        self.assertEqual(None, self.backup_persistent_files.restore())

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_global_pt_files', side_effect=Exception('Error'))
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_per_db_pt_files', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_Xactlog_files', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_pg_control_file', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.WorkerPool')
    @patch('gppylib.operations.persistent_rebuild.Command')
    @patch('gppylib.operations.persistent_rebuild.ValidateMD5Sum.init')
    def test_restore_with_global_file_bkup_error(self, mock1, mock2, mock3, mock4, mock5, mock6, mock7):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'Error'):
            self.backup_persistent_files.restore()

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_global_pt_files', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_per_db_pt_files', side_effect=Exception('Error'))
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_Xactlog_files', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_pg_control_file', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.WorkerPool')
    @patch('gppylib.operations.persistent_rebuild.Command')
    @patch('gppylib.operations.persistent_rebuild.ValidateMD5Sum.init')
    def test_restore_with_per_db_bkup_error(self, mock1, mock2, mock3, mock4, mock5, mock6, mock7):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'Error'):
            self.backup_persistent_files.restore()

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_global_pt_files', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_per_db_pt_files', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_Xactlog_files', side_effect=Exception('Error'))
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_pg_control_file', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.WorkerPool')
    @patch('gppylib.operations.persistent_rebuild.Command')
    @patch('gppylib.operations.persistent_rebuild.ValidateMD5Sum.init')
    def test_restore_with_xlog_bkup_error(self, mock1, mock2, mock3, mock4, mock5, mock6, mock7):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'Error'):
            self.backup_persistent_files.restore()

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_global_pt_files', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_per_db_pt_files', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_Xactlog_files', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_pg_control_file', side_effect=Exception('Error'))
    @patch('gppylib.operations.persistent_rebuild.WorkerPool')
    @patch('gppylib.operations.persistent_rebuild.Command')
    @patch('gppylib.operations.persistent_rebuild.ValidateMD5Sum.init')
    def test_restore_with_pg_control_bkup_error(self, mock1, mock2, mock3, mock4, mock5, mock6, mock7):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'Error'):
            self.backup_persistent_files.restore()

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_global_pt_files', side_effect=Exception('Error'))
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_per_db_pt_files', side_effect=Exception('Error'))
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_Xactlog_files', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_pg_control_file', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.WorkerPool')
    @patch('gppylib.operations.persistent_rebuild.Command')
    @patch('gppylib.operations.persistent_rebuild.ValidateMD5Sum.init')
    def test_restore_with_global_and_per_db_bkup_error(self, mock1, mock2, mock3, mock4, mock5, mock6, mock7):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'Error'):
            self.backup_persistent_files.restore()

    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_global_pt_files', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_per_db_pt_files', return_value=None)
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_Xactlog_files', side_effect=Exception('Error'))
    @patch('gppylib.operations.persistent_rebuild.BackupPersistentTableFiles._copy_pg_control_file', side_effect=Exception('Error'))
    @patch('gppylib.operations.persistent_rebuild.WorkerPool')
    @patch('gppylib.operations.persistent_rebuild.Command')
    @patch('gppylib.operations.persistent_rebuild.ValidateMD5Sum.init')
    def test_restore_with_xlog_and_pg_control_bkup_error(self, mock1, mock2, mock3, mock4, mock5, mock6, mock7):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.backup_persistent_files.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'Error'):
            self.backup_persistent_files.restore()

class RebuildTableTestCase(unittest.TestCase):
    def setUp(self):
        self.rebuild_table = RebuildTable(dbid_info=None)

    def test_initializer_captures_values(self):
        self.rebuild_table = RebuildTable(dbid_info="abcd", has_mirrors="efg", batch_size=123, backup_dir=456)
        self.assertEquals("abcd",self.rebuild_table.dbid_info)
        self.assertEquals("efg",self.rebuild_table.has_mirrors)
        self.assertEquals(123,self.rebuild_table.batch_size)
        self.assertEquals(456,self.rebuild_table.backup_dir)

    def test_get_valid_dbids(self):
        content_ids = [1, 2]
        expected = [0, 1]
        mock_segs = []
        for i in range(2):
            m = Mock()
            m.getSegmentContentId.return_value = i + 1
            m.getSegmentRole.return_value = 'p'
            m.getSegmentDbId.return_value = i
            m.getSegmentPort.return_value = 5000 + i
            m.getSegmentHostName.return_value = 'mdw%d' % (i + 1)
            m.getSegmentStatus.return_value = 'u'
            mock_segs.append(m)
        m = Mock()
        m.getDbList.return_value = mock_segs
        self.rebuild_table.gparray = m
        self.assertEqual(expected, self.rebuild_table._get_valid_dbids(content_ids))

    def test_get_valid_dbids_empty_contents(self):
        content_ids = []
        expected = []
        mock_segs = []
        for i in range(2):
            m = Mock()
            m.getSegmentContentId.return_value = i + 1
            m.getSegmentRole.return_value = 'p'
            m.getSegmentDbId.return_value = i
            m.getSegmentPort.return_value = 5000 + i
            m.getSegmentHostName.return_value = 'mdw%d' % (i + 1)
            m.getSegmentStatus.return_value = 'u'
            mock_segs.append(m)
        m = Mock()
        m.getDbList.return_value = mock_segs
        self.rebuild_table.gparray = m
        self.assertEqual(expected, self.rebuild_table._get_valid_dbids(content_ids))

    def test_get_valid_dbids_non_matching_content_ids(self):
        content_ids = [3, 4, 5]
        expected = []
        mock_segs = []
        for i in range(2):
            m = Mock()
            m.getSegmentContentId.return_value = i + 1
            m.getSegmentRole.return_value = 'p'
            m.getSegmentDbId.return_value = i
            m.getSegmentPort.return_value = 5000 + i
            m.getSegmentHostName.return_value = 'mdw%d' % (i + 1)
            m.getSegmentStatus.return_value = 'u'
            mock_segs.append(m)
        m = Mock()
        m.getDbList.return_value = mock_segs
        self.rebuild_table.gparray = m
        self.assertEqual(expected, self.rebuild_table._get_valid_dbids(content_ids))

    def test_get_valid_dbids_content_ids_down(self):
        content_ids = [1, 2, 3]
        mock_segs = []
        for i in range(2):
            m = Mock()
            m.getSegmentContentId.return_value = i + 1
            m.getSegmentRole.return_value = 'p'
            m.getSegmentDbId.return_value = i
            m.getSegmentPort.return_value = 5000 + i
            m.getSegmentHostName.return_value = 'mdw%d' % (i + 1)
            m.getSegmentStatus.return_value = 'u' if i % 2 else 'd'
            mock_segs.append(m)
        m = Mock()
        m.getDbList.return_value = mock_segs
        self.rebuild_table.gparray = m
        with self.assertRaisesRegexp(Exception, 'Segment .* is down. Cannot continue with persistent table rebuild'):
            self.rebuild_table._get_valid_dbids(content_ids)

    def test_get_valid_dbids_content_ids_resync(self):
        content_ids = [1, 2, 3]
        mock_segs = []
        for i in range(2):
            m = Mock()
            m.getSegmentContentId.return_value = i + 1
            m.getSegmentRole.return_value = 'p'
            m.getSegmentDbId.return_value = i
            m.getSegmentPort.return_value = 5000 + i
            m.getSegmentHostName.return_value = 'mdw%d' % (i + 1)
            m.getSegmentStatus.return_value = 'u'
            m.getSegmentMode.return_value = 'r' if i % 2 else 's'
            mock_segs.append(m)
        m = Mock()
        m.getDbList.return_value = mock_segs
        self.rebuild_table.gparray = m
        with self.assertRaisesRegexp(Exception, 'Segment .* is in resync. Cannot continue with persistent table rebuild'):
            self.rebuild_table._get_valid_dbids(content_ids)

    @patch('gppylib.operations.persistent_rebuild.ValidatePersistentBackup.validate_backups', return_value=Mock())
    def test_get_valid_dbids_content_ids_are_mirrors(self, mock1):
        content_ids = [1, 2, 3]
        expected = [1]
        mock_segs = []
        for i in range(2):
            m = Mock()
            m.getSegmentContentId.return_value = i + 1
            m.getSegmentRole.return_value = 'p' if i % 2 else 'm'
            m.getSegmentDbId.return_value = i
            m.getSegmentPort.return_value = 5000 + i
            m.getSegmentHostName.return_value = 'mdw%d' % (i + 1)
            m.getSegmentStatus.return_value = 'u'
            mock_segs.append(m)
        m = Mock()
        m.getDbList.return_value = mock_segs
        self.rebuild_table.gparray = m
        self.assertEqual(expected, self.rebuild_table._get_valid_dbids(content_ids))

    @patch('gppylib.operations.persistent_rebuild.GpArray')
    @patch('gppylib.operations.persistent_rebuild.RebuildTable._validate_backups')
    @patch('gppylib.operations.persistent_rebuild.RebuildTable._get_valid_dbids', return_value=[1, 2, 3])
    @patch('gppylib.operations.persistent_rebuild.ParallelOperation.run')
    def test_rebuild(self, mock1, mock2, mock3, mock4):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.rebuild_table.dbid_info = [d1, d2]
        expected_success = [d1, d2]
        expected_failure = []
        self.assertEqual((expected_success, expected_failure), self.rebuild_table.rebuild())

    @patch('gppylib.operations.persistent_rebuild.GpArray')
    @patch('gppylib.operations.persistent_rebuild.RebuildTable._validate_backups')
    @patch('gppylib.operations.persistent_rebuild.RebuildTable._get_valid_dbids', return_value=[1, 2, 3])
    @patch('gppylib.operations.persistent_rebuild.ParallelOperation.run')
    @patch('gppylib.operations.persistent_rebuild.RemoteOperation.get_ret', side_effect=[Mock(), Exception('Error')])
    def test_rebuild_with_errors(self, mock1, mock2, mock3, mock4, mock5):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.rebuild_table.dbid_info = [d1, d2]
        expected_success = [d1]
        expected_failure = [(d2, 'Error')]
        self.assertEqual((expected_success, expected_failure), self.rebuild_table.rebuild())

class ValidatePersistentBackupTestCase(unittest.TestCase):
    def setUp(self):
        self.validate_persistent_backup = ValidatePersistentBackup(dbid_info=None, timestamp='20140605101010')

    def test_process_results(self):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        m1 = Mock()
        m1.get_results.return_value = CommandResult(0, '/tmp/f1', '', True, False)
        m1.cmdStr = "find /tmp/f1 -name pt_rebuild_bk_"
        m2 = Mock()
        m2.get_results.return_value = CommandResult(0, '/tmp/f1', '', True, False)
        m2.cmdStr = "find /tmp/f1 -name pt_rebuild_bk_"
        m = Mock()
        m.getCompletedItems.return_value = [m1, m2]
        self.validate_persistent_backup._process_results(d1, m)

    def test_process_results_with_errors(self):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        m1 = Mock()
        m1.get_results.return_value = CommandResult(0, '/tmp/f1', '', True, False)
        m1.cmdStr = "find /tmp/f1 -name pt_rebuild_bk_"
        m2 = Mock()
        m2.get_results.return_value = CommandResult(1, '/tmp/f1', '', True, False)
        m2.cmdStr = "find /tmp/f1 -name pt_rebuild_bk_"
        m = Mock()
        m.getCompletedItems.return_value = [m1, m2]
        with self.assertRaisesRegexp(Exception, 'Failed to validate backups'):
            self.validate_persistent_backup._process_results(d1, m)

    def test_process_results_with_missing_backup(self):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        m1 = Mock()
        m1.get_results.return_value = CommandResult(0, '/tmp/f1', '', True, False)
        m1.cmdStr = "find /tmp/f1 -name pt_rebuild_bk_"
        m2 = Mock()
        m2.get_results.return_value = CommandResult(0, '', '', True, False)
        m2.cmdStr = "find /foo/bar -name pt_rebuild_bk_"
        m = Mock()
        m.getCompletedItems.return_value = [m1, m2]
        with self.assertRaisesRegexp(Exception, 'Failed to validate backups'):
            self.validate_persistent_backup._process_results(d1, m)

    @patch('gppylib.operations.persistent_rebuild.WorkerPool')
    def test_validate(self, mock1):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.validate_persistent_backup.dbid_info = [d1, d2]
        self.validate_persistent_backup.validate_backups()

    @patch('gppylib.operations.persistent_rebuild.WorkerPool')
    @patch('gppylib.operations.persistent_rebuild.ValidatePersistentBackup._process_results', side_effect=Exception('Failed to validate backups'))
    def test_validate_error_in_workerpool(self, mock1, mock2):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.validate_persistent_backup.dbid_info = [d1, d2]
        with self.assertRaisesRegexp(Exception, 'Failed to validate backups'):
            self.validate_persistent_backup.validate_backups()

class RunBackupRestoreTestCase(unittest.TestCase):
    def setUp(self):
        self.run_backup_restore = RunBackupRestore(dbid_info=None, timestamp=None)

    @patch('gppylib.operations.persistent_rebuild.WorkerPool')
    @patch('gppylib.operations.persistent_rebuild.RunBackupRestore._process_results')
    def test_run_backup_restore(self, mock1, mock2):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        host_to_dbid_info_map = {'h1': [d1], 'h2': [d2]}
        self.run_backup_restore._run_backup_restore(host_to_dbid_info_map)

    @patch('gppylib.operations.persistent_rebuild.WorkerPool')
    @patch('gppylib.operations.persistent_rebuild.RunBackupRestore._process_results', side_effect=Exception('ERROR'))
    def test_run_backup_restore_with_errors(self, mock1, mock2):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        host_to_dbid_info_map = {'h1': [d1], 'h2': [d2]}
        with self.assertRaisesRegexp(Exception, 'ERROR'):
            self.run_backup_restore._run_backup_restore(host_to_dbid_info_map)

    @patch('gppylib.operations.persistent_rebuild.WorkerPool')
    @patch('gppylib.operations.persistent_rebuild.RunBackupRestore._process_results')
    def test_run_backup_restore_with_restore(self, mock1, mock2):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        host_to_dbid_info_map = {'h1': [d1], 'h2': [d2]}
        self.run_backup_restore._run_backup_restore(host_to_dbid_info_map, restore=True)

    @patch('gppylib.operations.persistent_rebuild.WorkerPool')
    @patch('gppylib.operations.persistent_rebuild.RunBackupRestore._process_results', side_effect=Exception('ERROR'))
    def test_run_backup_restore_with_errors_with_restore(self, mock1, mock2):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        host_to_dbid_info_map = {'h1': [d1], 'h2': [d2]}
        with self.assertRaisesRegexp(Exception, 'ERROR'):
            self.run_backup_restore._run_backup_restore(host_to_dbid_info_map, restore=True)

    @patch('gppylib.operations.persistent_rebuild.WorkerPool')
    @patch('gppylib.operations.persistent_rebuild.RunBackupRestore._process_results')
    def test_run_backup_restore_with_validate(self, mock1, mock2):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        host_to_dbid_info_map = {'h1': [d1], 'h2': [d2]}
        self.run_backup_restore._run_backup_restore(host_to_dbid_info_map, validate_backups=True)

    @patch('gppylib.operations.persistent_rebuild.WorkerPool')
    @patch('gppylib.operations.persistent_rebuild.RunBackupRestore._process_results', side_effect=Exception('ERROR'))
    def test_run_backup_restore_with_errors_with_validate(self, mock1, mock2):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        host_to_dbid_info_map = {'h1': [d1], 'h2': [d2]}
        with self.assertRaisesRegexp(Exception, 'ERROR'):
            self.run_backup_restore._run_backup_restore(host_to_dbid_info_map, validate_backups=True)

    def test_get_host_to_dbid_info_map(self):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h2', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        expected = {'h1': [d1], 'h2': [d2]}
        self.run_backup_restore.dbid_info = [d1, d2]
        self.assertEqual(expected, self.run_backup_restore._get_host_to_dbid_info_map())

    def test_get_host_to_dbid_info_map_empty(self):
        self.run_backup_restore.dbid_info = []
        self.assertEqual({}, self.run_backup_restore._get_host_to_dbid_info_map())

    def test_get_host_to_dbid_info_map_multiple_entries_per_host(self):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        d2 = DbIdInfo(2, 'p', 3, 5002, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        expected = {'h1': [d1, d2]}
        self.run_backup_restore.dbid_info = [d1, d2]
        self.assertEqual(expected, self.run_backup_restore._get_host_to_dbid_info_map())

    def test_process_results(self):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        m1 = Mock()
        m1.get_results.return_value = CommandResult(0, '/tmp/f1', '', True, False)
        m2 = Mock()
        m2.get_results.return_value = CommandResult(0, '/tmp/f1', '', True, False)
        m = Mock()
        m.getCompletedItems.return_value = [m1, m2]
        self.run_backup_restore._process_results(m, 'ERR')

    def test_process_results_with_errors(self):
        d1 = DbIdInfo(1, 'p', 2, 5001, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        m1 = Mock()
        m1.get_results.return_value = CommandResult(0, '/tmp/f1', '', True, False)
        m2 = Mock()
        m2.get_results.return_value = CommandResult(1, 'ERR', '', True, False)
        m = Mock()
        m.getCompletedItems.return_value = [m1, m2]
        with self.assertRaisesRegexp(Exception, 'ERR'):
            self.run_backup_restore._process_results(m, 'ERR')

class ValidateMD5SumTestCase(unittest.TestCase):
    def setUp(self):
        self.validate_md5sum = ValidateMD5Sum(pool=None)

    @patch('platform.system', return_value='Darwin')
    def test_get_md5_prog_for_osx(self, mock1):
        self.assertEqual('md5', self.validate_md5sum._get_md5_prog())

    @patch('platform.system', return_value='Linux')
    def test_get_md5_prog_for_linux(self, mock1):
        self.assertEqual('md5sum', self.validate_md5sum._get_md5_prog())

    @patch('platform.system', return_value='Solaris')
    def test_get_md5_prog_for_invalid_os(self, mock1):
        with self.assertRaisesRegexp(Exception, 'Cannot determine the md5 program since Solaris platform is not supported'):
            self.validate_md5sum._get_md5_prog()

    @patch('platform.system', return_value='Darwin')
    def test_get_md5_results_pat_for_osx(self, mock1):
        pat = re.compile('MD5 \((.*)\) = (.*)')
        self.assertEqual(pat, self.validate_md5sum._get_md5_results_pat())

    @patch('platform.system', return_value='Linux')
    def test_get_md5_results_pat_for_osx(self, mock1):
        pat = re.compile('(.*) (.*)')
        self.assertEqual(pat, self.validate_md5sum._get_md5_results_pat())

    @patch('platform.system', return_value='Solaris')
    def test_get_md5_results_pat_for_invalid_os(self, mock1):
        with self.assertRaisesRegexp(Exception, 'Cannot determine the pattern for results of md5 program since Solaris platform is not supported'):
            self.validate_md5sum._get_md5_results_pat()

    @patch('platform.system', return_value='Darwin')
    def test_process_results_on_osx(self, mock1):
        m = Mock()
        m1 = Mock()
        m1.get_results.return_value = CommandResult(0, 'MD5 (foo) = afsdfasdf', '', True, False)
        m2 = Mock()
        m2.get_results.return_value = CommandResult(0, 'MD5 (foo1) = sdfadsff', '', True, False)
        m.getCompletedItems.return_value = [m1, m2]
        self.validate_md5sum.pool = m
        self.validate_md5sum.md5_results_pat = re.compile('MD5 \((.*)\) = (.*)')
        expected = {'foo': 'afsdfasdf', 'foo1': 'sdfadsff'}
        self.assertEqual(expected, self.validate_md5sum._process_md5_results())

    @patch('platform.system', return_value='Darwin')
    def test_process_results_on_osx_with_error(self, mock1):
        m = Mock()
        m1 = Mock()
        m1.get_results.return_value = CommandResult(0, 'MD5 (foo1) = sdfadsff', '', True, False)
        m2 = Mock()
        m2.get_results.return_value = CommandResult(1, '', 'Error', True, False)
        m.getCompletedItems.return_value = [m1, m2]
        self.validate_md5sum.pool = m
        self.validate_md5sum.md5_results_pat = re.compile('MD5 \((.*)\) = (.*)')
        with self.assertRaisesRegexp(Exception, 'Unable to calculate md5sum'):
            self.validate_md5sum._process_md5_results()

    @patch('platform.system', return_value='Linux')
    def test_process_results_on_linux(self, mock1):
        m = Mock()
        m1 = Mock()
        m1.get_results.return_value = CommandResult(0, 'afsdfasdf foo', '', True, False)
        m2 = Mock()
        m2.get_results.return_value = CommandResult(0, 'sdfadsff foo1', '', True, False)
        m.getCompletedItems.return_value = [m1, m2]
        self.validate_md5sum.pool = m
        self.validate_md5sum.md5_results_pat = re.compile('(.*) (.*)')
        expected = {'foo': 'afsdfasdf', 'foo1': 'sdfadsff'}
        self.assertEqual(expected, self.validate_md5sum._process_md5_results())

    @patch('platform.system', return_value='Linux')
    def test_process_results_on_linux_with_error(self, mock1):
        m = Mock()
        m1 = Mock()
        m1.get_results.return_value = CommandResult(0, 'sdfadsff fo1', '', True, False)
        m2 = Mock()
        m2.get_results.return_value = CommandResult(1, '', 'Error', True, False)
        m.getCompletedItems.return_value = [m1, m2]
        self.validate_md5sum.pool = m
        self.validate_md5sum.md5_results_pat = re.compile('(.*) (.*)')
        with self.assertRaisesRegexp(Exception, 'Unable to calculate md5sum'):
            self.validate_md5sum._process_md5_results()

class RebuildPersistentTableTestCase(unittest.TestCase):
    def setUp(self):
        self.rebuild_persistent_table = RebuildPersistentTables(content_id = None,
                                                                contentid_file = None,
                                                                backup=None,
                                                                restore=None,
                                                                batch_size=None,
                                                                backup_dir=None)

    @patch('gppylib.operations.persistent_rebuild.platform.system', return_value='Linux')
    def test_check_platform_linux(self, mock1):
        self.rebuild_persistent_table._check_platform()

    @patch('gppylib.operations.persistent_rebuild.platform.system', return_value='Solaris')
    def test_check_platform_non_linux(self, mock1):
        with self.assertRaisesRegexp(Exception, 'This tool is only supported on Linux and OSX platforms'):
            self.rebuild_persistent_table._check_platform()

    def test_validate_has_mirrors_and_standby(self):
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId.return_value = i - 1
            m.isSegmentMirror.return_value = True if i < 3 else False
            mock_segs.append(m)
        m = Mock()
        m.getDbList.return_value = mock_segs
        self.rebuild_persistent_table.gparray = m
        self.rebuild_persistent_table._validate_has_mirrors_and_standby()
        self.assertTrue(self.rebuild_persistent_table.has_mirrors)

    def test_validate_has_mirrors_and_standby_with_no_mirrors(self):
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId.return_value = i - 1
            m.isSegmentMirror.return_value = False
            mock_segs.append(m)
        m = Mock()
        m.getDbList.return_value = mock_segs
        self.rebuild_persistent_table.gparray = m
        self.rebuild_persistent_table._validate_has_mirrors_and_standby()
        self.assertFalse(self.rebuild_persistent_table.has_mirrors)

    def test_validate_has_mirrors_and_standby_with_mirrors_for_master(self):
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId.return_value = i - 1
            m.isSegmentMirror.return_value = True if i == -1 else False
            mock_segs.append(m)
        m = Mock()
        m.getDbList.return_value = mock_segs
        self.rebuild_persistent_table.gparray = m
        self.rebuild_persistent_table._validate_has_mirrors_and_standby()
        self.assertTrue(self.rebuild_persistent_table.has_standby)

    @patch('gppylib.operations.persistent_rebuild.findCmdInPath', return_value=True)
    def test_check_md5_prog(self, mock1):
        self.rebuild_persistent_table._check_md5_prog()

    @patch('gppylib.operations.persistent_rebuild.findCmdInPath', return_value=False)
    def test_check_md5_prog_no_md5(self, mock1):
        with self.assertRaisesRegexp(Exception, 'Unable to find md5.* program. Please make sure it is in PATH'):
            self.rebuild_persistent_table._check_md5_prog()

    @patch('gppylib.operations.persistent_rebuild.GpVersion.local', return_value=GpVersion('4.2.7.3'))
    def test_check_database_version(self, mock1):
        self.rebuild_persistent_table._check_database_version()

    @patch('gppylib.operations.persistent_rebuild.GpVersion.local', return_value=GpVersion('4.0.1.0'))
    def test_check_database_version_with_lower_version(self, mock1):
        with self.assertRaisesRegexp(Exception, 'This tool is not supported on Greenplum version lower than 4.1.0.0'):
            self.rebuild_persistent_table._check_database_version()

    @patch('gppylib.operations.persistent_rebuild.dbconn.execSQL', side_effect=[[[5090], [5091], [5092], [5093]], [[123, 'template1']],
                                                                               [[5094], [16992]]])
    @patch('gppylib.operations.persistent_rebuild.dbconn.connect')
    @patch('gppylib.operations.persistent_rebuild.dbconn.DbURL')
    def test_get_persistent_table_filenames(self, mock1, mock2, mock3):
        d1 = DbIdInfo(2, 'p', 3, 5002, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        self.rebuild_persistent_table.dbid_info = [d1]
        self.rebuild_persistent_table._get_persistent_table_filenames()
        expected_global = defaultdict(defaultdict)
        expected_files = ['5090', '5091', '5092', '5093']
        expected_dbid = {3:expected_files}
        expected_global = {'h1':expected_dbid}
        expected_perdb_pt_files = defaultdict(defaultdict)
        exp_pt_files = ['5094', '16992']
        exp_dboid = {123:exp_pt_files}
        exp_dbid = {3:exp_dboid}
        expected_perdb_pt_file = {'h1':exp_dbid}
        from gppylib.operations.persistent_rebuild import GLOBAL_PERSISTENT_FILES, PER_DATABASE_PERSISTENT_FILES
        self.assertEqual(GLOBAL_PERSISTENT_FILES, expected_global)
        self.assertEqual(PER_DATABASE_PERSISTENT_FILES, expected_perdb_pt_file)

    @patch('gppylib.operations.persistent_rebuild.dbconn.execSQL', side_effect=pt_query_side_effect)
    @patch('gppylib.operations.persistent_rebuild.dbconn.connect')
    @patch('gppylib.operations.persistent_rebuild.dbconn.DbURL')
    def test_get_persistent_table_filenames_lacking_global_relfilenode(self, mock1, mock2, mock3):
        d1 = DbIdInfo(2, 'p', 3, 5002, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        global remove_global_pt_entry
        remove_global_pt_entry = True
        self.rebuild_persistent_table.dbid_info = [d1]
        with self.assertRaisesRegexp(Exception, 'Missing relfilenode entry of global pesistent tables in pg_class'):
            self.rebuild_persistent_table._get_persistent_table_filenames()
        remove_global_pt_entry = False

    @patch('gppylib.operations.persistent_rebuild.dbconn.execSQL', side_effect=pt_query_side_effect)
    @patch('gppylib.operations.persistent_rebuild.dbconn.connect')
    @patch('gppylib.operations.persistent_rebuild.dbconn.DbURL')
    def test_get_persistent_table_filenames_lacking_per_database_relfilenode(self, mock1, mock2, mock3):
        d1 = DbIdInfo(2, 'p', 3, 5002, 'h1', {1000: '/tmp/p1', 3052: '/tmp/p2'}, {1000: [2000], 3052: [2001]}, {2000: [123], 2001: [234]}, False)
        global remove_per_db_pt_entry
        remove_per_db_pt_entry = True
        self.rebuild_persistent_table.dbid_info = [d1]
        with self.assertRaisesRegexp(Exception, 'Missing relfilenode entry of per database persistent tables in pg_class'):
            self.rebuild_persistent_table._get_persistent_table_filenames()
        remove_per_db_pt_entry = False

if __name__ == '__main__':
    unittest.main()
