#!-*- coding: utf-8 -*-
import argparse
import sys
from pygresql.pg import DB
import logging
import signal
from multiprocessing import Queue
from threading import Thread, Lock
import time
import string
from collections import defaultdict
import os
import re
try:
    from pygresql import pg
except ImportError, e:
    sys.exit('ERROR: Cannot import modules.  Please check that you have sourced greenplum_path.sh.  Detail: ' + str(e))

dbkeywords = "-- DB name: "

class connection(object):
    def __init__(self, host, port, dbname, user):
        self.host = host
        self.port = port
        self.dbname = dbname
        self.user = user
    
    def _get_pg_port(self, port):
        if port is not None:
            return port
        try:
            port = os.environ.get('PGPORT')
            if not port:
                port = self.get_port_from_conf()
            return int(port)
        except:
            sys.exit("No port been set, please set env PGPORT or MASTER_DATA_DIRECTORY or specify the port in the command line")

    def get_port_from_conf(self):
        datadir = os.environ.get('MASTER_DATA_DIRECTORY')
        if datadir:
            file = datadir +'/postgresql.conf'
            if os.path.isfile(file):
                with open(file) as f:
                    for line in f.xreadlines():
                        match = re.search('port=\d+',line)
                        if match:
                            match1 = re.search('\d+', match.group())
                            if match1:
                                return match1.group()

    def get_default_db_conn(self):
        db = DB(dbname=self.dbname,
                host=self.host,
                port=self._get_pg_port(self.port), 
                user=self.user)
        return db
    
    def get_db_conn(self, dbname):
        db = DB(dbname=dbname,
                host=self.host,
                port=self._get_pg_port(self.port),
                user=self.user)
        return db
    
    def get_db_list(self):
        db = self.get_default_db_conn()
        sql = "select datname from pg_database where datname not in ('template0');"      
        dbs = [datname for datname, in db.query(sql).getresult()]
        db.close
        return dbs

class CheckIndexes(connection):
    def get_affected_user_indexes(self, dbname):
        db = self.get_db_conn(dbname)
        # The built-in collatable data types are text,varchar,and char, and the indcollation contains the OID of the collation 
        # to use for the index, or zero if the column is not of a collatable data type.
        sql = """
        SELECT indexrelid::regclass::text, indrelid::regclass::text, coll, collname, pg_get_indexdef(indexrelid)
FROM (SELECT indexrelid, indrelid, indcollation[i] coll FROM pg_index, generate_subscripts(indcollation, 1) g(i)) s
JOIN pg_collation c ON coll=c.oid
WHERE collname != 'C' and collname != 'POSIX' and indexrelid >= 16384;
        """
        index = db.query(sql).getresult()
        logger.info("There are {} user indexes in database {} that might be affected due to upgrade.".format(len(index), dbname))
        db.close()
        return index

    def get_affected_catalog_indexes(self):
        db = self.get_default_db_conn()
        sql = """
        SELECT indexrelid::regclass::text, indrelid::regclass::text, coll, collname, pg_get_indexdef(indexrelid)
FROM (SELECT indexrelid, indrelid, indcollation[i] coll FROM pg_index, generate_subscripts(indcollation, 1) g(i)) s
JOIN pg_collation c ON coll=c.oid
WHERE collname != 'C' and collname != 'POSIX' and indexrelid < 16384;
        """
        index = db.query(sql).getresult()
        logger.info("There are {} catalog indexes that might be affected due to upgrade.".format(len(index)))
        db.close()
        return index

    def handle_one_index(self, name):
        # no need to handle special charactor here, because the name will include the double quotes if it has special charactors.
        sql = """
        reindex index {};
        """.format(name)
        return sql.strip()

    def dump_index_info(self, fn):
        dblist = self.get_db_list()
        f = open(fn, "w")

        # print all catalog indexes that might be affected.
        cindex = self.get_affected_catalog_indexes()
        if cindex:
            print>>f, dbkeywords, self.dbname
        for indexname, tablename, collate, collname, indexdef in cindex:
            print>>f, "-- catalog index name:", indexname, "| table name:", tablename, "| collate:", collate, "| collname:", collname, "| indexdef: ", indexdef
            print>>f, self.handle_one_index(indexname)
            print>>f

        # print all user indexes in all databases that might be affected.
        for dbname in dblist:
            index = self.get_affected_user_indexes(dbname)
            if index:
                print>>f, dbkeywords, dbname
            for indexname, tablename, collate, collname, indexdef in index:
                print>>f, "-- index name:", indexname, "| table name:", tablename, "| collate:", collate, "| collname:", collname, "| indexdef: ", indexdef
                print>>f, self.handle_one_index(indexname)
                print>>f

        f.close()

