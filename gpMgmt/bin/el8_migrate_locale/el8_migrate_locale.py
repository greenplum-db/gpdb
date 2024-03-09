#!/usr/bin/env python
#!-*- coding: utf-8 -*-
import argparse
import subprocess, sys
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
            sys.exit("No port has been set, please set env PGPORT or MASTER_DATA_DIRECTORY or specify the port in the command line")

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
        SELECT distinct(indexrelid), indexrelid::regclass::text as indexname, indrelid::regclass::text as tablename, collname, pg_get_indexdef(indexrelid)
FROM (SELECT indexrelid, indrelid, indcollation[i] coll FROM pg_index, generate_subscripts(indcollation, 1) g(i)) s
JOIN pg_collation c ON coll=c.oid
WHERE collname != 'C' and collname != 'POSIX' and indexrelid >= 16384;
        """
        index = db.query(sql).getresult()
        if index:
            logger.info("There are {} user indexes in database {} that needs reindex when doing OS upgrade from EL7->EL8.".format(len(index), dbname))
        db.close()
        return index

    def get_affected_catalog_indexes(self):
        db = self.get_default_db_conn()
        sql = """
        SELECT distinct(indexrelid), indexrelid::regclass::text as indexname, indrelid::regclass::text as tablename, collname, pg_get_indexdef(indexrelid)
