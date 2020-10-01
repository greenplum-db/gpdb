#!/usr/bin/env python

# Optimizer calibration test for bitmap indexes
#
# This program runs a set of queries, varying several parameters:
#
# - Selectivity of predicates
# - Plan type (plan chosen by the optimizer, various forced plans)
# - Width of the selected columns
#
# The program then reports the result of explaining these queries and
# letting the optimizer choose plans vs. forcing plans. Execution time
# can be reported, computing a mean and standard deviation of several
# query executions.
#
# The printed results are useful to copy and paste into a Google Sheet
# (expand columns after pasting)
#
# Run this program with the -h or --help option to see argument syntax
#
# See comment "How to add a test" below in the program for how to
# extend this program.

import argparse
import time
import re
import math

try:
    from gppylib.db import dbconn
except ImportError, e:
    sys.exit('ERROR: Cannot import modules.  Please check that you have sourced greenplum_path.sh.  Detail: ' + str(e))

# constants
# -----------------------------------------------------------------------------

_help = """
Run optimizer bitmap calibration tests. Optionally create the tables before running, and drop them afterwards.
This explains and runs a series of queries and reports the estimated and actual costs.
The results can be copied and pasted into a spreadsheet for further processing.
"""

TABLE_NAME_PATTERN = r"cal_txtest"
NDV_TABLE_NAME_PATTERN = r"cal_ndvtest"
BFV_TABLE_NAME_PATTERN = r"cal_bfvtest"

TABLE_SCAN = "table_scan"
TABLE_SCAN_PATTERN = r"Seq Scan"
TABLE_SCAN_PATTERN_V5 = r"Table Scan"

INDEX_SCAN = "index_scan"
INDEX_SCAN_PATTERN = r">  Index Scan"
INDEX_SCAN_PATTERN_V5 = r">  Index Scan"

BITMAP_SCAN = "bitmap_scan"
BITMAP_SCAN_PATTERN = r"Bitmap Heap Scan"
BITMAP_SCAN_PATTERN_V5 = r"Bitmap Table Scan"

HASH_JOIN = "hash_join"
HASH_JOIN_PATTERN = r"Hash Join"
HASH_JOIN_PATTERN_V5 = r"Hash Join"

NL_JOIN = "nl_join"
NL_JOIN_PATTERN = r"Nested Loop"
NL_JOIN_PATTERN_V5 = r"Nested Loop"

OPTIMIZER_DEFAULT_PLAN = "optimizer"



# global variables
# -----------------------------------------------------------------------------

glob_verbose=False
# only consider optimizer errors beyond x * sigma (standard deviation) as significant
glob_sigma_diff=3
glob_log_file=None
glob_exe_timeout=40000
glob_gpdb_major_version=7


# SQL statements, DDL and DML
# -----------------------------------------------------------------------------

_drop_tables = """
DROP TABLE IF EXISTS cal_txtest, cal_temp_ids, cal_dim, cal_bfvtest, cal_bfv_dim, cal_ndvtest;
"""

# create the table. Parameters:
# - WITH clause (optional), for append-only tables
_create_cal_table = """
CREATE TABLE cal_txtest(id int,
                        btreeunique int,
                        btree10 int,
                        btree100 int,
                        btree1000 int,
                        btree10000 int,
                        bitmap10 int,
                        bitmap100 int,
                        bitmap1000 int,
                        bitmap10000 int,
                        txt text)
%s
DISTRIBUTED BY (id);
"""

_create_bfv_table = """
CREATE TABLE cal_bfvtest (col1 integer,
                          wk_id int,
                          id integer)
%s
DISTRIBUTED BY (col1);
"""

_create_ndv_table = """
CREATE TABLE cal_ndvtest (id int, val int)
%s
DISTRIBUTED BY (id);
"""

_with_appendonly = """
WITH (appendonly=true, compresslevel=5, compresstype=zlib)
"""

_create_other_tables = [ """
CREATE TABLE cal_temp_ids(f_id int, f_rand double precision) DISTRIBUTED BY (f_id);
""",
                         """
CREATE TABLE cal_dim(dim_id int,
                     dim_id2 int,
                     txt text)
DISTRIBUTED BY (dim_id);
""",
"""
CREATE TABLE cal_bfv_dim (id integer, col2 integer) DISTRIBUTED BY (id);
""" ]

# insert into temp table. Parameters:
# - integer start value (usually 0 or 1)
# - integer stop value (suggested value is 10000000)
_insert_into_temp = """
INSERT INTO cal_temp_ids SELECT x, random() FROM (SELECT * FROM generate_series(%d,%d)) T(x);
"""

_insert_into_table = """
INSERT INTO cal_txtest
SELECT f_id,
       f_id,
       f_id%10,
       f_id%100,
       f_id%1000,
       f_id%10000,
       f_id%10,
       f_id%100,
       f_id%1000,
       f_id%10000,
       repeat('a', 900)
FROM cal_temp_ids
order by f_rand;
"""

_insert_into_other_tables = """
INSERT INTO cal_dim SELECT x, x, repeat('d', 100) FROM (SELECT * FROM generate_series(%d,%d)) T(x);
"""

_create_index_arr = ["""
CREATE INDEX cal_txtest_i_bitmap_10    ON cal_txtest USING bitmap(bitmap10);
""",
                   """
CREATE INDEX cal_txtest_i_bitmap_100   ON cal_txtest USING bitmap(bitmap100);
""",
                   """
CREATE INDEX cal_txtest_i_bitmap_1000  ON cal_txtest USING bitmap(bitmap1000);
""",
                   """
CREATE INDEX cal_txtest_i_bitmap_10000 ON cal_txtest USING bitmap(bitmap10000);
""",
    ]

_create_bfv_index_arr = ["""
CREATE INDEX idx_cal_bfvtest_bitmap ON cal_bfvtest USING bitmap(id);
""",
    ]

