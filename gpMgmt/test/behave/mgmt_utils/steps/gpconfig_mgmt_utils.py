from os import path
try:
    import subprocess32 as subprocess
except:
    import subprocess
from gppylib.db import dbconn
from gppylib.gparray import GpArray

from behave import given, when, then
from test.behave_utils.utils import *

from mgmt_utils import *


# This class is intended to store per-Scenario state that is built up over
# a series of steps.
class GpConfigContext:
    def __init__(self):
        self.master_postgres_file = ''
        self.standby_postgres_file = ''

@given('the gpconfig context is setup')
def impl(context):
    make_temp_dir(context, path.join('/tmp', 'gpconfig'))
    temp_base_dir = context.temp_base_dir
    context.gpconfig_context.working_directory = temp_base_dir
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    segments = gparray.getDbList()
    restore_commands = []
    for segment in segments:
        segment_tmp_directory = path.join(temp_base_dir, str(segment.dbid))
        os.mkdir(segment_tmp_directory)
        backup_path = path.join(segment_tmp_directory, 'postgresql.conf')
        original_path = path.join(segment.datadir, 'postgresql.conf')
        copy_command = ('scp %s:%s %s' % (segment.hostname, original_path, backup_path)).split(' ')
        restore_command = ('scp %s %s:%s' % (backup_path, segment.hostname, original_path)).split(' ')
        restore_commands.append(restore_command)

        subprocess.check_call(copy_command)

        if segment.content == -1:
            if segment.role == 'p':
                context.gpconfig_context.master_postgres_file = original_path
            else:
                context.gpconfig_context.standby_postgres_file = original_path

    def delete_temp_directory():
        if 'temp_base_dir' in context:
            shutil.rmtree(context.temp_base_dir)

    def restore_conf_files():
        for cmd in restore_commands:
            subprocess.check_call(cmd)

    context.add_cleanup(delete_temp_directory)
    context.add_cleanup(restore_conf_files)


@given('the user runs gpconfig sets guc "{guc}" with "{value}"')
def impl(context, guc, value):
    cmd = 'gpconfig -c %s -v %s' % (guc, value)
    context.execute_steps(u'''
        Given the user runs "%s"
        Then gpconfig should return a return code of 0
    ''' % cmd)

# FIXME: this assumes the standby host is the same as the master host
#  This is currently true for our demo_cluster and concourse_cluster
@when('the user writes "{guc}" as "{value}" to the master config file')
def impl(context, guc, value):
    if context.gpconfig_context.master_postgres_file:
        with open(context.gpconfig_context.master_postgres_file, 'a') as fd:
            fd.write("%s=%s\n" % (guc, value))
            fd.flush()
    if context.gpconfig_context.standby_postgres_file:
        with open(context.gpconfig_context.standby_postgres_file, 'a') as fd:
            fd.write("%s=%s\n" % (guc, value))
            fd.flush()