class CheckTables(connection):
    def __init__(self, host, port, dbname, user, order_size_ascend, nthread):
        self.host = host
        self.port = port
        self.dbname = dbname
        self.user = user
        self.order_size_ascend = order_size_ascend
        self.nthread = nthread
        self.filtertabs = []
        self.filtertabslock = Lock()
        self.total_leafs = 0
        self.total_roots = 0
        self.total_root_size = 0
        self.lock = Lock()
        self.qlist = Queue()
        signal.signal(signal.SIGTERM, self.sig_handler)
        signal.signal(signal.SIGINT, self.sig_handler)

    def get_affected_partitioned_tables(self, dbname):
        db = self.get_db_conn(dbname)
        # The built-in collatable data types are text,varchar,and char, and the defined collation of the column, or zero if the column is not of a collatable data type
        # filter the partition by list, because only partiton by range might be affected.
        sql = """
        WITH might_affected_tables AS (
        SELECT
        prelid,
        coll,
        attname,
        attnum,
        parisdefault
        FROM
        (
            select
            p.oid as poid,
            p.parrelid as prelid,
            t.attcollation coll,
            t.attname as attname,
            t.attnum as attnum
            from
            pg_partition p
            join pg_attribute t on p.parrelid = t.attrelid
            and t.attnum = ANY(p.paratts :: smallint[])
            and p.parkind = 'r'
        ) s
        JOIN pg_collation c ON coll = c.oid
        JOIN pg_partition_rule r ON poid = r.paroid
        WHERE
        collname != 'C' and collname != 'POSIX' 
        ),
        par_has_default AS (
        SELECT
        prelid,
        coll,
        attname,
        parisdefault
        FROM 
        might_affected_tables group by (prelid, coll, attname, parisdefault)
        )
        select prelid, prelid::regclass::text as partitionname, coll, attname, bool_or(parisdefault) as parhasdefault from par_has_default group by (prelid, coll, attname) ;
        """
        tabs = db.query(sql).getresult()
        logger.info("There are {} partitioned tables in database {} that might be affected due to upgrade.".format(len(tabs), dbname))
        db.close()
        return tabs

    # Escape double-quotes in a string, so that the resulting string is suitable for
    # embedding as in SQL. Analogouous to libpq's PQescapeIdentifier
    def escape_identifier(self, str):
        # Does the string need quoting? Simple strings with all-lower case ASCII
        # letters don't.
        SAFE_RE = re.compile('[a-z][a-z0-9_]*$')

        if SAFE_RE.match(str):
            return str

        # Otherwise we have to quote it. Any double-quotes in the string need to be escaped
        # by doubling them.
        return '"' + str.replace('"', '""') + '"'

    def handle_one_table(self, name):
        bakname = "%s" % (self.escape_identifier(name + "_bak"))
        sql = """
        begin; create temp table {1} as select * from {0}; truncate {0}; insert into {0} select * from {1}; commit;
        """.format(name, bakname)
        return sql.strip()

    def dump_table_info(self, dbname, parrelid):
        db = self.get_db_conn(dbname)
        sql_size = """
        with recursive cte(nlevel, table_oid) as (
            select 0, {}::regclass::oid
            union all
            select nlevel+1, pi.inhrelid
            from cte, pg_inherits pi
            where cte.table_oid = pi.inhparent
        )
        select sum(pg_relation_size(table_oid)) as size, count(1) as nleafs
        from cte where nlevel = (select max(nlevel) from cte);
        """
        r = db.query(sql_size.format(parrelid))
        size = r.getresult()[0][0]
        nleafs = r.getresult()[0][1]
        self.lock.acquire()
        self.total_root_size += size
        self.total_leafs += nleafs
        self.total_roots += 1
        self.lock.release()
        db.close()
        return "partition table, %s leafs, size %s" % (nleafs, size), size

    def dump_partition_tables(self, fn):
        dblist = self.get_db_list()
        f = open(fn, "w")

        for dbname in dblist:
            # get all the might-affected partitioned tables
            tables = self.get_affected_partitioned_tables(dbname)

            for t in tables:
                self.qlist.put(t)
            
            if tables:
                self.concurrent_check(dbname)

            if self.filtertabs:
                print>>f, "-- order table by size in %s order " % 'ascending' if self.order_size_ascend else '-- order table by size in descending order'
                print>>f, dbkeywords, dbname
                print>>f

            # get the filter partition tables, sort it by size and dump table info into report
            if self.filtertabs:
                if self.order_size_ascend:
                    self.filtertabs.sort(key=lambda x: x[-1], reverse=False)
                else:
                    self.filtertabs.sort(key=lambda x: x[-1], reverse=True)

            for result in self.filtertabs:
                parrelid = result[0]
                name = result[1]
                coll = result[2]
                attname = result[3]
                msg = result[4]
                print>>f, "-- parrelid:", parrelid, "| coll:", coll, "| attname:", attname, "| msg:", msg
                print>>f, self.handle_one_table(name)
                print>>f

            self.filtertabs = []
        
        logger.info("total table size (in GBytes) : {}".format(float(self.total_root_size) / 1024.0**3))
        logger.info("total partition tables       : {}".format(self.total_roots))
        logger.info("total leaf partitions        : {}".format(self.total_leafs))
        f.close()

    def concurrent_check(self, dbname):
        threads = []
        for i in range(self.nthread):
            t = Thread(target=CheckTables.check_partitiontables_by_guc,
                        args=[self, i, dbname])
            threads.append(t)
        for t in threads:
            t.start()
        for t in threads:
            t.join()
    
    def sig_handler(self, sig, arg):
        sys.stderr.write("terminated by signal %s\n" % sig)
        sys.exit(127)

    @staticmethod
    def check_partitiontables_by_guc(self, idx, dbname):
        logger.info("worker[{}]: begin: ".format(idx))
        logger.info("worker[{}]: connect to <{}> ...".format(idx, dbname))
        start = time.time()
        db = self.get_db_conn(dbname)
        has_error = False

        while not self.qlist.empty():
            result = self.qlist.get()
            parrelid = result[0]
            tablename = result[1]
            coll = result[2]
            attname = result[3]
            has_default_partition = result[4]
            # filter the tables by GUC gp_detect_data_correctness, only dump the error tables to the output file
            try:
                db.query("set gp_detect_data_correctness = 1;")
            except Exception as e:
                logger.warning("missing GUC gp_detect_data_correctness")
                db.close()

            get_partitionname_sql = """
            with recursive cte(root_oid, table_oid, nlevel) as (
                select parrelid, parrelid, 0 from pg_partition where not paristemplate and parlevel = 0
                union all
                select root_oid,  pi.inhrelid, nlevel+1
                from cte, pg_inherits pi
                where cte.table_oid = pi.inhparent
            )
            select root_oid::regclass::text as tablename, table_oid::regclass::text as partitioname
            from cte where nlevel = (select max(nlevel) from cte) and root_oid = {};
            """
            partitiontablenames = db.query(get_partitionname_sql.format(parrelid)).getresult()
            for tablename, partitioname in partitiontablenames:
                sql = "insert into {tab} select * from {tab}".format(tab=partitioname)
                try:
                    logger.info("start checking table {tab} ...".format(tab=partitioname))
                    db.query(sql)
                    logger.info("check table {tab} OK.".format(tab=partitioname))
                except Exception as e:
                    logger.info("check table {tab} error out: {err_msg}".format(tab=partitioname, err_msg=str(e)))
                    has_errror = True;

            if has_error:
                msg, size = self.dump_table_info(dbname, parrelid)
                self.filtertabslock.acquire()
                self.filtertabs.append((parrelid, tablename, coll, attname, msg, size))
                self.filtertabslock.release()
                if has_default_partition == 'f':
                    logger.warning("no default partition for {}".format(tablename))

            db.query("set gp_detect_data_correctness = 0;")
            
            end = time.time()
            total_time = end - start
            logger.info("Current progress: have {} remaining, {} seconds passed.".format(self.qlist.qsize(), total_time))

        db.close()
        logger.info("worker[{}]: finish.".format(idx))