_create_ndv_index_arr = ["""
CREATE INDEX cal_ndvtest_bitmap ON cal_ndvtest USING bitmap(val);
""",
    ]

_create_btree_indexes_ao_arr = ["""
CREATE INDEX cal_txtest_i_btree_unique ON cal_txtest USING btree(btreeunique);
""",
                   """
CREATE INDEX cal_txtest_i_btree_10     ON cal_txtest USING btree(btree10);
""",
                   """
CREATE INDEX cal_txtest_i_btree_100    ON cal_txtest USING btree(btree100);
""",
                   """
CREATE INDEX cal_txtest_i_btree_1000   ON cal_txtest USING btree(btree1000);
""",
                   """
CREATE INDEX cal_txtest_i_btree_10000  ON cal_txtest USING btree(btree10000);
""",
                   """
CREATE INDEX idx_cal_bfvtest_btree ON cal_bfvtest USING btree(id);
""",
                   """
CREATE INDEX cal_ndvtest_btree ON cal_ndvtest USING btree(val);
""",
]

_analyze_table = """
ANALYZE cal_txtest;
"""

_allow_system_mods = """
SET allow_system_table_mods to on;
"""

_allow_system_mods_v5 = """
SET allow_system_table_mods to 'dml';
"""

# Make sure pg_statistics has smooth and precise statistics, so that the cardinality estimates we get are very precise
#
# For NDVs of 100 or less, list all of them
# For NDVs of more than 100, make some dummy NDVs and 5 intervals of the same length
# So far, id and btreeunique are not yet used (staattnums 1 and 2), no stats are changed

_fix_statistics = ["""
UPDATE pg_statistic
  SET stadistinct = 10,
      stakind1 = 1,
      stanumbers1 = '{ 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1 }',
	  stavalues1 = '{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}'::int[]
WHERE starelid = 'cal_txtest'::regclass AND staattnum = 3;
""",
                   """
UPDATE pg_statistic
  SET stadistinct = 100,
      stakind1 = 1,
      stanumbers1 = '{ 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01 }',
	  stavalues1 = '{ 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99 }'::int[]
WHERE starelid = 'cal_txtest'::regclass AND staattnum = 4;
""",
                   """
UPDATE pg_statistic
  SET stadistinct = 1000,
      stakind1 = 1,
      stanumbers1 = '{ 0.001, 0.001, 0.001 }',
	  stavalues1 = '{100, 200, 300}'::int[],
	  stakind2 = 2,
	  stanumbers2 = '{}',
	  stavalues2 = '{0, 199, 399, 599, 799, 999}'::int[]
WHERE starelid = 'cal_txtest'::regclass AND staattnum = 5;
""",
                   """
UPDATE pg_statistic
  SET stadistinct = 10000,
      stakind1 = 1,
      stanumbers1 = '{ 0.0001, 0.0001, 0.0001 }',
	  stavalues1 = '{1000, 2000, 3000}'::int[],
	  stakind2 = 2,
	  stanumbers2 = '{}',
	  stavalues2 = '{0, 1999, 3999, 5999, 7999, 9999}'::int[]
WHERE starelid = 'cal_txtest'::regclass AND staattnum = 6;
""",
                   """
UPDATE pg_statistic
  SET stadistinct = 10,
      stakind1 = 1,
      stanumbers1 = '{ 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1 }',
	  stavalues1 = '{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}'::int[]
WHERE starelid = 'cal_txtest'::regclass AND staattnum = 7;
""",
                   """
UPDATE pg_statistic
  SET stadistinct = 100,
      stakind1 = 1,
      stanumbers1 = '{ 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01 }',
	  stavalues1 = '{ 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99 }'::int[]
WHERE starelid = 'cal_txtest'::regclass AND staattnum = 8;
""",
                   """
UPDATE pg_statistic
  SET stadistinct = 1000,
      stakind1 = 1,
      stanumbers1 = '{ 0.001, 0.001, 0.001 }',
	  stavalues1 = '{100, 200, 300}'::int[],
	  stakind2 = 2,
	  stanumbers2 = '{}',
	  stavalues2 = '{0, 199, 399, 599, 799, 999}'::int[]
WHERE starelid = 'cal_txtest'::regclass AND staattnum = 9;
""",
                   """
UPDATE pg_statistic
  SET stadistinct = 10000,
      stakind1 = 1,
      stanumbers1 = '{ 0.0001, 0.0001, 0.0001 }',
	  stavalues1 = '{1000, 2000, 3000}'::int[],
	  stakind2 = 2,
	  stanumbers2 = '{}',
	  stavalues2 = '{0, 1999, 3999, 5999, 7999, 9999}'::int[]
WHERE starelid = 'cal_txtest'::regclass AND staattnum = 10;
""",
                   """
UPDATE pg_statistic
  SET stadistinct = 10000,
      stakind1 = 1,
      stanumbers1 = '{ 0.0001, 0.0001, 0.0001 }',
	  stavalues1 = '{1000, 2000, 3000}'::int[],
	  stakind2 = 2,
	  stanumbers2 = '{}',
	  stavalues2 = '{0, 1999, 3999, 5999, 7999, 9999}'::int[]
WHERE starelid = 'cal_dim'::regclass AND staattnum = 1;
""",
                   """
UPDATE pg_statistic
  SET stadistinct = 10000,
      stakind1 = 1,
      stanumbers1 = '{ 0.0001, 0.0001, 0.0001 }',
	  stavalues1 = '{1000, 2000, 3000}'::int[],
	  stakind2 = 2,
	  stanumbers2 = '{}',
	  stavalues2 = '{0, 1999, 3999, 5999, 7999, 9999}'::int[]
WHERE starelid = 'cal_dim'::regclass AND staattnum = 2;
""" ]


