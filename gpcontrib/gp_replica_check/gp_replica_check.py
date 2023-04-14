#!/usr/bin/env python3

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

from __future__ import print_function
import argparse
import sys
try:
    import subprocess32 as subprocess
except:
    import subprocess
import threading
import pipes  # for shell-quoting, pipes.quote()
import os
from collections import defaultdict
from pg import DB


def run_sql(sql, host=None, port=None,
            dbname="postgres", is_query=True,
            is_utility=False):
    if host is None:
        host = os.getenv("PGHOST")
    if port is None:
        port = int(os.getenv("PGPORT"))
    opt = "-c gp_role=utility" if is_utility else None
    with DB(dbname=dbname, host=host, port=port, opt=opt) as db:
        r = db.query(sql)
        if is_query:
            return r.getresult()


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
        self.lock = threading.Lock()

    def __str__(self):
        return '\n(%s) ---------\nHost: %s, Port: %s, Database: %s\n\
Primary Location: %s\n\
Mirror  Location: %s' % (self.getName(), self.host, self.port, self.datname,
                                          self.ploc, self.mloc)

    def run(self):
        if self.primary_status.strip() == 'd':
            print("Primary segment for content %d is down" % self.content)
        else:
            try:
                sql = "select * from gp_replica_check('%s', '%s', '%s')" % (self.ploc, self.mloc, self.relation_types)
                res = run_sql(sql, host=self.host, port=self.port, is_utility=True)
                self.result = True if res and res[0] and res[0][0] else False
                with self.lock:
                    print(self)
                    if self.result:
                        print("(%s) ** passed **" % (self.getName()))
                    else:
                        print("(%s) --> failed <--" % (self.getName()))
                        print (res)
            except:
                with self.lock:
                    print(self)
                    raise

def create_restartpoint_on_ckpt_record_replay(set):
    if set:
        cmd = "gpconfig -c create_restartpoint_on_ckpt_record_replay -v on --skipvalidation && gpstop -u"
    else:
        cmd = "gpconfig -r create_restartpoint_on_ckpt_record_replay --skipvalidation && gpstop -u"
    print(cmd)
    try:
        res = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True).decode()
        print(res)
    except subprocess.CalledProcessError as e:
        print('returncode: (%s), cmd: (%s), output: (%s)' % (e.returncode, e.cmd, e.output))
        if set:
            print('''guc setting with gpconfig & then updating with "gpstop -u" failed.
Probably there are some nodes could not be brought up and thus we
can not run the test. That is probably because previous tests cause
the instability of the cluster (indicate a bug usually) or because more time
is needed for the cluster to be ready due to heavy load (consider increasing
timeout configurations for this case). In any case we just fail and skip
the test. Please check the server logs to find why.''')
        sys.exit(2)

def install_extension(databases):
    get_datname_sql = ''' SELECT datname FROM pg_database WHERE datname != 'template0' '''
    create_ext_sql = ''' CREATE EXTENSION IF NOT EXISTS gp_replica_check WITH SCHEMA public'''

    database_list = list(map(str.strip, databases.split(',')))
    print("Creating gp_replica_check extension on databases if needed:")
    datnames = [dn for dn, in run_sql(get_datname_sql)]
    for datname in datnames:
        if len(datname) >= 1 and (datname.strip() in database_list or 'all' in database_list):
            run_sql(create_ext_sql, dbname=datname, is_query=False)
            print("CREATE EXTENSION gp_replica_check OK! DATABASE (%s), SCHEMA (%s)" % (datname, "public"))

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
    seglist = run_sql(seglist_sql)
    segmap = defaultdict(list)
    for segrow in seglist:
        segmap[segrow[2]].append(segrow)

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

    database_list = list(map(str.strip, databases.split(',')))
    dbrawlist = [dn for dn, in run_sql(dblist_sql)]
    dblist = []
    for dbrow in dbrawlist:
        dbname = dbrow
        if len(dbname) >= 1 and (dbname in database_list or 'all' in database_list):
            dblist.append(dbname)

    return dblist

def start_verification(segmap, dblist, relation_types):
    replica_check_list = []
    failed = False
    for content, seglist in list(segmap.items()):
        for segrow in seglist:
            for dbname in dblist:
                replica_check = ReplicaCheck(segrow, dbname, relation_types)
                replica_check_list.append(replica_check)
                replica_check.start()

    for replica_check in replica_check_list:
        replica_check.join()
        if not replica_check.result:
            failed = True

    if failed:
        print("replica check failed for one or more segments. Please check above logs for details.")
        sys.exit(1)

    print("replica check succeeded")

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
    create_restartpoint_on_ckpt_record_replay(True)
    start_verification(get_segments(), get_databases(args.databases), args.relation_types)
    create_restartpoint_on_ckpt_record_replay(False)