class PostFix(connection): 
    def __init__(self, dbname, port, host, user, script_file):
        self.dbname = dbname
        self.port = self._get_pg_port(port)
        self.host = host
        self.user = user
        self.script_file = script_file
        self.dbdict = defaultdict(list)

        self.parse_inputfile()

    def parse_inputfile(self):
        with open(self.script_file) as f:
            for line in f:
                sql = line.strip()
                if sql.startswith(dbkeywords):
                    db_name = sql.split(dbkeywords)[1].strip()
                if (sql.startswith("reindex") and sql.endswith(";") and sql.count(";") == 1):
                    self.dbdict[db_name].append(sql)
                if (sql.startswith("begin;") and sql.endswith("commit;")):
                    self.dbdict[db_name].append(sql)

    def run(self):
        try:
            for db_name, commands in self.dbdict.items():
                total_counts = len(commands)
                logger.info("db: {}, total have {} commands to execute".format(db_name, total_counts))
                for command in commands:
                    self.run_alter_command(db_name, command)
        except KeyboardInterrupt:
            sys.exit('\nUser Interrupted')

        logger.info("All done")

    def run_alter_command(self, db_name, command):
        try:
            db = self.get_db_conn(db_name)
            logger.info("db: {}, executing command: {}".format(db_name, command))
            db.query(command)

            if (command.startswith("begin")):
                pieces = [p for p in re.split("( |\\\".*?\\\"|'.*?')", command) if p.strip()]
                index = pieces.index("truncate")
                if 0 < index < len(pieces) - 1:
                    table_name = pieces[index+1]
                    analyze_sql = "analyze {};".format(table_name)
                    logger.info("db: {}, executing analyze command: {}".format(db_name, analyze_sql))
                    db.query(analyze_sql)

            db.close()
        except Exception, e:
            logger.error("{}".format(str(e)))