# deal with command line arguments
# -----------------------------------------------------------------------------

def parseargs():
    parser = argparse.ArgumentParser(description=_help, version='1.0')

    parser.add_argument("tests", metavar="TEST", choices=[ [], "all", "none", "bitmap_scan_tests", "bitmap_join_tests" ], nargs="*",
                        help="Run these tests (all, bitmap_scan_tests, bitmap_join_tests), default is none")
    parser.add_argument("--create", action="store_true",
                        help="Create the tables to use in the test")
    parser.add_argument("--execute", type=int, default="0",
                        help="Number of times to execute queries, 0 (the default) means explain only")
    parser.add_argument("--drop", action="store_true",
                        help="Drop the tables used in the test when finished")
    parser.add_argument("--verbose", action="store_true",
                        help="Print more verbose output")
    parser.add_argument("--logFile", default="",
                        help="Log diagnostic output to a file")
    parser.add_argument("--host", default="localhost",
                        help="Host to connect to (default is localhost).")
    parser.add_argument("--port", type=int, default="0",
                        help="Port on the host to connect to")
    parser.add_argument("--dbName", default="",
                        help="Database name to connect to")
    parser.add_argument("--appendOnly", action="store_true",
                        help="Create an append-only table (uses only bitmap indexes). Default is a heap table")
    parser.add_argument("--numRows", type=int, default="10000000",
                        help="Number of rows to INSERT INTO the table (default is 10 million)")

    parser.set_defaults(verbose=False, filters=[], slice=(None, None))

    # Parse the command line arguments
    args = parser.parse_args()
    return args, parser

def log_output(str):
    if glob_verbose:
        print(str)
    if glob_log_file != None:
        glob_log_file.write(str + "\n")


# SQL related methods
# -----------------------------------------------------------------------------

def connect(host, port_num, db_name):
    try:
        dburl = dbconn.DbURL(hostname=host, port=port_num, dbname=db_name)
        conn = dbconn.connect(dburl, encoding="UTF8")
    except Exception as e:
        print("Exception during connect: %s" % e)
        quit()
    return conn

def select_version(conn):
    global glob_gpdb_major_version
    sqlStr = "SELECT version()"
    curs = dbconn.query(conn, sqlStr)

    rows = curs.fetchall()
    for row in rows:
        log_output(row[0])
        glob_gpdb_major_version = int(re.sub(".*Greenplum Database ([0-9]*)\..*", "\\1", row[0]))
        log_output("GPDB major version is %d" % glob_gpdb_major_version)

    log_output("Backend pid:")
    sqlStr = "SELECT pg_backend_pid()"
    curs = dbconn.query(conn, sqlStr)

    rows = curs.fetchall()
    for row in rows:
        log_output(str(row[0]))

def execute_sql(conn, sqlStr):
    try:
        log_output("")
        log_output("Executing query: %s" % sqlStr)
        dbconn.execSQL(conn, sqlStr)
    except Exception as e:
        print("")
        print("Error executing query: %s; Reason: %s" % (sqlStr, e))
        dbconn.execSQL(conn, "abort")

def execute_sql_arr(conn, sqlStrArr):
    for sqlStr in sqlStrArr:
        execute_sql(conn, sqlStr)

def execute_and_commit_sql(conn, sqlStr):
    execute_sql(conn, sqlStr)
    commit_db(conn)

def commit_db(conn):
    execute_sql(conn, "commit")

# run an SQL statement and return the elapsed wallclock time, in seconds
def timed_execute_sql(conn, sqlStr):
    start = time.time()
    execute_sql(conn, sqlStr)
    end = time.time()
    elapsed_time_in_msec = round((end-start)*1000)
    log_output("Elapsed time (msec): %.0f" % elapsed_time_in_msec)
    return elapsed_time_in_msec


# run an SQL statement n times, unless it takes longer than a timeout

def timed_execute_n_times(conn, sqlStr, exec_n_times):
    sum_exec_times = 0.0
    sum_square_exec_times = 0.0
    e = 1
    act_num_exes = exec_n_times
    while e <= act_num_exes:
        exec_time = timed_execute_sql(conn, sqlStr)
        sum_exec_times += exec_time
        sum_square_exec_times += exec_time*exec_time
        if exec_time > glob_exe_timeout:
            # we exceeded the timeout, don't keep executing this long query
            act_num_exes = e
            log_output("Query %s exceeded the timeout of %d seconds" % (sqlStr, glob_exe_timeout))
        e = e+1

    # compute mean and standard deviation of the execution times
    mean = sum_exec_times / act_num_exes
    if exec_n_times == 1:
        # be safe, avoid any rounding errors
        variance = 0.0
    else:
        variance = sum_square_exec_times / act_num_exes - mean * mean
    return (round(mean, 3), round(math.sqrt(variance), 3), act_num_exes)


# Explain a query and find a table scan or index scan in an explain output
# return the scan type and the corresponding cost.
# Use this for scan-related tests.

def explain_index_scan(conn, sqlStr):
    cost = -1.0
    scan_type = ""
    try:
        log_output("")
        log_output("Executing query: %s" % ("explain " + sqlStr))
        exp_curs = dbconn.query(conn, "explain " + sqlStr)
        rows = exp_curs.fetchall()
        table_scan_pattern = TABLE_SCAN_PATTERN
        index_scan_pattern = INDEX_SCAN_PATTERN
        bitmap_scan_pattern = BITMAP_SCAN_PATTERN
        if (glob_gpdb_major_version) <= 5:
            table_scan_pattern = TABLE_SCAN_PATTERN_V5
            index_scan_pattern = INDEX_SCAN_PATTERN_V5
            bitmap_scan_pattern = BITMAP_SCAN_PATTERN_V5
    
        for row in rows:
            log_output(row[0])
            if re.search(TABLE_NAME_PATTERN, row[0]) or re.search(NDV_TABLE_NAME_PATTERN, row[0]):
                if re.search(bitmap_scan_pattern, row[0]):
                    scan_type = BITMAP_SCAN
                    cost = cost_from_explain_line(row[0])
                elif re.search(index_scan_pattern, row[0]):
                    scan_type = INDEX_SCAN
                    cost = cost_from_explain_line(row[0])
                elif re.search(table_scan_pattern, row[0]):
                    scan_type = TABLE_SCAN
                    cost = cost_from_explain_line(row[0])

    except Exception as e:
        log_output("\n*** ERROR explaining query:\n%s;\nReason: %s" % ("explain " + sqlStr, e))

    return (scan_type, cost)


