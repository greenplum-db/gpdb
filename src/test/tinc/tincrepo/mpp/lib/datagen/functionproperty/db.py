import os
import sys

import tinctest

from mpp.lib.datagen import TINCTestDatabase
from mpp.lib.PSQL import PSQL

from tinctest.lib import run_shell_command

class FunctionPropertyTestDatabase(TINCTestDatabase):
    """
    Database for cardinality estimation tests that requires a specified gpsd to be loaded
    """

    def __init__(self, database_name):
        super(FunctionPropertyTestDatabase, self).__init__(database_name)

    def setUp(self):
        # Call out to TINCTestDatabase.setUp which will return True if the database already exists
        if super(FunctionPropertyTestDatabase, self).setUp():
            return True
        # Means the database is created for the first time and we need to run the corresponding gpsd file
        # and load gpoptsutil udfs
        source_dir = os.path.dirname(sys.modules[self.__class__.__module__].__file__)
        out_dir = os.path.join(source_dir, 'output')
        if not os.path.exists(out_dir):
            os.makedirs(out_dir)
        outfile = os.path.join(out_dir, 'setup.out')

        # Load gpsd
        setup_file=os.path.join(source_dir, "setup.sql") 
        PSQL.run_sql_file(setup_file, dbname = self.db_name, out_file = outfile)
        return False
