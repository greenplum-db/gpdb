#!/usr/local/bin/python

import sys
import subprocess
import re
import argparse
import os

_help = """
Gets the values of debug counters collected with the code in the
CDebugCounter class, see file ../libgpos/include/gpos/common/CDebugCounter.h
"""

# Description:
#
# We start by getting relevant log file lines using grep, looking for the pattern
# "DebugCounterEvent".
#
# The log file is either specified by the user, or we grep the log files
# $MASTER_DATA_DIRECTORY/log/*.csv
#
# The log may contain multiple "runs". A run is a series of log entries made by
# the same process. Note that this tool doesn't support concurrent processes logging.
#
# Finally, for each run, we massage the log file lines into a CSV format by removing
# leading and trailing elements. The result is a CSV file printed to stdout.

try:
	from gppylib.db import dbconn
except ImportError, e:
	sys.exit('ERROR: Cannot import modules.  Please check that you have sourced greenplum_path.sh to set PYTHONPATH. '
			 'Detail: ' + str(e))

glob_use_sql = False
glob_sql_drop = False
glob_conn = None
glob_sql_inserted_rows = 0

# SQL queries
# -----------------------------------------------------------------------------

glob_drop_table = """
drop table if exists debug_counters
"""

glob_set_search_path = """
set search_path to public
"""

glob_create_table = """
create table if not exists debug_counters
   (run int,
    query_number int,
    query_name text,
    counter_name text,
    counter_type text,
    counter_value double precision)
distributed by (query_number, counter_name)
"""

glob_insert = """
insert into debug_counters values(%s, %s, '%s', '%s', '%s', %s)
"""


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


def execute_sql(conn, sqlStr):
	try:
		dbconn.execSQL(conn, sqlStr)
	except Exception as e:
		print("")
		print("Error executing query: %s; Reason: %s" % (sqlStr, e))
		dbconn.execSQL(conn, "abort")


def commit_db(conn):
	execute_sql(conn, "commit")


def print_or_insert_row(csv):
	global glob_sql_inserted_rows

	if glob_use_sql:
		# split the comma-separated values and trim away whitespace
		columns = [x.strip() for x in csv.split(',')]
		execute_sql(glob_conn, glob_insert % tuple(columns))
		glob_sql_inserted_rows = glob_sql_inserted_rows + 1
	else:
		print(csv)


def print_or_insert_header_row(csv):
	if not glob_use_sql:
		print(csv)


# Methods related to reading and processing log files
# -----------------------------------------------------------------------------

def run_command(command):
	p = subprocess.Popen(command,
						 stdout=subprocess.PIPE,
						 stderr=subprocess.STDOUT)
	return iter(p.stdout.readline, b'')


def processLogFile(logFileLines, allruns):
	# every start marker indicates a new run, we usually print only the last such run,
	# unless allruns is set to true
	header_printed = False
	current_run_number = 0
	current_run_name = ""
	current_output = []

	for line in logFileLines:
		utf8_line = line.decode('utf-8')
		start_marker_match = re.search(r'CDebugCounterEventLoggingStart', utf8_line)
		counter_log_match = re.search(r'CDebugCounterEvent\(', utf8_line)
		eof_match = re.match(r'EOF', utf8_line)

		if start_marker_match or eof_match:
			if len(current_run_name) > 0:
				# we are at the end of a run (and possibly the beginning of a new one)
				# if allruns is True then print this run, otherwise print it only when we are at EOF
				if allruns or eof_match:
					if not header_printed:
						# print this header only once
						print_or_insert_header_row("run, query num, query name, counter name, counter_type, counter_value")
						header_printed = True
					# print a dummy counter line that indicates the starting time of the run
					print_or_insert_row("%d, 0, , %s, run_start_time, 0" % (current_run_number, current_run_name))
					# now print the actual counter values
					for l in current_output:
						print_or_insert_row(l)

			# prepare for the next run by cleaning out our output list, this happens whether
			# we printed the previous run or whether we ignored it
			current_output = []

			if (not allruns):
				# we are only interested in the last run, discard
				# all information about previous runs
				current_run_number = 0

			# assign a number and a name to a run that may be following
			current_run_number += 1
			# the run name is the starting timestamp
			current_run_name = re.sub(r'.*:([0-9]+-[^,]+),.*\n',
									  r'\1',
									  utf8_line)
		elif counter_log_match:
			# we encountered the log entry for one event, remove prefix and suffix and
			# extract the comma-separated values in-between
			csv = re.sub(r'.*CDebugCounterEvent\(.*\)\s*,\s*([^"]+).*\n',
						 r'\1',
						 utf8_line)
			# prepend the run number to the comma-separated values
			csv = "%d, %s" % (current_run_number, csv)
			current_output.append(csv)


def parseargs():
	parser = argparse.ArgumentParser(description=_help, version='1.0')

	parser.add_argument("--logFile", default="",
						help="GPDB log file saved from a run with debug event counters enabled (default is to search "
							 "GPDB master log directory)")
	parser.add_argument("--allRuns", action="store_true",
						help="Record all runs, instead of just the last one, use this if you had several psql runs")
	parser.add_argument("--sql", action="store_true",
						help="Instead of printing the results on stdout, record them in an SQL database")
	parser.add_argument("--host", default="localhost",
						help="Host to connect to (default is localhost).")
	parser.add_argument("--port", type=int, default="0",
						help="Port on the host to connect to")
	parser.add_argument("--dbName", default="",
						help="Database name to connect to")
	parser.add_argument("--recreateTable", action="store_true",
						help="Drop and recreate the debug_counters table, erasing any pre-existing data")
	parser.add_argument("--raw", action="store_true",
						help="Just print out the log data, to be used later as a log file for this script")

	parser.set_defaults(verbose=False, filters=[], slice=(None, None))

	# Parse the command line arguments
	args = parser.parse_args()
	return args, parser


def main():
	global glob_use_sql
	global glob_conn
	global glob_sql_drop

	args, parser = parseargs()

	logfile = args.logFile
	allruns = args.allRuns
	glob_use_sql = args.sql
	glob_sql_drop = args.recreateTable

	if glob_use_sql:
		glob_conn = connect(args.host, args.port, args.dbName)
		execute_sql(glob_conn, glob_set_search_path)

		if glob_sql_drop:
			execute_sql(glob_conn, glob_drop_table)
		execute_sql(glob_conn, glob_create_table)
		commit_db(glob_conn)

	grep_command = 'grep CDebugCounterEvent '
	gather_command = ['sh', '-c']

	if logfile is None or len(logfile) == 0:
		if 'MASTER_DATA_DIRECTORY' in os.environ:
			master_data_dir = os.environ['MASTER_DATA_DIRECTORY']
		else:
			print("$MASTER_DATA_DIRECTORY environment variable is not defined, exiting")
			exit()
		grep_command = grep_command + master_data_dir + '/log/*.csv'
	else:
		grep_command = grep_command + logfile

	grep_command = grep_command + "; echo EOF"

	gather_command.append(grep_command)

	all_lines = run_command(gather_command)

	if args.raw:
		# just print out the raw log data (already filtered through grep),
		# it can be saved an fed later as a log file to this program
		for l in all_lines:
			print(l)
	else:
		# regular processing of the log file
		processLogFile(all_lines, allruns)

		if glob_use_sql:
			commit_db(glob_conn)
			glob_conn.close()
			print("Inserted a total of %d rows into table public.debug_counters." % glob_sql_inserted_rows)

if __name__ == "__main__":
	main()