# Explain a query and find a join in an explain output
# return the scan type and the corresponding cost.
# Use this for scan-related tests.

def explain_join_scan(conn, sqlStr):
    cost = -1.0
    scan_type = ""
    try:
        log_output("")
        log_output("Executing query: %s" % ("explain " + sqlStr))
        exp_curs = dbconn.query(conn, "explain " + sqlStr)
        rows = exp_curs.fetchall()
        hash_join_pattern = HASH_JOIN_PATTERN
        nl_join_pattern = NL_JOIN_PATTERN
        table_scan_pattern = TABLE_SCAN_PATTERN
        index_scan_pattern = INDEX_SCAN_PATTERN
        bitmap_scan_pattern = BITMAP_SCAN_PATTERN
        if (glob_gpdb_major_version) <= 5:
            hash_join_pattern = HASH_JOIN_PATTERN_V5
            nl_join_pattern = NL_JOIN_PATTERN_V5
            table_scan_pattern = TABLE_SCAN_PATTERN_V5
            index_scan_pattern = INDEX_SCAN_PATTERN_V5
            bitmap_scan_pattern = BITMAP_SCAN_PATTERN_V5

        # save the cost of the join above the scan type
        for row in rows:
            log_output(row[0])
            if re.search(nl_join_pattern, row[0]):
                cost = cost_from_explain_line(row[0])
            elif re.search(hash_join_pattern, row[0]):
                cost = cost_from_explain_line(row[0])
            # mark the scan type used underneath the join
            if re.search(TABLE_NAME_PATTERN, row[0]) or re.search(BFV_TABLE_NAME_PATTERN, row[0]):
                if re.search(bitmap_scan_pattern, row[0]):
                    scan_type = BITMAP_SCAN
                elif re.search(index_scan_pattern, row[0]):
                    scan_type = INDEX_SCAN
                elif re.search(table_scan_pattern, row[0]):
                    scan_type = TABLE_SCAN

    except Exception as e:
        log_output("\n*** ERROR explaining query:\n%s;\nReason: %s" % ("explain " + sqlStr, e))

    return (scan_type, cost)


# extract the cost c from the cost=x..c in an explain line

def cost_from_explain_line(line):
    return float(re.sub(r".*\.\.([0-9.]+) .*", r"\1", line))


# methods that run queries with varying parameters, recording results
# and finding crossover points
# -----------------------------------------------------------------------------


# iterate over one parameterized query, using a range of parameter values, explaining and (optionally) executing the query

