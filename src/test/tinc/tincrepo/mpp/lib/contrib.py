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

from mpp.lib.PSQL import PSQL

class ContribException(Exception):
    pass

class Contrib(object):
    """
    In Postgres, we have contributors of modules including tools, analysis, utilities, plugin, etc.
    They are not part of the core PostgreSQL system.

    This class helps to install and uninstall the contribution package from a specific location,
    verify whether the install and uninstall exist.
    
    For example:
    Pgcrypto contribution, after installation, there are two files installed in
    $GPHOME/share/postgresql/contrib/pgcrypto.sql and uninstall_pgcrypto.sql.

    contrib = Contrib("pgcrypto.sql", "uninstall_pgcrypto.sql", self.db_name)
    contrib.install()
    """
    pg_port = os.getenv("PGPORT", 5432)

    def __init__(self, install, uninstall, dbname, loc="share/postgresql/contrib"):
        """
        Initialize contrib class
        @type install: string
        @param install: install filename

        @type uninstall: string
        @param uninstall: uninstall filename

        @type dbname: string
        @param dbname: database name

        @type loc: string
        @param loc: location of contrib filename
        """
        GPHOME = os.environ.get("GPHOME")
        self.db_name = dbname
        self.install_file = "%s/%s/%s" % (GPHOME, loc, install)
        self.uninstall_file = "%s/%s/%s" % (GPHOME, loc, uninstall)
     
        if os.path.exists(self.install_file) is False:
            raise ContribException("%s does not exist, please check contrib package is properly installed." % self.install_file)

        if os.path.exists(self.uninstall_file) is False:
            raise ContribException("%s does not exist, please check contrib package is properly installed." % self.uninstall_file)

    def install(self):
        """
        Install the contrib file
        """
        rc = PSQL.run_sql_file(self.install_file, dbname = self.db_name)
        if rc is False:
            raise ContribException("Installation failed %s on database: %s" % (self.install_file, self.db_name))

    def uninstall(self):
        """
        Uninstall the contrib file
        """
        rc = PSQL.run_sql_file(self.uninstall_file, dbname = self.db_name)
        if rc is False:
            raise ContribException("Uninstallation failed %s on database: %s" % (self.uninstall_file, self.db_name))