FROM (SELECT indexrelid, indrelid, indcollation[i] coll FROM pg_index, generate_subscripts(indcollation, 1) g(i)) s
JOIN pg_collation c ON coll=c.oid
WHERE collname != 'C' and collname != 'POSIX' and indexrelid < 16384;
        """
        index = db.query(sql).getresult()
        if index:
            logger.info("There are {} catalog indexes that needs reindex when doing OS upgrade from EL7->EL8.".format(len(index)))
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
            print>>f, "\c ", self.dbname
        for indexrelid, indexname, tablename, collname, indexdef in cindex:
            print>>f, "-- catalog indexrelid:", indexrelid, "| index name:", indexname, "| table name:", tablename, "| collname:", collname, "| indexdef: ", indexdef
            print>>f, self.handle_one_index(indexname)
            print>>f

        # print all user indexes in all databases that might be affected.
        for dbname in dblist:
            index = self.get_affected_user_indexes(dbname)
            if index:
                print>>f, "\c ", dbname
            for indexrelid, indexname, tablename, collname, indexdef in index:
                print>>f, "-- indexrelid:", indexrelid, "| index name:", indexname, "| table name:", tablename, "| collname:", collname, "| indexdef: ", indexdef
                print>>f, self.handle_one_index(indexname)
                print>>f

        f.close()

class CheckTables(connection):
    def __init__(self, host, port, dbname, user, order_size_ascend, nthread, pre_upgrade, loglevel):
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
        self.pre_upgrade = pre_upgrade
        self.loglevel = loglevel
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
        select prelid as parrelid, prelid::regclass::text as tablename, coll as collation, attname, bool_or(parisdefault) as hasDefaultPartition from par_has_default group by (prelid, coll, attname) ;
        """

        #print more info, like the number of range-partition tables and partition key collation.
        sqlForDebug = """
        select
            p.parrelid,
            p.parrelid::regclass::text as tablename,
            t.attcollation,
            t.attrelid,
            t.attname,
            t.attnum
            from
            pg_partition p
            join pg_attribute t on p.parrelid = t.attrelid
            and t.attnum = ANY(p.paratts :: smallint[])
            and p.parkind = 'r' -- filter out the range-partition tables
            order by t.attcollation;
        """
        if self.loglevel == logging.DEBUG:
            tabsForDebug = db.query(sqlForDebug)
            result = tabsForDebug.getresult()
            logger.debug("There are {} range partitioning tables in database {}.".format(len(result), dbname))
            if len(result):
                print tabsForDebug

        # Filtered partition range table that partition key in collate types.
        filterTabs = db.query(sql)
        filterResult = filterTabs.getresult()
        if len(filterResult):
            logger.warning("There are {} range partitioning tables with partition key in collate types(like varchar, char, text) in database {}, these tables might be affected due to Glibc upgrade and should be checked when doing OS upgrade from EL7 to EL8.".format(len(filterResult), dbname))
            if self.loglevel == logging.DEBUG:
                print filterTabs

        db.close()
        return filterResult

    def try_pg_dump_db(self, oids, database_name, dest_file):
        """
        Backup tables to a file.
        """
        dmp_cmd = "pg_dump --relation-oids {} -d {} -f {}".format(oids, database_name, dest_file)
        testddlbroken_cmd = "createdb tempdb_for_check_broken_ddl && {} && psql -d tempdb_for_check_broken_ddl -f {} && dropdb tempdb_for_check_broken_ddl".format(dmp_cmd, dest_file)
        print testddlbroken_cmd
        p = subprocess.Popen(testddlbroken_cmd, shell=True, stderr=subprocess.PIPE)
        if p.wait() is not 0:
            sys.stderr.write('\nError while dumping schema.\n\n' + p.communicate()[1] + '\n\n')
            sys.exit(1)
        else:
            output, errors = p.communicate()
            print output
            print errors

    def check_broken_partition_ddl(self, tables, dbname):
        # only check the broken partition ddl after OS upgrade
        if self.pre_upgrade:
            return
        # get the oids of the tables, then check by using pg_dump
        oids = []
        for parrelid, tablename, coll, attname, has_default_partition in tables:
            oids.append(parrelid)

        if len(oids) > 0:
            result = ','.join(str(i) for i in oids)
            self.try_pg_dump_db(result, dbname, "check_broken_ddl.sql")

    # get the tables which distribution column is using custom operator class, it may be affected by the OS upgrade, so give a warning.
    def get_custom_opclass_as_distribute_keys_tables(self, dbname):
        db = self.get_db_conn(dbname)
        sql = """
        select table_oid::regclass::text as tablename, max(distclass) as distclass from (select localoid , unnest(distclass::int[]) distclass from gp_distribution_policy) x(table_oid, distclass) group by table_oid having max(distclass) > 16384;
        """
        tables = db.query(sql)
        result = tables.getresult()
        if result:
            logger.warning("There are {} tables in database {} that the distribution key is using custom operator class, should be checked when doing OS upgrade from EL7 to EL8.".format(len(result), dbname))
            print tables
        db.close()

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
        bakname = "{}".format(self.escape_identifier(name + "_bak"))
        sql = """
        begin; create temp table {1} as select * from {0}; truncate {0}; insert into {0} select * from {1}; commit;
        """.format(name, bakname)
        return sql.strip()

    def get_table_size_info(self, dbname, parrelid):
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

    def dump_tables(self, fn):
        dblist = self.get_db_list()
        f = open(fn, "w")

        for dbname in dblist:
            table_info = []
            # check tables that the distribution columns are using custom operator class
            self.get_custom_opclass_as_distribute_keys_tables(dbname)

            # get all the might-affected partitioned tables
            tables = self.get_affected_partitioned_tables(dbname)

            # check the ddl for the above partition range tables
            self.check_broken_partition_ddl(tables, dbname)

            if tables:
                # if check before os upgrade, it will print the SQL results and doesn't do the GUC check.
                if self.pre_upgrade:
                    for parrelid, tablename, coll, attname, has_default_partition in tables:
                        # get the partition table size info to estimate the time
                        msg, size = self.get_table_size_info(dbname, parrelid)
                        table_info.append((parrelid, tablename, coll, attname, msg, size))
                        # if no default partition, give a warning, in case of migrate failed
                        if has_default_partition == 'f':
                            logger.warning("no default partition for {}".format(tablename))
                else:
                    # start multiple threads to check if the rows are still in the correct partitions after os upgrade, if check failed, add these tables to filtertabs
                    for t in tables:
                        # qlist is used by multiple threads
                        self.qlist.put(t)
                    self.concurrent_check(dbname)
                    table_info = self.filtertabs[:]
                    self.filtertabs = []

            # dump the table info to the specified output file
            if table_info:
                print>>f, "-- order table by size in %s order " % 'ascending' if self.order_size_ascend else '-- order table by size in descending order'
                print>>f, "\c ", dbname
                print>>f

                # sort the tables by size
                if self.order_size_ascend:
                    self.filtertabs.sort(key=lambda x: x[-1], reverse=False)
                else:
                    self.filtertabs.sort(key=lambda x: x[-1], reverse=True)

                for result in table_info:
                    parrelid = result[0]
                    name = result[1]
                    coll = result[2]
                    attname = result[3]
                    msg = result[4]
                    print>>f, "-- parrelid:", parrelid, "| coll:", coll, "| attname:", attname, "| msg:", msg
                    print>>f, self.handle_one_table(name)
                    print>>f

        # print the total partition table size
        self.print_size_summary_info()

        f.close()

    def print_size_summary_info(self):
        print "---------------------------------------------"
        KB = float(1024)
        MB = float(KB ** 2)
        GB = float(KB ** 3)
        if self.total_root_size < KB:
            print("total partition tables size  : {} Bytes".format(int(float(self.total_root_size))))
        elif KB <= self.total_root_size < MB:
            print("total partition tables size  : {} KB".format(int(float(self.total_root_size) / KB)))
        elif MB <= self.total_root_size < GB:
            print("total partition tables size  : {} MB".format(int(float(self.total_root_size) / MB)))
        else:
            print("total partition tables size  : {} GB".format(int(float(self.total_root_size) / GB)))

        print("total partition tables       : {}".format(self.total_roots))
        print("total leaf partitions        : {}".format(self.total_leafs))
        print "---------------------------------------------"

    # start multiple threads to do the check
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
    # check these tables by using GUC gp_detect_data_correctness, dump the error tables to the output file
    def check_partitiontables_by_guc(self, idx, dbname):
        logger.info("worker[{}]: begin: ".format(idx))
        logger.info("worker[{}]: connect to <{}> ...".format(idx, dbname))
        start = time.time()
        db = self.get_db_conn(dbname)
        has_error = False

        try:
            db.query("show gp_detect_data_correctness;")
        except Exception as e:
            logger.warning("Failed to get GUC gp_detect_data_correctness")
        else:
            while not self.qlist.empty():
                result = self.qlist.get()
                parrelid = result[0]
                tablename = result[1]
                coll = result[2]
                attname = result[3]
                has_default_partition = result[4]

                db.query("set gp_detect_data_correctness = 1;")
                # get the leaf partition names
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
                        has_error = True

                # if check failed, dump the table to the specified out file.
                if has_error:
                    # get the partition table size info to estimate the time
                    msg, size = self.get_table_size_info(dbname, parrelid)
                    self.filtertabslock.acquire()
                    self.filtertabs.append((parrelid, tablename, coll, attname, msg, size))
                    self.filtertabslock.release()
                    has_error = False
                    if has_default_partition == 'f':
                        logger.warning("no default partition for {}".format(tablename))

                db.query("set gp_detect_data_correctness = 0;")

                end = time.time()
                total_time = end - start
                logger.info("Current progress: have {} remaining, {} seconds passed.".format(self.qlist.qsize(), round(total_time, 2)))

        finally:
            if db:
                db.close()
            logger.info("worker[{}]: finish.".format(idx))