def find_crossover(conn, lowParamValue, highParamLimit, setup, parameterizeMethod, explain_method, reset_method, plan_ids, force_methods, execute_n_times):
    # expects the following:
    # - conn:               A connection
    # - lowParamValue:      The lowest (integer) value to try for the parameter
    # - highParamLimit:     The highest (integer) value to try for the parameter + 1
    # - setup:              A method that runs any sql needed for setup before a particular select run, given a parameterized query and a paramter value
    # - parameterizeMethod: A method to generate the actual query text, given a parameterized query and a parameter value
    # - explain_method:     A method that takes a connection and an SQL string and returns a tuple (plan, cost)
    # - reset_method:       A method to reset all gucs and similar switches, to get the default plan by the optimizer
    #                       the method takes one parameter, the connection
    # - plan_ids:           A list with <p> plan ids returned by explain_method. Usually the number <p> is 2.
    # - force_methods:      A list with <p> methods to force each plan id in the plan_ids array (these methods usually set gucs)
    #                       each methods takes one parameter, the connection
    # - execute_n_times:    The number of times to execute the query (0 means don't execute, n>0 means execute n times)
    
    # returns the following:
    # - An explain dictionary, containing a mapping between a subset of the parameter values and result tuples, each result tuple consisting of
    #   <p> + 2 values:
    #   - the plan id chosen by default by the optimizer
    #   - the estimated cost for the optimal plan, chosen by the optimizer
    #   - p values for the estimated cost when forcing plan i, 0 <= i < p
    # - An execution dictionary that, if execute_n_times is > 0, contains a mapping of a subset of the parameter values and plan ids
    #   to execution times and standard deviations in execution times: (param_value, plan_id) -> (mean_exec_time, stddev_exec_time)
    #   - mean_exec_time: average execution time (in seconds, rounded to milliseconds) for the plan
    #   - stddev_exec_time: standard deviation of the different execution times for this parameter value and plan
    # - A list of error messages
    explainDict = {}
    execDict = {}
    errMessages = []
    timedOutDict = {}
    expPrevPlan = ""
    expPrevParamValue = lowParamValue
    expCrossoverOccurred = False
    expCrossoverLow = lowParamValue - 1
    expCrossoverHigh = highParamLimit
    expCrossoverLowPlan = ""
    expCrossoverHighPlan = ""
    reset_method(conn)

    # determine the increment
    incParamValue = (highParamLimit - lowParamValue) / 10
    if incParamValue == 0:
        incParamValue = 1
    elif highParamLimit <= lowParamValue:
        errMessages.append("Low parameter value %d must be less than high parameter limit %d" % (lowParamValue, highParamLimit))
        return (explainDict, execDict, errMessages)

    # first part, run through the parameter values and determine the plan and cost chosen by the optimizer
    for paramValue in range(lowParamValue, highParamLimit, incParamValue):
        
        # do any setup required
        setupString = setup(paramValue)
        execute_sql(conn, setupString)
        # explain the query and record which plan it chooses and what the cost is
        sqlString = parameterizeMethod(paramValue)
        (plan, cost) = explain_method(conn, sqlString)
        explainDict[paramValue] = (plan, cost)
        log_output("For param value %d the optimizer chose %s with a cost of %f" % (paramValue, plan, cost))
        
        # look for the crossover from one plan to another
        if not expCrossoverOccurred and paramValue > lowParamValue and plan != expPrevPlan:
            expCrossoverOccurred = True
            expCrossoverLow = expPrevParamValue
            expCrossoverLowPlan = expPrevPlan
            expCrossoverHigh = paramValue
            expCrossoverHighPlan = plan
        expPrevPlan = plan
        expPrevParamValue = paramValue

        # execute the query, if requested
        if execute_n_times > 0:
            timed_execute_and_check_timeout(conn, sqlString, execute_n_times, paramValue, OPTIMIZER_DEFAULT_PLAN, execDict, timedOutDict, errMessages)

    # second part, force different plans and record the costs
    for plan_num in range(0, len(plan_ids)):
        plan_id = plan_ids[plan_num]
        reset_method(conn)
        log_output("----------- Now forcing a %s plan --------------" % plan_id)
        force_methods[plan_num](conn)
        for paramValue in range(lowParamValue, highParamLimit, incParamValue):
            # do any setup required
            setupString = setup(paramValue)
            execute_sql(conn, setupString)
      			# explain the query with the forced plan
            sqlString = parameterizeMethod(paramValue)
            (plan, cost) = explain_method(conn, sqlString)
            if plan_id != plan:
                errMessages.append("For parameter value %d we tried to force a %s plan but got a %s plan." % (paramValue, plan_id, plan))
                log_output("For parameter value %d we tried to force a %s plan but got a %s plan." % (paramValue, plan_id, plan))
            # update the result dictionary
            resultList = list(explainDict[paramValue])
            defaultPlanCost = resultList[1]
            # sanity check, the forced plan shouldn't have a cost that is lower than the default plan cost
            if defaultPlanCost > cost * 1.1:
                errMessages.append("For parameter value %d and forced %s plan we got a cost of %f that is lower than the default cost of %f for the default %s plan." % (paramValue, plan_id, cost, defaultPlanCost, resultList[0]))
            resultList.append(cost)
            explainDict[paramValue] = tuple(resultList)
            log_output("For param value %d we forced %s with a cost of %f" % (paramValue, plan, cost))

            # execute the forced plan
            if execute_n_times > 0:
                # execute the query <execute_n_times> times and record the mean and stddev of the time in execDict
                timed_execute_and_check_timeout(conn, sqlString, execute_n_times, paramValue, plan_id, execDict, timedOutDict, errMessages)

    # cleanup at exit
    reset_method(conn)

    return (explainDict, execDict, errMessages)


# Check for plans other than the optimizer-chosen plan that are significantly
# better. Return the plan id and how many percent better that plan is or return ("", 0).

def checkForOptimizerErrors(paramValue, chosenPlan, plan_ids, execDict):
    # check whether a plan other that the optimizer's choice was better
    if chosenPlan in plan_ids:
        # take the best of the execution times (optimizer choice and the same plan forced)
        # and use the larger of the standard deviations
        defaultExeTime = 1E6
        defaultStdDev = 0.0
        if (paramValue, OPTIMIZER_DEFAULT_PLAN) in execDict:
            defaultExeTime, defaultStdDev = execDict[(paramValue, OPTIMIZER_DEFAULT_PLAN)]

        if (paramValue, chosenPlan) in execDict:
            forcedExeTime, forcedStdDev = execDict[(paramValue, chosenPlan)]
            defaultExeTime = min(defaultExeTime, forcedExeTime)
            defaultStdDev = max(defaultStdDev, forcedStdDev)

        for pl in plan_ids:
            if (paramValue, pl) in execDict:
                altExeTime, altStdDev = execDict[(paramValue, pl)]

                # The execution times tend to be fairly unreliable. Try to avoid false positives by
                # requiring a significantly better alternative, measured in standard deviations.
                if altExeTime + glob_sigma_diff * max(defaultStdDev, altStdDev) < defaultExeTime:
                    optimizerError = 100.0 * (defaultExeTime - altExeTime) / defaultExeTime
                    # yes, plan pl is significantly better than the optimizer default choice
                    return (pl, round(optimizerError,1))

    # the optimizer chose the right plan (at least we have not enough evidence to the contrary)
    return ("", 0.0)


# print the results of one test run

