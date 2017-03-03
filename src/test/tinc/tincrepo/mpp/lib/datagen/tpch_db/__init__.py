from gppylib.commands.base import Command, CommandResult, REMOTE

from mpp.lib.datagen import TINCTestDatabase

from mpp.lib.PSQL import PSQL

import os, commands

class TPCHDBDatabase(TINCTestDatabase):
    def __init__(self, database_name = "tpch_1g_heap", scale_factor = 1, storage_type = 'heap', partition = False):
        self.db_name = database_name
        self.scale_factor = scale_factor
        self.storage_type = storage_type
        self.partition = partition
        self.primary_segments_count = 1
        self._pwd = os.path.abspath(os.path.dirname(__file__))
        super(TPCHDBDatabase, self).__init__(database_name)
        
        
    def setUp(self):
        if super(TPCHDBDatabase, self).setUp():
            return True
        #Find number of primary segments
        self.primary_segments_count = (PSQL.run_sql_command("select 'primary_segment' from gp_segment_configuration \
                                      where content >= 0 and role = 'p'")).count('primary_segment')
                                      
        self._loading_pl = os.path.join(self._pwd, 'generate_load_tpch.pl')
        if not os.path.exists(self._loading_pl):
            print('generate_load_tpch.pl does not exist. Load fails. ')
            return False

        command = ''
        default_port = os.environ['PGPORT']
        
        if self.storage_type == 'heap':
            command = '%s -scale %s -num %s -port %s -db %s -table heap'%(self._loading_pl, self.scale_factor ,self.primary_segments_count - 1, default_port, self.db_name)
        elif self.storage_type == 'ao':
            command = '%s -scale %s -num %s -port %s -db %s -table ao -orient row'%(self._loading_pl, self.scale_factor ,self.primary_segments_count - 1, default_port, self.db_name)
        elif self.storage_type == 'aoco':
            command = '%s -scale %s -num %s -port %s -db %s -table ao -orient column'%(self._loading_pl, self.scale_factor ,self.primary_segments_count - 1, default_port, self.db_name)
        else:
            command = '%s -scale %s -num %s -port %s -db %s -table heap'%(self._loading_pl, self.scale_factor ,self.primary_segments_count - 1, default_port, self.db_name)

        if self.partition:
            command += ' -partition True'
        
        # print ('Execute command: %s'%command)
        status, output = commands.getstatusoutput(command)    
        if status != 0:
            print ('Error in generate and load TPCH data. ')
            print (output)
            return False
        
        return False
        
        
        
        
        
        
        
        
