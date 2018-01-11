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

from gppylib.db import dbconn

class DbConn(object):
    """
    Just a short-cut class for dbconn package.  By default, it is
    in autocommit mode, which commits transactions whenever a command
    finishes.
    """

    class Tuple(object):
        pass

    def __init__(self, utility=False, autocommit=True, **kwargs):
        self.dburl = dbconn.DbURL(**kwargs)
        self.conn = dbconn.connect(self.dburl, utility=utility)
        self.autocommit = autocommit

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        self.conn.commit()
        self.conn.close()

    def execute(self, sql):
        """
        Executes SQL and returns a list of dictionary with keys
        as column names.  Returns an empty list if no rows are
        returned from SQL.
        """

        cursor = dbconn.execSQL(self.conn, sql)
        # description wouldn't appear if DML or similar
        if cursor.description:
            names = [desc[0] for desc in cursor.description]
        rows = list()

        if cursor.rowcount > 0:
            for res in cursor.fetchall():
                row = DbConn.Tuple()
                for i, name in enumerate(names):
                    setattr(row, name, res[i])
                rows.append(row)

        if self.autocommit:
            self.commit()
        cursor.close()
        return rows

    def commit(self):
        self.conn.commit()