def print_results(testTitle, explainDict, execDict, errMessages, plan_ids):
    # print out the title of the test
    print("")
    print(testTitle)
    print("")
    exeTimes = len(execDict) > 0

    # make a list of plan ids with the default plan ids as first entry
    plan_ids_with_default = [ OPTIMIZER_DEFAULT_PLAN ]
    plan_ids_with_default.extend(plan_ids)

    # print a header row
    headerList = [ "Parameter value", "Plan chosen by optimizer", "Cost" ]
    for p_id in plan_ids:
        headerList.append("Cost of forced %s plan" % p_id)
    if exeTimes:
        headerList.append("Best execution plan")
        headerList.append("Optimization error (pct)")
        headerList.append("Execution time for default plan (ms)")
        for p_id in plan_ids:
            headerList.append("Execution time for forced %s plan (ms)" % p_id)
        headerList.append("std dev default")
        for p_id in plan_ids:
            headerList.append("std dev %s" % p_id)
    print(", ".join(headerList))

    # sort the keys of the dictionary by parameter value
    sorted_params = sorted(explainDict.keys())

    # for each parameter value, print one line with comma-separated values
    for p_val in sorted_params:
        # add the explain-related values
        vals = explainDict[p_val]
        resultList = [ str(p_val) ]
        for v in vals:
            resultList.append(str(v))
        # add the execution-related values, if applicable
        if exeTimes:
            # calculate the optimizer error
            bestPlan, optimizerError = checkForOptimizerErrors(p_val, vals[0], plan_ids, execDict)
            resultList.append(bestPlan)
            resultList.append(str(optimizerError))

            stddevList = []
            # our execution times will be a list of 2* (p+1) items,
            # (default exe time, forced exe time plan 1 ... p, stddev for default time, stddevs for plans 1...p)

            # now loop over the list of p+1 plan ids
            for plan_id in plan_ids_with_default:
                if (p_val, plan_id) in execDict:
                    # we did execute the query for this, append the avg time
                    # right away and save the standard deviation for later
                    mean, stddev = execDict[(p_val, plan_id)]
                    resultList.append(str(mean))
                    stddevList.append(str(stddev))
                else:
                    # we didn't execute this query, add blank values
                    resultList.append("")
                    stddevList.append("")

            # now add the standard deviations to the end of resultList
            resultList.extend(stddevList)

        # print a comma-separated list of result values (CSV)
        print(", ".join(resultList))

    # if there are any errors, print them at the end, leaving an empty line between the result and the errors
    if (len(errMessages) > 0):
        print("")
        print("%d diagnostic message(s):" % len(errMessages))
        for e in errMessages:
            print(e)


# execute a query n times, with a guard against long-running queries,
# and record the result in execDict and any errors in errMessages

def timed_execute_and_check_timeout(conn, sqlString, execute_n_times, paramValue, plan_id, execDict, timedOutDict, errMessages):
    # timedOutDict contains a record of queries that have previously timed out:
    # plan_id -> (lowest param value for timeout, highest value for timeout, direction)
    # right now we ignore low/high values and direction (whether the execution increases or decreases with
    # increased parameter values)
    if plan_id in timedOutDict:
        # this plan has timed out with at least one parameter value, decide what to do
        paramValLow, paramValHigh, direction = timedOutDict[plan_id]
        # for now, just return, once we time out for a plan we give up
        log_output("Not executing the %s plan for paramValue %d, due to previous timeout" % (plan_id, paramValue))
        return

    # execute the query
    mean, stddev, num_execs = timed_execute_n_times(conn, sqlString, execute_n_times)

    # record the execution stats
    execDict[(paramValue, plan_id)] = (mean, stddev)

    # check for timeouts
    if num_execs < execute_n_times or mean > glob_exe_timeout:
        # record the timeout, without worrying about low/high values or directions for now
        timedOutDict[plan_id] = (paramValue, paramValue, "unknown_direction")
        errMessages.append("The %s plan for parameter value %d took more than the allowed timeout, it was executed only %d time(s)" %
                           (plan_id, paramValue, num_execs))


# Definition of various test suites
# -----------------------------------------------------------------------------

# How to add a test:
#
# - Define some queries to run as text constants below. Use the tables
#   created by this program or add more tables to be created.
# - Define methods that parameterize these test queries, given an integer
#   parameter value in a range that you can define later.
# - Use the predefined types of plans (TABLE_SCAN, INDEX_SCAN) or add your
#   own plan types above. Note that you will also need to change or implement
#   an explain method that takes a query, explains it, and returns the plan
#   type and the estimated cost.
# - Define methods to force the desired plan types and also a method to reset
#   the connection so it doesn't force any of these plans.
# - Now you are ready to add another test, using method run_bitmap_index_tests()
#   as an example.
# - Add your test as a choice for the "tests" command line argument and add a
#   call to your test to the main program

# SQL test queries
# -----------------------------------------------------------------------------

# ------------  SQL test queries - bitmap index scan --------------

# GUC set statements

_reset_index_scan_forces = [ """
SELECT enable_xform('CXformImplementBitmapTableGet');
""",
                           """
SELECT enable_xform('CXformGet2TableScan');
""" ]

_force_sequential_scan = [ """
SELECT disable_xform('CXformImplementBitmapTableGet');
""" ]

_force_index_scan = [ """
SELECT disable_xform('CXformGet2TableScan');
""" ]

_reset_index_join_forces = [ """
SELECT enable_xform('CXformPushGbBelowJoin');
""",
                     """
RESET optimizer_enable_indexjoin;
""",
                     """
RESET optimizer_enable_hashjoin;
""" ]

_force_hash_join = [ """
SELECT disable_xform('CXformPushGbBelowJoin');
""",
                     """
SET optimizer_enable_indexjoin to off;
""" ]

_force_index_nlj = [ """
SELECT disable_xform('CXformPushGbBelowJoin');
""",
                     """
SET optimizer_enable_hashjoin to off;
""" ]

# setup statements

_insert_into_bfv_tables = """
TRUNCATE cal_bfvtest;
TRUNCATE cal_bfv_dim;
INSERT INTO cal_bfvtest SELECT col1, col1, col1 FROM (SELECT generate_series(1,%d) col1)a;
INSERT INTO cal_bfv_dim SELECT col1, col1 FROM (SELECT generate_series(1,%d,3) col1)a;
ANALYZE cal_bfvtest;
ANALYZE cal_bfv_dim;
"""

_insert_into_ndv_tables= """
TRUNCATE cal_ndvtest;
INSERT INTO cal_ndvtest SELECT i, i %% %d FROM (SELECT generate_series(1,1000000) i)a;
ANALYZE cal_ndvtest;
"""

# query statements

_bitmap_select_10_pct = """
SELECT count(*) %s
FROM cal_txtest
WHERE bitmap10 BETWEEN 0 AND %d;
"""

