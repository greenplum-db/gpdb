import os
import sys

import tinctest

from mpp.lib.datagen import TINCTestDatabase
from mpp.lib.PSQL import PSQL

from tinctest.lib import local_path

from gppylib.commands.base import Command, CommandResult

class WorldDatabase(TINCTestDatabase):

    def __init__(self, database_name = "world_db"):
        self.db_name = database_name
        super(WorldDatabase, self).__init__(database_name)

    def setUp(self):
        # Assume setup is done if db exists
        output = PSQL.run_sql_command("select 'command_found_' || datname from pg_database where datname like '" + self.db_name + "'")
        if 'command_found_' + self.db_name in output:
            return
        cmd = Command('dropdb', "dropdb " + self.db_name)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        cmd = Command('createdb', "createdb " + self.db_name)
        cmd.run(validateAfter=True)
        result = cmd.get_results()
        PSQL.run_sql_file(local_path('world.sql'), dbname = self.db_name)