class migrate(connection):
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
                if sql.startswith("\c"):
                    db_name = sql.split("\c")[1].strip()
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
    parser = argparse.ArgumentParser(prog='el8_migrate_locale')
    parser.add_argument('--host', type=str, help='Greenplum Database hostname')
    parser.add_argument('--port', type=int, help='Greenplum Database port')
    parser.add_argument('--dbname', type=str,  default='postgres', help='Greenplum Database database name')
    parser.add_argument('--user', type=str, help='Greenplum Database user name')
    parser.add_argument('--verbose', help="Print more info", action="store_const", dest="loglevel", const=logging.DEBUG, default=logging.INFO)

    subparsers = parser.add_subparsers(help='sub-command help', dest='cmd')
    parser_precheck_index = subparsers.add_parser('precheck-index', help='list affected index')
    required = parser_precheck_index.add_argument_group('required arguments')
    required.add_argument('--out', type=str, help='outfile path for the reindex commands', required=True)

    parser_precheck_table = subparsers.add_parser('precheck-table', help='list affected tables')
    required = parser_precheck_table.add_argument_group('required arguments')
    required.add_argument('--out', type=str, help='outfile path for the rebuild partition commands', required=True)
    parser_precheck_table.add_argument('--pre_upgrade', action='store_true', help='check tables before os upgrade to EL8')
    parser_precheck_table.add_argument('--order_size_ascend', action='store_true', help='sort the tables by size in ascending order')
    parser_precheck_table.set_defaults(order_size_ascend=False)
    parser_precheck_table.add_argument('--nthread', type=int, default=1, help='the concurrent threads to check partition tables')

    parser_run = subparsers.add_parser('migrate', help='run the reindex and the rebuild partition commands')
    required = parser_run.add_argument_group('required arguments')
    required.add_argument('--input', type=str, help='the file contains reindex or rebuild partition commands', required=True)

    args = parser.parse_args()
    return args

if __name__ == "__main__":
    args = parseargs()
    # initialize logger
    logging.basicConfig(level=args.loglevel, stream=sys.stdout, format="%(asctime)s - %(levelname)s - %(message)s")
    logger = logging.getLogger()

    if args.cmd == 'precheck-index':
        ci = CheckIndexes(args.host, args.port, args.dbname, args.user)
        ci.dump_index_info(args.out)
    elif args.cmd == 'precheck-table':
        ct = CheckTables(args.host, args.port, args.dbname, args.user, args.order_size_ascend, args.nthread, args.pre_upgrade, args.loglevel)
        ct.dump_tables(args.out)
    elif args.cmd == 'migrate':
        cr = migrate(args.dbname, args.port, args.host, args.user, args.input)
        cr.run()
    else:
        sys.stderr.write("unknown subcommand!")
        sys.exit(127)