_bitmap_select_1_pct = """
SELECT count(*) %s
FROM cal_txtest
WHERE bitmap100 BETWEEN 0 AND %d;
"""

_bitmap_select_pt1_pct = """
SELECT count(*) %s
FROM cal_txtest
WHERE bitmap1000 BETWEEN 0 AND %d;
"""

_bitmap_select_pt01_pct = """
SELECT count(*) %s
FROM cal_txtest
WHERE bitmap10000 BETWEEN 0 AND %d;
"""

_bitmap_select_pt01_pct_multi = """
SELECT count(*) %s
FROM cal_txtest
WHERE bitmap10000 = 0 OR bitmap10000 BETWEEN 2 AND %d+2;
"""

_bitmap_index_join = """
SELECT count(*) %s
FROM cal_txtest f JOIN cal_dim d ON f.bitmap10000 = d.dim_id
WHERE d.dim_id2 BETWEEN 0 AND %d;
"""

_bfv_join = """
SELECT count(*) 
FROM cal_bfvtest ft, cal_bfv_dim dt1
WHERE ft.id = dt1.id;
"""

_bitmap_index_ndv = """
SELECT count(*)
FROM cal_ndvtest
WHERE val <= 1000000;
"""

# Parameterize methods for the test queries above
# -----------------------------------------------------------------------------

# bitmap index scan with 0...100 % of values, for parameter values 0...10, in 10 % increments
def parameterize_bitmap_index_10_narrow(paramValue):
    return _bitmap_select_10_pct % ("", paramValue)

def parameterize_bitmap_index_10_wide(paramValue):
    return _bitmap_select_10_pct % (", max(txt)", paramValue)

# bitmap index scan with 0...100 % of values, for parameter values 0...10,000, in .01 % increments
def parameterize_bitmap_index_10000_narrow(paramValue):
    return _bitmap_select_pt01_pct % ("", paramValue)

def parameterize_bitmap_index_10000_wide(paramValue):
    return _bitmap_select_pt01_pct % (", max(txt)", paramValue)

# bitmap index scan with 0...100 % of values, for parameter values 0...10,000, in .01 % increments, multiple ranges
def parameterize_bitmap_index_10000_multi_narrow(paramValue):
    return _bitmap_select_pt01_pct_multi % ("", paramValue)

def parameterize_bitmap_index_10000_multi_wide(paramValue):
    return _bitmap_select_pt01_pct_multi % (", max(txt)", paramValue)

# index join with 0...100 % of fact values, for parameter values 0...10,000, in .01 % increments
def parameterize_bitmap_join_narrow(paramValue):
    return _bitmap_index_join % ("", paramValue)

def parameterize_bitmap_join_wide(paramValue):
    return _bitmap_index_join % (", max(f.txt)", paramValue)

def parameterize_insert_join_bfv(paramValue):
    return _insert_into_bfv_tables % (paramValue, paramValue)

def parameterize_insert_ndv(paramValue):
    return _insert_into_ndv_tables % (paramValue)

def parameterize_bitmap_join_bfv(paramValue):
    return _bfv_join

def parameterize_bitmap_index_ndv(paramValue):
    return _bitmap_index_ndv

def noSetupRequired(paramValue):
    return "SELECT 1;"

def explain_bitmap_index(conn, sqlStr):
    return explain_index_scan(conn, sqlStr)

def reset_index_test(conn):
    execute_sql_arr(conn, _reset_index_scan_forces)

def force_table_scan(conn):
    execute_sql_arr(conn, _force_sequential_scan)

def force_bitmap_scan(conn):
    execute_sql_arr(conn, _force_index_scan)

def reset_index_join(conn):
    execute_sql_arr(conn, _reset_index_join_forces)

def force_hash_join(conn):
    execute_sql_arr(conn, _force_hash_join)

def force_index_join(conn):
    execute_sql_arr(conn, _force_index_nlj)


# Helper methods for running tests
# -----------------------------------------------------------------------------

def run_one_bitmap_scan_test(conn, testTitle, paramValueLow, paramValueHigh, setup, parameterizeMethod, execute_n_times):
    plan_ids = [ BITMAP_SCAN, TABLE_SCAN ]
    force_methods = [ force_bitmap_scan, force_table_scan ]
    explainDict, execDict, errors = find_crossover(conn, paramValueLow, paramValueHigh, setup, parameterizeMethod, explain_bitmap_index, reset_index_test, plan_ids, force_methods, execute_n_times)
    print_results(testTitle, explainDict, execDict, errors, plan_ids)

def run_one_bitmap_join_test(conn, testTitle, paramValueLow, paramValueHigh, setup, parameterizeMethod, execute_n_times):
    plan_ids = [ BITMAP_SCAN, TABLE_SCAN ]
    force_methods = [ force_index_join, force_hash_join ]
    explainDict, execDict, errors = find_crossover(conn, paramValueLow, paramValueHigh, setup, parameterizeMethod, explain_join_scan, reset_index_join, plan_ids, force_methods, execute_n_times)
    print_results(testTitle, explainDict, execDict, errors, plan_ids)


# Main driver for the tests
# -----------------------------------------------------------------------------

