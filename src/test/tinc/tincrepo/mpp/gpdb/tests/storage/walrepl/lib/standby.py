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

from gppylib.gparray import GpArray
from gppylib.commands import gp
from gppylib.db import dbconn

import mpp.gpdb.tests.storage.walrepl.lib

import os
import shutil
import subprocess
import signal

class Standby(object):
    """
    This class represents Standby and is responsible for operations
    to create, start, etc.
    """

    def __init__(self, datadir, port, primary_conninfo=''):
        self.datadir = os.path.abspath(datadir)
        self.port = port
        self.primary_conninfo = primary_conninfo
        self.recovery_conf_path = os.path.join(self.datadir, 'recovery.conf')
        self.promote_path = os.path.join(datadir, 'promote')
        self.postmaster_pid_path = os.path.join(datadir, 'postmaster.pid')

    def _run_utility_sql(self, sql, **kwargs):
        with mpp.gpdb.tests.storage.walrepl.lib.DbConn(utility=True, **kwargs) as conn:
            return conn.execute(sql)

    def remove_catalog_standby(self, dburl):
        sql = "SELECT gp_remove_master_standby()"
        self._run_utility_sql(sql)

    def add_catalog_standby(self, dburl, gparray):
        master = gparray.master

        # This will eventually support extra filespaces,
        # but for now only signle filespace, which is pg_system
        # is supported.  pg_basebackup also needs to
        # recognize this setting.
        #
        #fsmap = master.getSegmentFilespaces()
        #fslist = ["['{0}', '{1}']".format(dbarray.getFileSpaceName(fsoid), path)
        #            for fsoid, path in fsmap.items()]
        fslist = ["['pg_system', '{0}']".format(self.datadir)]

        sql = ("SELECT gp_add_master_standby('{hostname}', '{address}', "
               "ARRAY[{filespaces}], {port})").format(
                hostname=master.getSegmentHostName(),
                address=master.getSegmentAddress(),
                filespaces=', '.join(fslist),
                port=self.port)
        self._run_utility_sql(sql)

    def update_primary_pg_hba(self):
        primary_datadir = os.environ.get('MASTER_DATA_DIRECTORY')
        pg_hba = os.path.join(primary_datadir, 'pg_hba.conf')
        with open(pg_hba) as f:
            lines = f.readlines()

        sql = 'select rolname from pg_authid where oid = 10'
        username = self._run_utility_sql(sql)[0].rolname
        values = ['host', 'replication', username, '0.0.0.0/0', 'trust']
        newline = '\t'.join(values) + '\n'
        if newline not in lines:
            lines.append(newline)

        tmpfile = pg_hba + '.tmp'
        with open(tmpfile, 'w') as f:
            f.writelines(lines)

        os.rename(tmpfile, pg_hba)

    def create(self):
        dburl = dbconn.DbURL()
        gparray = GpArray.initFromCatalog(dburl, utility=True)
        if gparray.standbyMaster:
            self.remove_catalog_standby(dburl)

        self.add_catalog_standby(dburl, gparray)
        shutil.rmtree(self.datadir, True)
        rc = subprocess.call(
                ['pg_basebackup', '-x', '-R', '-D', self.datadir])

        # this is ugly, but required by gpstart, who determines
        # port number from postgresql.conf and removes UNIX socket file.
        postgresql_conf = os.path.join(self.datadir, 'postgresql.conf')
        conf_tmp = open(postgresql_conf + '.tmp', 'w')
        with open(postgresql_conf) as conf:
            for line in conf.readlines():
                if "port=" in line:
                    line = "port={0} #{1}".format(self.port, line)
                conf_tmp.write(line)
        conf_tmp.close()
        os.rename(postgresql_conf + '.tmp', postgresql_conf)

        self.update_primary_pg_hba()

        return rc


    def create_recovery_conf(self):
        """
        Put recovery.conf with necessary information.  primary_conninfo
        will come from constructor parameter.
        """

        with open(self.recovery_conf_path, 'w') as f:
            f.write("standby_mode = 'on'\n")
            f.write("primary_conninfo = '{0}'\n".format(
                self.primary_conninfo))

    def start(self):
        """
        Start the standby postmaster.  The options to pg_ctl needs to be
        determined by gppylib logic.
        """

        dburl = dbconn.DbURL()
        gparray = GpArray.initFromCatalog(dburl, utility=True)
        numcontent = gparray.getNumSegmentContents()
        standby = gparray.standbyMaster
        master = gp.MasterStart("Starting Master Standby",
                                self.datadir, self.port, standby.dbid,
                                0, numcontent, None, None, None)
        # -w option would wait forever.
        master.cmdStr = master.cmdStr.replace(' -w', '')
        master.run(validateAfter=True)

        return master.get_results()

    def stop(self):
        """
        Stop the standby if it runs.
        """

        # immediate is necessary if it's in recovery (for now).
        # we don't care the result.
        master = gp.MasterStop("Stopping Master Standby",
                               self.datadir, mode='immediate')
        master.run()

    def _wait_for_ready(self):
        for i in mpp.gpdb.tests.storage.walrepl.lib.polling(20, 0.5):
            proc = subprocess.Popen(['psql', '-p', str(self.port),
                                    '-c', 'select 1'],
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
            stdout = proc.communicate()[0]
            #logger.debug('psql:' + stdout)
            rc = proc.returncode
            if rc == 0:
                break

        return rc == 0

    def promote(self):
        """
        Promote standby using pg_ctl.
        """

        proc = subprocess.Popen(['pg_ctl', '-D', self.datadir, 'promote'],
                                stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)
        stdout = proc.communicate()[0]
        if not self._wait_for_ready():
            return False

        return True

    def promote_manual(self):
        """
        Stop streaming and let the standby finish recovery.
        The protocol is to create promote file in the data directory,
        and send SIGUSR1 to postmaster.
        """

        with open(self.promote_path, 'w') as f:
            f.write('')
        with open(self.postmaster_pid_path) as f:
            pid = int(f.readline())
        os.kill(pid, signal.SIGUSR1)
        if not self._wait_for_ready():
            return False

        return True
