#! /usr/bin/env python

'''
Tool to validate replication consistency between primary and mirror.

============================= DISCLAIMER =============================
This gp_replica_check tool is for 6.0+ development testing replication
consistency between pairs of primary and mirror segments. Currently
the tool is only supported on a single node cluster. The tool creates
a new extension called gp_replica_check and then invokes the extension
on all the primary segments in Utility mode. The extension will do md5
checksum validation on each block of the relation and report a
mismatch for the first inconsistent block. Each block read from disk
will utilize the internal masking functions to ensure that false
mismatches are not reported such as header or hint-bit mismatches. The
user is able to specify what relation types and databases they would
like to validate or it defaults to all.
======================================================================

Note:
For global directory checking, -d flag must be all or template1 at the
moment due to the filespace mapping query we execute.

Usage:
gp_replica_check.py
gp_replica_check.py -d "mydb1,mydb2,..."
gp_replica_check.py -r "heap,ao,btree,..."
gp_replica_check.py -d "mydb1,mydb2,..." -r "hash,bitmap,gist,..."
'''

import argparse
import sys
import subprocess
import threading
import Queue

class ReplicaCheck(threading.Thread):
    def __init__(self, segrow, datname, relation_types):
        super(ReplicaCheck, self).__init__()
        self.host = segrow[0]
        self.port = segrow[1]
        self.content = segrow[2]
        self.primary_status = segrow[3]
        self.ploc = segrow[4]
        self.mloc = segrow[5]
        self.datname = datname
        self.relation_types = relation_types;
        self.result = False

    def __str__(self):
        return 'Host: %s, Port: %s, Database: %s\n\
Primary Data Directory Location: %s\n\
Mirror Data Directory Location: %s' % (self.host, self.port, self.datname,
                                          self.ploc, self.mloc)

    def run(self):
        print(self)
        cmd = '''PGOPTIONS='-c gp_session_role=utility' psql -h %s -p %s -c "select * from gp_replica_check('%s', '%s', '%s')" %s''' % (self.host, self.port,
                                                                                                                                        self.ploc, self.mloc,
                                                                                                                                        self.relation_types,
                                                                                                                                        self.datname)

        if self.primary_status.strip() == 'd':
            print "Primary segment for content %d is down" % self.content
        else:
            try:
                res = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
                print res
                self.result = True if res.strip().split('\n')[-2].strip() == 't' else False
            except subprocess.CalledProcessError, e:
                print 'returncode: (%s), cmd: (%s), output: (%s)' % (e.returncode, e.cmd, e.output)

def install_extension(databases):
    get_datname_sql = ''' SELECT datname FROM pg_database WHERE datname != 'template0' '''
    create_ext_sql = ''' CREATE EXTENSION IF NOT EXISTS gp_replica_check '''

    database_list = map(str.strip, databases.split(','))
    print "Creating gp_replica_check extension on databases if needed:"
    datnames = subprocess.check_output('psql postgres -t -c "%s"' % get_datname_sql, stderr=subprocess.STDOUT, shell=True).split('\n')
    for datname in datnames:
        if len(datname) > 1 and (datname.strip() in database_list or 'all' in database_list):
            print subprocess.check_output('psql %s -t -c "%s"' % (datname.strip(), create_ext_sql), stderr=subprocess.STDOUT, shell=True)

# Get the primary and mirror servers, for each content ID.
def get_segments():
    seglist_sql = '''
SELECT gscp.address, gscp.port, gscp.content, gscp.status, gscp.datadir as p_datadir, gscm.datadir as m_datadir
FROM gp_segment_configuration gscp,
     gp_segment_configuration gscm
WHERE gscp.content = gscm.content
      AND gscp.role = 'p'
      AND gscm.role = 'm'
'''
    seglist = subprocess.check_output('psql postgres -t -c "%s"' % seglist_sql, stderr=subprocess.STDOUT, shell=True).split('\n')
    segmap = {}
    for segrow in seglist:
        segelements = map(str.strip, segrow.split('|'))
        if len(segelements) > 1:
            segmap.setdefault(segelements[2], []).append(segelements)

    return segmap

# Get list of database from pg_database.
#
# The argument is a list of "allowed" database. The returned list will be
# filtered to contain only database from that list. If it includes 'all',
# then all databases are returned. (template0 is left out in any case)
def get_databases(databases):
    dblist_sql = '''
SELECT datname FROM pg_catalog.pg_database WHERE datname != 'template0'
'''

    database_list = map(str.strip, databases.split(','))

    dbrawlist = subprocess.check_output('psql postgres -t -c "%s"' % dblist_sql, stderr=subprocess.STDOUT, shell=True).split('\n')
    dblist = []
    for dbrow in dbrawlist:
        dbname = dbrow.strip()
        if dbname != '':
            if dbname in database_list or 'all' in database_list:
                dblist.append(dbname)

    return dblist

def start_verification(segmap, dblist, relation_types):
    replica_check_list = []
    for content, seglist in segmap.items():
        for segrow in seglist:
            for dbname in dblist:
                replica_check = ReplicaCheck(segrow, dbname, relation_types)
                replica_check_list.append(replica_check)
                replica_check.start()
                replica_check.join()

    for replica_check in replica_check_list:
        if not replica_check.result:
            print "replica check failed"
            sys.exit(1)

    print "replica check succeeded"

def defargs():
    parser = argparse.ArgumentParser(description='Run replication check on all segments')
    parser.add_argument('--databases', '-d', type=str, required=False, default='all',
                        help='Database names to run replication check on')
    parser.add_argument('--relation-types', '-r', type=str, required=False, default='all',
                        help='Relation types to run replication check on')

    return parser.parse_args()

if __name__ == '__main__':
    args = defargs()

    install_extension(args.databases)
    start_verification(get_segments(), get_databases(args.databases), args.relation_types)
