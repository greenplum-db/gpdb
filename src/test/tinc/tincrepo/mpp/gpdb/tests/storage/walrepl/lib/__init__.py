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

# Due to name collision with walrepl.lib.dbconn, use full-qualified name for now.
# The plan is to deprecate walrepl.lib.dbconn
import gppylib.db.dbconn
from gppylib.commands.base import *
from gppylib.gparray import GpArray

import os
import re
import shutil
import socket
import time

from mpp.gpdb.tests.storage.walrepl.lib.dbconn import DbConn
from mpp.gpdb.tests.storage.walrepl.lib.standby import Standby
from mpp.gpdb.tests.storage.walrepl.lib.pg_controldata import PgControlData
from tinctest.main import TINCException

class WalReplException(TINCException): pass

def polling(max_try, interval):
    """
    This function is convenient when creating wait-poll loop.
    e.g.
        for i in walrepl.lib.polling(10, 0.5):
            res = try_something()
            if res:
                break

        # check if we succeeded or exceed retries
        if res:
            succeed()
        else:
            failed()
    """

    retry = 0
    while True:
        yield retry
        if retry > max_try:
            break
        time.sleep(interval)
        retry += 1

class SmartCmd(Command, object):
    """
    SmartCmd is an extension to Command.  It takes fewer arguments to do the same
    job.  Especially, it decides context automatically so that you don't need to
    bother it anymore.
    """

    def __init__(self, cmd, host='localhost', stdin=None):
        context = LOCAL
        # We need better way to check it.  The bottom line is to run it in REMOTE.
        if host != 'localhost' and host != socket.gethostname():
            context = REMOTE

        super(SmartCmd, self).__init__(cmd, cmd, ctxt=context,
                                       remoteHost=host, stdin=stdin)

def get_postmaster_pid(datadir, host='localhost'):
    """
    Get postmaster pid from the postmaster.pid.
    Returns -1 if not available.
    """

    cmd_str = 'head -1 {datadir}/postmaster.pid'.format(datadir=datadir)
    cmd = SmartCmd(cmd_str, host=host)
    cmd.run()
    results = cmd.get_results()
    if results.rc != 0:
        return -1

    return int(results.stdout.strip())


class NewEnv(object):
    def __init__(self, **kwargs):
        self.args = kwargs

    def __enter__(self):
        orig = dict()
        for key, val in self.args.items():
            if key in os.environ:
                orig[key] = os.environ[key]
            os.environ[key] = val
        self.orig = orig

    def __exit__(self, type, value, traceback):
        for key in self.args.keys():
            if key in self.orig:
                os.environ[key] = self.orig[key]
            else:
                del os.environ[key]


def cleanupTablespaces(conn):
    """
    Deletes all non-system tablespaces with tables under them in this
    database.
    """

    tablespaces = conn.execute("SELECT oid, spcname FROM pg_tablespace "
                               "WHERE spcname <> 'pg_default' AND spcname <> 'pg_global'")
    for tblspc in tablespaces:
        tsoid = tblspc.oid
        tables = conn.execute(
            ("SELECT relname FROM pg_class "
             "WHERE reltablespace = {0} AND relkind = 'r'").format(tsoid))
        table_names = [table.relname for table in tables]
        if len(table_names) > 0:
            conn.execute("DROP TABLE {0}".format(','.join(table_names)))
        conn.execute("DROP TABLESPACE {0}".format(tblspc.spcname))

def cleanupFilespaces(dbname):
    """
    Deletes filespaces except system filespace along with tablespace
    associated with the filespaces and tables, then delete the local
    directories for them.  The dbname parameter is used to drop tables
    under non-system filespace, and this works only if the tables belong
    to this database.  Needs improvement.
    """

    dburl = gppylib.db.dbconn.DbURL()
    gparray = GpArray.initFromCatalog(dburl, utility=True)

    with DbConn(dbname=dbname) as conn:
        cleanupTablespaces(conn)


class PreprocessFileMixin(object):
    """
    Provide preprocess_file(in_file).  Need self.fsprefix.
    """

    fsprefix = '/tmp'

    def preprocess_file(self, in_path):
        """
        Proprocess .in files, to match enviroment.  Currently CREATE FILESPACE
        will be substituted.

        e.g.
        CREATE FILESPACE fs();
        will be completed with dbid/directory path.
        """

        dburl = gppylib.db.dbconn.DbURL()
        gparray = GpArray.initFromCatalog(dburl, utility=True)
        assert(in_path.endswith('.in'))
        out_path = in_path[:-3]
        pat = re.compile(r'CREATE\s+FILESPACE\s+(\w+)', re.I)
        with open(out_path, 'w') as out_file:
            for line in open(in_path):
                m = pat.match(line)
                if m:
                    # The file paths will look like
                    #   <fsprefix>/<fsname>/gpdb<dbid>
                    fsname = m.group(1)
                    basepath = os.path.join(self.fsprefix, fsname)
                    if not os.path.exists(basepath):
                        os.makedirs(basepath)
                    buf = list()
                    for db in gparray.getDbList():
                        fselocation = os.path.join(basepath,
                                                   'gpdb' + str(db.dbid))
                        buf.append("{0}: '{1}'".format(
                            db.dbid, fselocation))
                    line = "CREATE FILESPACE {0}({1});\n".format(
                            fsname, ', '.join(buf))

                out_file.write(line)
