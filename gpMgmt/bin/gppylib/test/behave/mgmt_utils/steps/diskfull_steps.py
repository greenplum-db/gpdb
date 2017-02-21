#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2016. All Rights Reserved.
#

"""
diskfull_steps.py:

Contains steps for testting warnings on diskfull feature.
"""


# import Python standard library modules
import glob
import os
import re


# import GPDB modules
try:
  from gppylib.db import dbconn
  from gppylib.gparray import GpArray
  from gppylib.logfilter import *

except ImportError, e:
  sys.exit("ERROR: Cannot import modules.  Please check that you have "
           "sourced greenplum_path.sh.  Detail: " + str(e))




#-------------------------------------------------------------------------------
#
# Utility Functions
#

def get_int_limit(limit_val):
  """
  Convert a string value to an integer and verify that it is between 0 and 100
  """

  try:
    limit = int(limit_val)

  except Exception as e:
    raise Exception("Limit value string is not valid %s" % limit_val)

  if (limit < 0) or (limit > 100):
    raise Exception("Limit is out of bound %d" % limit)

  return limit



def get_diskusage_soft_limit_from_output(context):
  """
  Get diskusage_soft_limit value from stdout
  """

  stdout = context.stdout_message
  lines = stdout.splitlines()

  if "Master  value: " in lines[2]:
    master_value_txt, delim, limit_val = lines[2].partition(':')
    limit_val = limit_val.strip()
    limit = get_int_limit(limit_val)
    return limit

  raise Exception("diskusage_soft_limit not found %s" % stdout)



def get_seg_data_dir(context):
  """
  Get path to the data directory of the first database of the first segment.
  """

  gparray = GpArray.initFromCatalog(dbconn.DbURL())
  return gparray.segments[0].get_dbs()[0].getSegmentDataDirectory()



def set_soft_limit(context, limit):
  """
  set the gp_diskusage_soft_limit GUC.
  """

  command = "gpconfig -c gp_diskusage_soft_limit -v %d" % limit
  run_gpcommand(context, command)

  command = "gpstop -u"
  run_gpcommand(context, command)



def get_log_entry(log_file, timestamp, pattern):
  """
  Get the first matching log entry after timestamp.
  """

  with open(log_file, "rU") as f:
    warning_entries = FilterLogEntries(f, beginstamp = timestamp,
                                       include = pattern)
    for entry in warning_entries:
      return entry

    return None




#-------------------------------------------------------------------------------
#
# Behave Rules
#

@given("gp_diskusage_soft_limit is stored")
def impl(context):
  """
  Store the current value of the gp_diskusage_soft_limit.
  """

  context.diskusage_soft_limit = get_diskusage_soft_limit_from_output(context)



@then("the gp_diskusage_soft_limit is reset to the original value")
def impl(context):
  """
  Restore gp_diskusage_soft_limit to its stored value.
  """

  set_soft_limit(context, context.diskusage_soft_limit)



@given("the user sets gp_diskusage_soft_limit to a very low value")
def impl(context):
  """
  Set gp_diskusage_soft_limit to 1%.
  """

  context.timestamp = datetime.now()
  set_soft_limit(context, 1)



@then("the log file has the soft limit exceeded warning")
def impl(context):
  """
  Verfiy the soft limit exceeded warning in the segment log file.

  Check for the warning message in the segment log file after the GUC
  was set.

  If no match is found, wait for 10 seconds and retry.  After 7
  retries, assume that the warning was not issued.  This makes sure
  that we wait long enought for the FTS probe interval of 60 seconds.
  """

  seg_data_dir = get_seg_data_dir(context)
  seg_log_dir = os.path.join(seg_data_dir, "pg_log")
  seg_log_filenames = os.path.join(seg_log_dir, "*.csv")
  latest_seg_log_file = max(glob.iglob(seg_log_filenames))

  for _ in range(7):
    entry = get_log_entry(latest_seg_log_file, context.timestamp,
                      "SoftLimit of 1% crossed for directory")
    if entry:
      return

    time.sleep(10)

  raise Exception("SoftLimit exceeded warning not issued")
