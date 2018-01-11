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

import unittest2 as unittest
import sys
import os
import re
import shutil

import tinctest
from gppylib.commands.base import Command, REMOTE
from gppylib.db import dbconn
from gppylib.db.dbconn import UnexpectedRowsError
from tinctest.lib import local_path, Gpdiff
from tinctest.runner import TINCTextTestResult
from tinctest import logger

from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL

class ExpansionTestCase(MPPTestCase):
    """
        Base class for an expansion test case
    """
    def __init__(self, methodName):
        self.test_database = 'testdb'
        self.distribution_sql = self._get_absolute_filename('distribution_policy.sql')
        self.distribution_out = self._get_absolute_filename('distribution_policy.out')
        self.distribution_ans = self._get_absolute_filename('distribution_policy.ans')
        super(ExpansionTestCase, self).__init__(methodName) 

    def distribution_policy_snapshot(self, diff=False):
        """
            Take a snapshot of the distribution policies of the tables.
            @return: True if we succesfully collected the distribution policy for the tables.
                     False even if we are unable to collect the distribution policy for a single table.
        """
        logger.info("Collecting distribution policies of all tables on all DBs ...")
        with dbconn.connect(dbconn.DbURL()) as conn:
            cmd = "select datname from pg_database where datname not in ('postgres', 'template1', 'template0')"
            rows = dbconn.execSQL(conn, cmd)

            for row in rows:
                dbname_sql = 'distribution_%s.sql' % row[0].strip() 
                if diff:
                    dbname_ans = 'distribution_%s.out' % row[0].strip()
                else:
                    dbname_ans = 'distribution_%s.ans' % row[0].strip() 
                shutil.copyfile(self.distribution_sql, dbname_sql)

                data = ''
                with open(dbname_sql, 'r') as fp:
                    for line in fp:
                        data += line
                
                newdata = data.replace('$DBNAME', row[0].strip())
                with open(dbname_sql, 'w') as fp:
                    fp.write(newdata)

                if not PSQL.run_sql_file(dbname=self.test_database, sql_file=dbname_sql, out_file=dbname_ans):
                    raise Exception("failed to get the distribution policy: '%s'" % dbname_sql)

                if diff:
                    dbname_out = "%s.out" % dbname_ans[:-4]
                    equal = Gpdiff.are_files_equal(dbname_ans, dbname_out)
                    if not equal:
                        return False

        return True
                    
    def run_data_validation(self):
        """
            Validate data by executing a SQL file and comparing results 
            with the answer file.
        """    
        filename = self._get_absolute_filename(self.select_file)
        logger.info("Validating data using '%s' ..." % filename)
        if not PSQL.run_sql_file(sql_file=filename, dbname=self.dbname):
            raise Exception("failed querying data pre-expansion: '%s'" % filename)

        if not filename.endswith('sql'):
            raise Exception("The filename must end in .sql extension")

        answer_file = re.sub('sql$', 'ans', filename)
        output_file = re.sub('sql$', 'out', filename)

        if not Gpdiff.are_files_equal(output_file, answer_file):
            logger.error("files don't match pre-expansion: '%s' and '%s'" % (answer_file, output_file))
            return False

        return True

    def setup_cluster(self, initsystem_file, data_load_file, select_file, dbname=None):
        """
            Runs gpinitsystem to initialize the cluster and creates the test database.
        """
        if dbname is None:
            dbname = self.test_database

        initsystem_file = self._get_absolute_filename(initsystem_file)
        self.run_initsystem(initsystem_file)

        logger.info("Creating DB %s ..." % dbname)
        cmd = Command('create a test database', 'createdb %s' % self.test_database)
        cmd.run(validateAfter=True)

        data_load_file = self._get_absolute_filename(data_load_file)
        logger.info("Loading data '%s' ..." % data_load_file)
        if not PSQL.run_sql_file(sql_file=data_load_file, dbname=dbname, output_to_file=False):
            raise Exception("data load of %s failed" % data_load_file)

        self.select_file = select_file
        self.dbname = dbname

        assert self.run_data_validation()

    def run_expansion(self, mapfile):
        """
            Run an expansion test based on the mapping file
        """
        self.expansion_map_file = mapfile = self._get_absolute_filename(mapfile)
        self.distribution_policy_snapshot(diff=False)
        logger.info("Running expansion setup ...")
        cmd = Command(name='run gpexpand', cmdStr='gpexpand -i %s -D %s' % (mapfile, self.test_database), ctxt=REMOTE, remoteHost='localhost')
        cmd.run(validateAfter=True)
        return True

    def run_data_redistribution(self, duration='60:00:00'):
        """
            Run expansion to perform data redistribution
        """
        logger.info("Running expansion redistribution ...")
        cmd = Command(name='run data distribution', cmdStr='gpexpand -d %s -D %s' % (duration, self.test_database), ctxt=REMOTE, remoteHost='localhost')
        cmd.run(validateAfter=True)

        self.run_data_validation()

        return True

    def cleanup_expansion(self):
        """
            Run gpexpand to cleanup the expansion catalog
            @return: True if the cleanup of gpexpand schema suceeded 
                     False otherwise
        """
        logger.info("Running expansion cleanup ...")
        query = "SELECT count(*) FROM information_schema.schemata where schema_name='gpexpand';"

        cmd = Command(name='run gpexpand cleanup',
                      cmdStr='echo -e \"y\n\"  | gpexpand -c -D %s' % self.test_database,
                      ctxt=REMOTE,
                      remoteHost='localhost')
        cmd.run(validateAfter=True)

        with dbconn.connect(dbconn.DbURL(dbname=self.test_database)) as conn:
            try:
                row = dbconn.execSQLForSingleton(conn, query)
            except UnexpectedRowsError, e:
                return False
            
            if row != 0:
                return False

        return True 

    def expansion_validation(self):
        """Validate that the pre/post snapshots are consistent"""
        assert self.distribution_policy_snapshot(diff=True)
        assert self.hosts_gpsegconf_validation()
        assert self.catalog_validation()
        assert self.gpdb_functionality_validation()
        assert self.check_skew()
 
    def catalog_validation(self):
        """
            Validate that there are no inconsistencies in the catalog
            @return: True if there are no inconsistencies 
                     False otherwise
        """
        logger.info("Running gpcheckcat to validate catalog ...")

        # Fetch the count of databases using gpcheckcat that pass the catalog check test
        out_file = self._get_absolute_filename('gpcheckcat.out')
        assert self.db_port is not None
        cmd_str = '$GPHOME/bin/lib/gpcheckcat -A -O -p %s &> %s' % (self.db_port, out_file)   
        cmd = Command('run gpcheckcat', cmd_str)
        cmd.run(validateAfter=True)

        line_no = 0
        with open(out_file) as fp:
            for line in fp:
                if 'Found no catalog issue' in line:
                    line_no += 1

        count_db = 0
        # fetch the database count on the host using pg_catalog 
        with dbconn.connect(dbconn.DbURL()) as conn:
            row = dbconn.execSQLForSingleton(conn, "select count(*) from pg_database")

        # -1 because gpcheckcat does not run against template0
        count_db = row - 1

        # Check if the numbers match else expansion dint go through fine return false
        if line_no != count_db:
            failed_dbs = self._get_checkcat_failed_dbs(out_file)
            logger.error('gpcheckcat failed for the following databases %s' % failed_dbs)
            return False

        return True

    def _get_checkcat_failed_dbs(self, filename):
        """Gives the list of databases for which gpcheckcat failed"""
        failed = True
        database = None
        failed_dbs = []

        connection_info_pat = re.compile('Connected as user \'(.*)\' to database \'(.*)\', port \'(.*)\', gpdb version \'(.*)\'')
        with open(filename) as out_file:
            for line in out_file:
                mat = connection_info_pat.match(line)
                if mat is None:
                    if line.strip() == 'Found no catalog issue':
                        failed = False
                    continue
                if failed and database is not None:
                    failed_dbs.append(database)
                database = mat.group(2)
                failed = True
            if failed:
                failed_dbs.append(database)

        return failed_dbs
        
    def hosts_gpsegconf_validation(self):
        """ 
            Validate if the new hosts are added to gp_segment_configuration table 
            Parse the expansion map and populate the datafields into a list
        """
        logger.info("Verifying expanded segments in gp_segment_configuration table ...")

        with open(self.expansion_map_file) as fp:
            replication_port_list = []
            for line in fp:
                fields = line.split(':')
                if len(fields) == 8:
                    replication_port_list.append(fields[7])
                    cmd = """select count(*)
                             from gp_segment_configuration
                             where hostname = '%s'
                             and address = '%s'
                             and port = %s
                             and dbid = %s
                             and content = %s
                             and role = '%s'
                             and replication_port = %s""" % (fields[0], fields[1], fields[2],  fields[4], fields[5], fields[6], fields[7])
                else:
                    cmd = """select count(*)
                             from gp_segment_configuration
                             where hostname = '%s'
                             and address = '%s'
                             and port = %s
                             and dbid = %s
                             and content = %s
                             and role = '%s'""" % (fields[0], fields[1], fields[2], fields[4], fields[5], fields[6])
        
                with dbconn.connect(dbconn.DbURL()) as conn:
                    row = dbconn.execSQLForSingleton(conn, cmd)

                if row != 1:
                    return False
            
        return True

    def check_random_dist_tuple_count_skew(self, tname, values):
        """ 
            max - min should not exceed 5% of the Maximum number of tuples.
            @return: False if there is any error
                     True otherwise
        """
        if not values:
            return True

        max_tuple_count, min_tuple_count = max(values), min(values)

        diff = max_tuple_count - min_tuple_count
        pct = float(diff) / float(max_tuple_count) * 100.0

        if pct > 5:
            logger.error("MAX (%d) MIN (%d) DIFF (%d) PCT(%f)" % (max_tuple_count, min_tuple_count, diff, pct))
            return False

        logger.info("OK: Table (%s) Max (%d) Min (%d) tuples per segdb" % (tname, max_tuple_count, min_tuple_count))

        return True

    def check_skew(self):
        """Check that all tables have been distributed reasonably"""
        tables = []

        logger.info("Checking skew ...")

        with dbconn.connect(dbconn.DbURL(dbname=self.test_database)) as conn:
            query = "select fq_name  from gpexpand.status_detail where dbname = '%s'" % self.test_database
            rows = dbconn.execSQL(conn, query)
            for row in rows:
                tables.append(row[0].partition(".")[2])

            for table in tables:
                query = "select data.segid, data.segtupcount from gp_toolkit.gp_skew_details( (select oid from pg_class where relname = 't1')) as data" 
                rows = dbconn.execSQL(conn, query)
                tuplecounts = []
                for row in rows:
                    segtupcount = row[1]
                    tuplecounts.append(segtupcount)

                if not self.check_random_dist_tuple_count_skew(table, tuplecounts):
                    raise Exception("Table %s has not been redistributed well.  Check skew." % table)

        return True

    def gpdb_functionality_validation(self):
        """
            Verify that we can create a table, insert data, 
            select data and drop data.
        """ 
        filename = self._get_absolute_filename('test_basic_gpdb_functionality.sql')
        logger.info('Validating that Greenplum Database is still functional ...')
        assert PSQL.run_sql_file(sql_file=filename, dbname=self.test_database)

        answer_file = re.sub('sql$', 'ans', filename)
        output_file = re.sub('sql$', 'out', filename)

        if not Gpdiff.are_files_equal(output_file, answer_file):
            logger.error('Could not validate gpdb functionality')
            return False

        return True

