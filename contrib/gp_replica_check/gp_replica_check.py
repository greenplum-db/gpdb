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
    def __init__(self, row, relation_types):
        super(ReplicaCheck, self).__init__()
        self.host = row[0]
        self.port = row[1]
        self.content = row[2]
        self.primary_status = row[3]
        self.datname = row[4]
        self.ploc = row[5]
        self.mloc = row[6]
        self.relation_types = relation_types;
        self.result = False

    def __str__(self):
        return 'Host: %s, Port: %s, Database: %s\n\
Primary Filespace Location: %s\n\
Mirror Filespace Location: %s' % (self.host, self.port, self.datname,
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

def run_checkpoint():
    print "Calling checkpoint:"
    print subprocess.check_output('psql postgres -t -c "CHECKPOINT"', stderr=subprocess.STDOUT, shell=True)

def get_fsmap(databases):
    fslist_sql = '''
SELECT gscp.address,
       gscp.port,
       gscp.content,
       gscp.status,
       pdb.datname,
       CASE WHEN tep.oid = 1663 THEN fep.fselocation || '/' || 'base/' || pdb.oid
            WHEN tep.oid = 1664 THEN fep.fselocation || '/' || 'global'
            ELSE fep.fselocation || '/' || tep.oid
       END AS ploc,
       CASE WHEN tem.oid = 1663 THEN fem.fselocation || '/' || 'base/' || pdb.oid
            WHEN tem.oid = 1664 THEN fem.fselocation || '/' || 'global'
            ELSE fem.fselocation || '/' || tem.oid
       END AS mloc
FROM gp_segment_configuration gscp, pg_filespace_entry fep, pg_tablespace tep,
     gp_segment_configuration gscm, pg_filespace_entry fem, pg_tablespace tem,
     pg_filespace pfs, pg_database pdb
WHERE fep.fsedbid = gscp.dbid
      AND fem.fsedbid = gscm.dbid
      AND ((tep.oid = 1663 AND tem.oid = 1663) OR (tep.oid = 1664 AND tem.oid = 1664 AND pdb.oid = 1))
      AND gscp.content = gscm.content
      AND gscp.role = 'p'
      AND gscm.role = 'm'
      AND pfs.oid = fep.fsefsoid
      AND pdb.datname != 'template0'
'''

    database_list = map(str.strip, databases.split(','))

    fsmap = {}
    fslist = subprocess.check_output('psql postgres -t -c "%s"' % fslist_sql, stderr=subprocess.STDOUT, shell=True).split('\n')
    for fsrow in fslist:
        fselements = map(str.strip, fsrow.split('|'))
        if len(fselements) > 1 and (fselements[4].strip() in database_list or 'all' in database_list):
            fsmap.setdefault(fselements[2], []).append(fselements)

    return fsmap

def start_verification(fsmap, relation_types):
    replica_check_list = []
    for content, fslist in fsmap.items():
        for fsrow in fslist:
            replica_check = ReplicaCheck(fsrow, relation_types)
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
    run_checkpoint()
    start_verification(get_fsmap(args.databases), args.relation_types)