def parseargs():
    parser = argparse.ArgumentParser(prog='upgrade_check')
    parser.add_argument('--host', type=str, help='Greenplum Database hostname')
    parser.add_argument('--port', type=int, help='Greenplum Database port')
    parser.add_argument('--dbname', type=str,  default='postgres', help='Greenplum Database database name')
    parser.add_argument('--user', type=str, help='Greenplum Database user name')

    subparsers = parser.add_subparsers(help='sub-command help', dest='cmd')
    parser_precheck_index = subparsers.add_parser('precheck-index', help='list affected index')
    parser_precheck_table = subparsers.add_parser('precheck-table', help='list affected tables')
    parser_precheck_table.add_argument('--order_size_ascend', action='store_true', help='sort the tables by size in ascending order')
    parser_precheck_table.set_defaults(order_size_ascend=False)
    parser_precheck_index.add_argument('--out', type=str, help='outfile path for the reindex commands', required=True)
    parser_precheck_table.add_argument('--out', type=str, help='outfile path for the rebuild partition commands', required=True)
    parser_precheck_table.add_argument('--nthread', type=int, default=1, help='the concurrent threads to check partition tables by using GUC')

    parser_run = subparsers.add_parser('postfix', help='postfix run the reindex and the rebuild partition commands')
    parser_run.add_argument('--input', type=str, help='the file contains reindex or rebuild partition ccommandsmds', required=True)

    args = parser.parse_args()
    return args

if __name__ == "__main__":
    args = parseargs()
    # initialize logger
    logging.basicConfig(level=logging.DEBUG, stream=sys.stdout, format="%(asctime)s - %(levelname)s - %(message)s")
    logger = logging.getLogger()

    if args.cmd == 'precheck-index':
        ci = CheckIndexes(args.host, args.port, args.dbname, args.user)
        ci.dump_index_info(args.out)
    elif args.cmd == 'precheck-table':
        ct = CheckTables(args.host, args.port, args.dbname, args.user, args.order_size_ascend, args.nthread)
        ct.dump_partition_tables(args.out)
    elif args.cmd == 'postfix':
        cr = PostFix(args.dbname, args.port, args.host, args.user, args.input)
        cr.run()
    else:
        sys.stderr.write("unknown subcommand!")
        sys.exit(127)