def run_bitmap_index_scan_tests(conn, execute_n_times):

    run_one_bitmap_scan_test(conn,
                             "Bitmap Scan Test, NDV=10, selectivity_pct=10*parameter_value, count(*)",
                             0,
                             10,
                             noSetupRequired,
                             parameterize_bitmap_index_10_narrow,
                             execute_n_times)

    # all full table scan, no crossover
    run_one_bitmap_scan_test(conn,
                             "Bitmap Scan Test, NDV=10, selectivity_pct=10*parameter_value, max(txt)",
                             0,
                             3,
                             noSetupRequired,
                             parameterize_bitmap_index_10_wide,
                             execute_n_times)

    run_one_bitmap_scan_test(conn,
                             "Bitmap Scan Test, NDV=10000, selectivity_pct=0.01*parameter_value, count(*)",
                             0,
                             20,
                             noSetupRequired,
                             parameterize_bitmap_index_10000_narrow,
                             execute_n_times)

    run_one_bitmap_scan_test(conn,
                             "Bitmap Scan Test, NDV=10000, selectivity_pct=0.01*parameter_value, count(*), largeNDV test",
                             0,
                             300,
                             noSetupRequired,
                             parameterize_bitmap_index_10000_narrow,
                             execute_n_times)

    run_one_bitmap_scan_test(conn,
                             "Bitmap Scan Test, NDV=10000, selectivity_pct=0.01*parameter_value, max(txt)",
                             5,
                             25,
                             noSetupRequired,
                             parameterize_bitmap_index_10000_wide,
                             execute_n_times)

    run_one_bitmap_scan_test(conn,
                             "Bitmap Scan Test, multi-range, NDV=10000, selectivity_pct=0.01*parameter_value, count(*)",
                             0,
                             100,
                             noSetupRequired,
                             parameterize_bitmap_index_10000_multi_narrow,
                             execute_n_times)

    run_one_bitmap_scan_test(conn,
                             "Bitmap Scan Test, multi-range, NDV=10000, selectivity_pct=0.01*parameter_value, max(txt)",
                             0,
                             60,
                             noSetupRequired,
                             parameterize_bitmap_index_10000_multi_wide,
                             execute_n_times)

    run_one_bitmap_scan_test(conn,
                             "Bitmap Scan Test, ndv test, rows=1000000, parameter = insert statement modulo, count(*)",
                             1, # modulo ex. would replace x in the following: SELECT i % x FROM generate_series(1,10000)i;
                             10000, #max here is 10000 (num of rows)
                             parameterize_insert_ndv,
                             parameterize_bitmap_index_ndv,
                             execute_n_times)

def run_bitmap_index_join_tests(conn, execute_n_times):

    run_one_bitmap_join_test(conn,
                             "Bitmap Join Test, NDV=10000, selectivity_pct=0.01*parameter_value, count(*)",
                             0,
                             900,
                             noSetupRequired,
                             parameterize_bitmap_join_narrow,
                             execute_n_times)
    
    run_one_bitmap_join_test(conn,
                             "Bitmap Join Test, NDV=10000, selectivity_pct=0.01*parameter_value, max(txt)",
                             0,
                             900,
                             noSetupRequired,
                             parameterize_bitmap_join_wide,
                             execute_n_times)

    run_one_bitmap_join_test(conn,
                             "Bitmap Join BFV Test, Large Data, parameter = num rows inserted",
                             10000, # num of rows inserted
                             900000,
                             parameterize_insert_join_bfv,
                             parameterize_bitmap_join_bfv,
                             execute_n_times)
    

# common parts of all test suites, create tables, run tests, drop objects
# -----------------------------------------------------------------------------

# create the table(s), as regular or AO table, and insert num_rows into the main table
def createDB(conn, use_ao, num_rows):
    create_options = ""
    if use_ao:
        create_options = _with_appendonly
    create_cal_table_stmt = _create_cal_table % create_options
    create_bfv_table = _create_bfv_table % create_options
    create_ndv_table = _create_ndv_table % create_options
    insert_into_temp_stmt = _insert_into_temp % (1,num_rows)
    insert_into_other_stmt = _insert_into_other_tables % (1,10000)
        
    execute_sql(conn, _drop_tables)
    execute_sql(conn, create_cal_table_stmt)
    execute_sql(conn, create_bfv_table)
    execute_sql(conn, create_ndv_table)
    execute_sql_arr(conn, _create_other_tables)
    commit_db(conn)
    execute_and_commit_sql(conn, insert_into_temp_stmt)
    execute_and_commit_sql(conn, _insert_into_table)
    execute_and_commit_sql(conn, insert_into_other_stmt)
    execute_sql_arr(conn, _create_index_arr)
    execute_sql_arr(conn, _create_bfv_index_arr)
    execute_sql_arr(conn, _create_ndv_index_arr)
    if use_ao:
      execute_sql_arr(conn, _create_btree_indexes_ao_arr)
    execute_sql(conn, _analyze_table)
    commit_db(conn)

def dropDB(conn):
     execute_sql(conn, _drop_tables)

# ensure that we have perfect histogram statistics on the relevant columns
def smoothStatistics(conn):
    if glob_gpdb_major_version > 5:
        execute_sql(conn, _allow_system_mods)
    else:
        execute_sql(conn, _allow_system_mods_v5)
    execute_sql_arr(conn, _fix_statistics)
    commit_db(conn)


def main():
    global glob_verbose
    global glob_log_file
    args, parser = parseargs()
    if args.logFile != "":
        glob_log_file = open(args.logFile, "wt", 1)
    if args.verbose:
        glob_verbose = True
    log_output("Connecting to host %s on port %d, database %s" % (args.host, args.port, args.dbName))
    conn = connect(args.host, args.port, args.dbName)
    select_version(conn)
    if args.create:
        createDB(conn, args.appendOnly, args.numRows)
        smoothStatistics(conn)
    for test_unit in args.tests:
        if test_unit == "all":
            run_bitmap_index_scan_tests(conn, args.execute)
            run_bitmap_index_join_tests(conn, args.execute)
        elif test_unit == "bitmap_scan_tests":
            run_bitmap_index_scan_tests(conn, args.execute)
        elif test_unit == "bitmap_join_tests":
            run_bitmap_index_join_tests(conn, args.execute)
        elif test_unit == "none":
            print("Skipping tests")
            
    if args.drop:
        dropDB(conn)
    if glob_log_file != None:
        glob_log_file.close()
    
if __name__ == "__main__":
    main()
