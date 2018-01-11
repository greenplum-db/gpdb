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

import os
import sys

import tinctest

from mpp.lib.datagen import TINCTestDatabase
from mpp.lib.datagen.dataset import DataSetDatabase

from mpp.lib.PSQL import PSQL

class DispatchSkewDatabase(DataSetDatabase):
    """
    Database with skewed tables based on dataset.
    """

    def __init__(self, database_name="queryfinish"):
        super(DispatchSkewDatabase, self).__init__(
                database_name=database_name)

    def setUp(self):
        # Super class will return True if the database already exists.
        if super(DispatchSkewDatabase, self).setUp():
            return True

        # Database is created for the first time and we need to run some
        # some more setup.
        source_dir = os.path.dirname(
                sys.modules[self.__class__.__module__].__file__)

        sql_file = os.path.join(source_dir, "setup.sql")
        out_file = sql_file.replace('.sql', '.out')
        PSQL.run_sql_file(sql_file, dbname=self.db_name, out_file=out_file)

        return False

