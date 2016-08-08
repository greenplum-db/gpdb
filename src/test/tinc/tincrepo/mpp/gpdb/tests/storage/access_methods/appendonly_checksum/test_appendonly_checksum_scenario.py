"""
Copyright (C) 2004-2015 Pivotal Software, Inc. All rights reserved.

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

import os
import glob
import sys
import tinctest
import commands
from mpp.lib.PSQL import PSQL
from mpp.models import MPPTestCase
from tinctest.lib import local_path, Gpdiff
from tinctest.models.scenario import ScenarioTestCase
from mpp.gpdb.tests.storage.lib.gp_filedump import GpfileTestCase
from gppylib.commands.base import Command, ExecutionError

class CONST(object):
    ''' For test_ao_checksum_corruption '''
    HEADER_OFFSET = 8
    DATA_OFFSET = -3
    FIND_CHAR = 'a' 
    CORRUPTION = '*'
    ERR_MSG = 'ERROR:  Header checksum does not match'
    BOF = 0
    EOF = 2
    ''' For test_aochecksum_size '''
    CHKSUM_OFF = 'off'
    CHKSUM_ON = 'on'
    SMALL_HEADER_LEN_WITH_CHKSUM_ON = 'headerLen: 24'
    SMALL_HEADER_LEN_WITH_CHKSUM_OFF = 'headerLen: 16'
    LARGE_HEADER_LEN_WITH_CHKSUM_ON = 'headerLen: 24'
    LARGE_HEADER_LEN_WITH_CHKSUM_OFF = 'headerLen: 16'
    BULKDENSE_HEADER_LEN_WITH_CHKSUM_ON = 'headerLen: 32'
    BULKDENSE_HEADER_LEN_WITH_CHKSUM_OFF = 'headerLen: 24'
    SMALL_HEADER_TYPE = 'AoHeaderKind_SmallContent'
    LARGE_HEADER_TYPE = 'AoHeaderKind_LargeContent'
    BULKDENSE_HEADER_TYPE = 'DatumStreamVersion_Dense'


class AppendonlyChecksumTestCase(ScenarioTestCase, MPPTestCase):
    '''
    
    @gucs gp_create_table_random_default_distribution=off
    @dbname gptest
    @description Test for Appendonly Checksum
    @product_version gpdb: [4.3.4.0-]
    '''
    def __init__(self, methodName):

        super(AppendonlyChecksumTestCase,self).__init__(methodName)
        self.gpfile = GpfileTestCase()
        self.dbname=os.environ.get('PGDATABASE')


    @classmethod
    def setUpClass(cls):
        base_dir = os.path.dirname(sys.modules[cls.__module__].__file__)
        test_out_dir=os.path.join( base_dir,"output_scenario")
        try:
            os.mkdir(test_out_dir)
        except OSError as e:
            tinctest.logger.info( "create output_scenario dir error %s " % format(e.strerror))

    @classmethod 
    def create_table(cls,tab_name):
        base_dir = os.path.dirname(sys.modules[cls.__module__].__file__)
        sql_file = os.path.join( base_dir, "sql_scenario", tab_name + ".sql");    
        ans_file = os.path.join( base_dir, "expected_scenario", tab_name + ".ans");    
        out_file = os.path.join( base_dir, "output_scenario", tab_name + ".out");    
        ''' Run the provided sql and validate it '''
        PSQL.run_sql_file(sql_file,out_file=out_file)
        ''' Compare the out and ans files '''
        result = Gpdiff.are_files_equal(out_file, ans_file)
        errmsg='Gpdiff are not equal for file '+ sql_file
        assert result, errmsg

    @classmethod
    def select_table(cls,tab_name, noncorrupted_checksum=True):
        base_dir = os.path.dirname(sys.modules[cls.__module__].__file__)
        sql_file = os.path.join( base_dir, "sql_scenario", tab_name + ".sql");
        ans_file = os.path.join( base_dir, "expected_scenario", tab_name + ".ans");
        out_file = os.path.join( base_dir, "output_scenario", tab_name + ".out");
        ''' Run the provided sql and validate it '''
        PSQL.run_sql_file(sql_file,out_file=out_file)
        if noncorrupted_checksum:
            ''' Compare the out and ans files '''
            result = Gpdiff.are_files_equal(out_file, ans_file)
            errmsg='Gpdiff are not equal for file '+ sql_file
            assert result, errmsg
        else:
            ''' Verification for corruption case '''
            find_msg="grep -c " +"'"+ CONST.ERR_MSG + "' "+out_file
            tinctest.logger.info('Find Error Message : %s ' % find_msg)
            status, result=commands.getstatusoutput(find_msg)
            assert(result > 0),"Did not find message " + msg

    def test_ao_checksum_corruption(self):
            ''' 
            The following test simulates corruption of header and data content and verifies the select behavior 
            on this corrupted  table.
            The test also verifies the behavior of GUC gp_appendonly_verify_block_checksums(on/off) if the last data-block is
            corrupted
            PARAMETERS (via data_provider):
            0. Test Name
            1. Sql file to create scenario. This is also used in verification because for every test sql file 
               there is  also a count_ sql file. (verify_sql variable below is populated with this value for each test)
            2. Starting position in data file from where position will be calculated for corruption
            3. (For appendonly_verify_block_checksums_co test only) The character in the data file that will be 
               flipped with uppercase to simulate the corruption of the last record, Resulting in different checksum
            3. (All others) Location in data file that will be corrupted. CONST.CORRUPTION will be used as new character 
               that will be  used to overwrite a location in data file,resulting in different checksum 

            STEPS :
            1. Create table of required type of header (large header_content or small header_content (ao/co)
            2. Find the data file for this relationship
            3. Take a backup before corruption
            4. For appendonly_verify_block_checksums_co test, from the eof find the first occurance of CONST.FIND_CHAR
               For all others from corruption_start position find corruption_offset location. For tests that corrupt the
               header content the corruption_start is bof and for tests that corrupt the data content the start position 
               is eof.
            5. Run the count_ sql corrosponding to the test and verify that the out file contains CONST.ERR_MSG
            6. Replace the corrupt data file with the original file (see step 3)
            7. ReRun the count_ sql corrosponding to the test and verify that it passes its comparison with ans file


            @data_provider data_provider_for_checksum_corruption
            '''
            test_name = self.test_data[0]
            tab_name = self.test_data[1][0]
            corruption_start = self.test_data[1][1]
            corruption_offset = 0
            corruption_char= ''
            if type(self.test_data[1][2]) == type(0) :
                '''
                Find the location that will be corrupted  for all tests except appendonly_verify_block_checksums_co
                '''
                corruption_offset = self.test_data[1][2]
            else:
                '''
                Find the character that will be flipped with the reverse case  for test appendonly_verify_block_checksums_co only
                '''
                corruption_char=self.test_data[1][2]

            tinctest.logger.info('=======================================')
            tinctest.logger.info('Starting Test %s' % test_name)
            tinctest.logger.info('Table Name %s' % tab_name)
            tinctest.logger.info('corruption start position %s' % corruption_start)
            tinctest.logger.info('corruption offset position %s' % corruption_offset)
            tinctest.logger.info('corruption character %s' % corruption_char)
            tinctest.logger.info('=======================================')

            self.create_table(tab_name)
            (host, db_path) = self.gpfile.get_host_and_db_path(self.dbname)
            tinctest.logger.info('Hostname=%s     data_directory=%s' %(host,db_path))
            file_list = self.gpfile.get_relfile_list(self.dbname, tab_name, db_path, host)
            data_file=db_path+'/'+file_list[0]
            ''' Take a backup of the data file before corruption '''
            cmd = "cp -f "+ data_file +" " + data_file +".bak"  
            tinctest.logger.info("Backup data-file : %s" % cmd)
            Command("Backup data-file", cmd).run(validateAfter=True)
            try:
             with open(data_file , "r+") as f:
                char_location=0
                write_char=CONST.CORRUPTION
                verify_sql='count_'+tab_name

                ''' For appendonly_verify_block_checksums test only '''
                if corruption_char == CONST.FIND_CHAR:
                    while (True):
                        char_location +=(-1)
                        f.seek(char_location,corruption_start)
                        if (f.read(1) == corruption_char):
                            corruption_offset=char_location
                            write_char=CONST.FIND_CHAR.upper()
                            verify_sql='count_appendonly_verify_block_checksums_co_on'
                            break

                f.seek(corruption_offset,corruption_start)
                f.write(write_char)
            except IOError as e:
                errmsg="I/O error({0}): {1}".format(e.errno, e.strerror)
                tinctest.logger.info("%s" % errmsg)
            except:
                errmsg = "Unexpected error:", sys.exc_info()[0]
                tinctest.logger.info("%s" % errmsg)
                raise
            f.close()
            self.select_table(verify_sql,noncorrupted_checksum=False)
            if corruption_char == 'a':
                self.select_table('count_appendonly_verify_block_checksums_co_off',noncorrupted_checksum=True)

            '''Replace the corrupted data file with the good one from backup taken earlier'''
            cmd = "cp -f "+ data_file+".bak " + data_file
            tinctest.logger.info("Restore data-file from backup : %s" % cmd)
            Command("Restore data-file", cmd).run(validateAfter=True)
            self.select_table(verify_sql,noncorrupted_checksum=True)

    def test_aochecksum_size(self):
            '''
            The following test verifies that the header length is calculated properly when the checksum is on & off
            for  small, large and bulk-dense header content.

            PARAMETERS (via data_provider):
            0. Test Name
            1. Sql file to create scenario. 
            2. Expected length of the header
            3. Expected type of the header

            STEPS :
            1. Create table of required type of header (bulk, large header_content or small header_content (ao/co)
            2. Find the data file for this relationship
            3. Get the gp_filedump of the data file
            4. Verify  expected length of the header
            5. Verify  expected type of the header

            @data_provider data_types_provider_aochecksum_size
            '''

            tab_name = self.test_data[0] 
            chksum_flag = self.test_data[1][0]
            header_size = self.test_data[1][1] 
            header_type = self.test_data[1][2] 
            tinctest.logger.info('=======================================')
            tinctest.logger.info('Starting Test %s' % tab_name)
            tinctest.logger.info('Table Name %s' % tab_name)
            tinctest.logger.info('chksum_flag %s' % chksum_flag )
            tinctest.logger.info('header_size %s' % header_size)
            tinctest.logger.info('header_type %s' % header_type)
            tinctest.logger.info('=======================================')
            self.create_table(tab_name)        
            (host, db_path) = self.gpfile.get_host_and_db_path(self.dbname)
            tinctest.logger.info('Hostname=%s     data_directory=%s' %(host,db_path)) 
            tinctest.logger.info( "tab_name : %s       chksum_flag: %s     header_size: %s   header_type: %s" % (tab_name,chksum_flag,header_size,header_type))
            file_list = self.gpfile.get_relfile_list(self.dbname, tab_name, db_path, host)
            for i in range(0, len(file_list)): 
                tinctest.logger.info('Getting checksum info for table %s with  relfilenode %s' % (tab_name, file_list[i]))
                flag=''
                if chksum_flag == 'on':
                   flag=' -M'
                if tab_name.endswith('ao'):
                   flag=flag+' -O row '
                has_headerlen = self.gpfile.check_in_filedump(db_path, host, file_list[i], header_size,flag)
                has_headertype = self.gpfile.check_in_filedump(db_path, host, file_list[i], header_type,flag)
                if not has_headerlen:
                    raise Exception ("Checksum validation failed for table %s with relfilenode: %s because header length is not %s" % (tab_name,file_list[i],header_size))
                if not has_headertype:
                    raise Exception ("Checksum validation failed for table %s with relfilenode: %s because header type is not %s" % (tab_name,file_list[i],header_type))

@tinctest.dataProvider('data_provider_for_checksum_corruption')
def test_data_provider_for_checksum_corruption():
    data = {
                        "chksum_on_corrupt_header_largecontent_co":['chksum_on_corrupt_header_large_co',CONST.BOF,CONST.HEADER_OFFSET],
                        "chksum_on_corrupt_content_largecontent_co":['chksum_on_corrupt_header_large_co',CONST.EOF,CONST.DATA_OFFSET],
                        "chksum_on_corrupt_header_smallcontent_co":['chksum_on_header_sml_co',CONST.BOF,CONST.HEADER_OFFSET],
                        "chksum_on_corrupt_content_smallcontent_co":['chksum_on_header_sml_co',CONST.EOF,CONST.DATA_OFFSET],
                        "chksum_on_corrupt_header_smallcontent_ao":['chksum_on_header_sml_ao',CONST.BOF,CONST.HEADER_OFFSET],
                        "chksum_on_corrupt_content_smallcontent_ao":['chksum_on_header_sml_ao',CONST.EOF,CONST.DATA_OFFSET],
                        "appendonly_verify_block_checksums_co":['appendonly_verify_block_checksums_co',CONST.EOF,CONST.FIND_CHAR]
            }
    return data

@tinctest.dataProvider('data_types_provider_aochecksum_size')
def test_data_provider_aochecksum_size():
    data = {
                        "chksum_on_header_bulkdense_co":[CONST.CHKSUM_ON,CONST.BULKDENSE_HEADER_LEN_WITH_CHKSUM_ON,CONST.BULKDENSE_HEADER_TYPE],
                        "chksum_off_header_bulkdense_co":[CONST.CHKSUM_OFF,CONST.BULKDENSE_HEADER_LEN_WITH_CHKSUM_OFF,CONST.BULKDENSE_HEADER_TYPE],
                        "chksum_off_header_sml_ao":[CONST.CHKSUM_OFF,CONST.SMALL_HEADER_LEN_WITH_CHKSUM_OFF,CONST.SMALL_HEADER_TYPE],
                        "chksum_off_header_sml_co":[CONST.CHKSUM_OFF,CONST.SMALL_HEADER_LEN_WITH_CHKSUM_OFF,CONST.SMALL_HEADER_TYPE],
                        "chksum_on_header_sml_ao":[CONST.CHKSUM_ON,CONST.SMALL_HEADER_LEN_WITH_CHKSUM_ON,CONST.SMALL_HEADER_TYPE],
                        "chksum_on_header_sml_co":[CONST.CHKSUM_ON,CONST.SMALL_HEADER_LEN_WITH_CHKSUM_ON,CONST.SMALL_HEADER_TYPE],
                        "chksum_on_header_large_co":[CONST.CHKSUM_ON,CONST.LARGE_HEADER_LEN_WITH_CHKSUM_ON,CONST.LARGE_HEADER_TYPE],
                        "chksum_off_header_large_co":[CONST.CHKSUM_OFF,CONST.LARGE_HEADER_LEN_WITH_CHKSUM_OFF,CONST.LARGE_HEADER_TYPE]
            } 
    return data

