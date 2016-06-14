# coding: utf-8 

import os
import socket
import gzip
from gppylib.commands.base import Command, REMOTE, WorkerPool, CommandResult
from gppylib.db import dbconn
from gppylib.test.behave_utils.utils import getRows, validate_parse_email_file
from gppylib.gparray import GpArray
from gppylib.operations.unix import CheckFile
from gppylib.test.behave_utils.utils import backup_data_to_file, check_table_exists, validate_restore_data_in_file

master_data_dir = os.environ.get('MASTER_DATA_DIRECTORY')

comment_start_expr = '-- '
comment_expr = '-- Name: '
comment_data_expr_a = '-- Data: '
comment_data_expr_b = '-- Data for Name: '
len_start_comment_expr = len(comment_start_expr)

@given('the user locks "{table_name}" in "{lock_mode}" using connection "{conn}" on "{dbname}"')
@when('the user locks "{table_name}" in "{lock_mode}" using connection "{conn}" on "{dbname}"')
@then('the user locks "{table_name}" in "{lock_mode}" using connection "{conn}" on "{dbname}"')
def impl(context, table_name, lock_mode, conn, dbname):
    query = "begin; lock table %s in %s" % (table_name, lock_mode)
    conn = dbconn.connect(dbconn.DbURL(dbname=dbname)) # todo not truthful about using conn parameter
    dbconn.execSQL(conn, query) 
    context.conn = conn

@when('the user runs the query "{query}" in database "{dbname}" in a worker pool "{poolname}" as soon as pg_class is locked')
@then('the user runs the query "{query}" in database "{dbname}" in a worker pool "{poolname}" as soon as pg_class is locked')
def impl(context, query, dbname, poolname):
    pool = WorkerPool(numWorkers=1)
    cmd = on_unlock(query,dbname)
    pool.addCommand(cmd)
    if not hasattr(context, 'pool'):
        context.pool = {}
    context.pool[poolname] = pool
    context.cmd = cmd

@when('the user runs the "{cmd}" in a worker pool "{poolname}"')
@then('the user runs the "{cmd}" in a worker pool "{poolname}"')
def impl(context, cmd, poolname):
    command = Command(name='run gpcrondump in a separate thread', cmdStr=cmd)
    pool = WorkerPool(numWorkers=1)
    pool.addCommand(command)
    if not hasattr(context, 'pool'):
        context.pool = {}
    context.pool[poolname] = pool
    context.cmd = cmd
 
class on_unlock(Command):
    def __init__(self, query, dbname):
        self.dbname = dbname
        self.query = query
        self.result = 1
        self.completed = False
        self.halt = False
        Command.__init__(self, 'on unlock', 'on unlock', ctxt=None, remoteHost=None)

    def get_results(self):
        return CommandResult(self.result, '', '', self.completed, self.halt)

    def run(self):
        while check_pg_class_lock(self.dbname) != 1:
            pass
        with dbconn.connect(dbconn.DbURL(dbname=self.dbname)) as conn:
            dbconn.execSQL(conn, self.query)
        self.result = 0
        self.completed = True
        self.halt = False

def check_pg_class_lock(dbname):
   seg_count = 1

   query = """select count(*)
            from pg_locks
            where relation in (select oid from pg_class where relname='pg_class')
                  and locktype='relation' and mode='ExclusiveLock'"""
   row_count = getRows(dbname, query)[0][0]
   return row_count

@given('the "{backup_pg}" has a lock on the pg_class table in "{dbname}"')
@when('the "{backup_pg}" has a lock on the pg_class table in "{dbname}"')
@then('the "{backup_pg}" has a lock on the pg_class table in "{dbname}"')
def impl(context, dbname, backup_pg):
    seg_count = 1
    timeout = 2
    while timeout > 0:
        row_count = check_pg_class_lock(dbname)
        time.sleep(1)
        timeout -= 1
    if row_count != seg_count:
        raise Exception("Incorrect (number of) lock/locks on pg_class, expected count = %s, received count = %s" % (seg_count, row_count))

@then('the worker pool "{poolname}" is cleaned up')
@when('the worker pool "{poolname}" is cleaned up')
def impl(context, poolname):
    pool = context.pool[poolname]
    if pool:
        pool.join()
        for c in pool.getCompletedItems():
            result = c.get_results()
            context.ret_code = result.rc
            context.stdout_message = result.stdout
            context.error_message = result.stderr
        pool.haltWork()
        pool.joinWorkers()
    else:
        raise Exception('Worker pool is None.Probably behave step to initialize the worker pool is missing.')

@given('the user drops "{tablename}" in "{dbname}" in a worker pool "{poolname}"')
@then('the user drops "{tablename}" in "{dbname}" in a worker pool "{poolname}"')
@when('the user drops "{tablename}" in "{dbname}" in a worker pool "{poolname}"')
def impl(context, tablename, dbname, poolname):
    pool = WorkerPool(numWorkers=1)
    cmd = Command(name='drop a table in a worker pool', cmdStr="""psql -c "DROP TABLE %s" -d %s""" % (tablename, dbname))
    pool.addCommand(cmd)
    if not hasattr(context, 'pool'):
        context.pool = {}
    context.pool[poolname] = pool
    

@given('the user closes the connection "{conn_name}"')
@when('the user closes the connection "{conn_name}"')
@then('the user closes the connection "{conn_name}"')
def impl(context, conn_name):
    query = """ROLLBACK;"""
    dbconn.execSQL(context.conn, query) 
    context.conn.close()

@given('verify that "{backup_pg}" has no lock on the pg_class table in "{dbname}"')
@when('verify that "{backup_pg}" has no lock on the pg_class table in "{dbname}"')
@then('verify that "{backup_pg}" has no lock on the pg_class table in "{dbname}"')
def impl(context, backup_pg, dbname):
    query = """select count(*) 
             from pg_locks 
             where relation in (select oid from pg_class where relname='pg_class') 
                   and locktype='relation' and mode='ExclusiveLock'"""

    row_count = getRows(dbname, query)[0][0]
    if row_count != 0:
        raise Exception("Found a ExclusiveLock on pg_class")

@given('there is a "{tabletype}" table "{table_name}" with compression "{compression_type}" in "{dbname}" with data and {rowcount} rows')
@when('there is a "{tabletype}" table "{table_name}" with compression "{compression_type}" in "{dbname}" with data and {rowcount} rows')
@then('there is a "{tabletype}" table "{table_name}" with compression "{compression_type}" in "{dbname}" with data and {rowcount} rows')
def impl(context, tabletype, table_name, compression_type, dbname, rowcount):
    populate_regular_table_data(context, tabletype, table_name, compression_type, dbname, int(rowcount))

@given('verify the metadata dump file syntax under "{directory}" for comments and types')
@when('verify the metadata dump file syntax under "{directory}" for comments and types')
@then('verify the metadata dump file syntax under "{directory}" for comments and types')
def impl(context, directory):
    names = ["Name", "Data", "Data for Name"]
    types = ["TABLE", "TABLE DATA", "EXTERNAL TABLE", "ACL", "CONSTRAINT", "COMMENT", "PROCEDURAL LANGUAGE", "SCHEMA", "AOSTORAGEOPTS"]
    master_dump_dir = directory if len(directory.strip()) != 0 else master_data_dir
    metadata_path = __get_dump_metadata_path(context, master_dump_dir)

    # gzip in python 2.6 does not support __exit__, so it cannot be used in "with"
    # with gzip.open(metadata_path, 'r') as fd:

    fd = None
    try:
        fd = gzip.open(metadata_path, 'r')
        line = None
        for line in fd:
            if (line[:3] == comment_start_expr):
                if (line.startswith(comment_expr) or line.startswith(comment_data_expr_a) or line.startswith(comment_data_expr_b)):
                    name_k, type_k, schema_k = get_comment_keys(line)
                    if (name_k not in names and type_k != "Type" and schema_k != "Schema"):
                        raise Exception("Unknown key in the comment line of the metdata_file '%s'. Please check and confirm if the key is correct" % (metadata_file))
                    name_v, type_v, schema_v = get_comment_values(line)
                    if (type_v not in types):
                        raise Exception("Value of Type in the comment line '%s' of the metadata_file '%s' does not fall under the expected list %s. Please check if the value is correct" %(type_v, metadata_file, types))
        if not line:
            raise Exception('Metadata file has no data')
    finally:
        if fd:
            fd.close()

@given('verify the metadata dump file does not contain "{target}"')
@when('verify the metadata dump file does not contain "{target}"')
@then('verify the metadata dump file does not contain "{target}"')
def impl(context, target):
    metadata_path = __get_dump_metadata_path(context, master_data_dir)

    fd = None
    try:
        fd = gzip.open(metadata_path, 'r')
        line = None
        for line in fd:
            if target in line:
                raise Exception("Unexpectedly found %s in metadata file %s" % (target, metadata_path))
        if not line:
            raise Exception('Metadata file has no data')
    finally:
        if fd:
            fd.close()

@given('verify the metadata dump file does contain "{target}"')
@when('verify the metadata dump file does contain "{target}"')
@then('verify the metadata dump file does contain "{target}"')
def impl(context, target):
    metadata_path = __get_dump_metadata_path(context, master_data_dir)

    fd = None
    try:
        fd = gzip.open(metadata_path, 'r')
        line = None
        for line in fd:
            if target in line:
                return
        if not line:
            raise Exception('Metadata file has no data')
        raise Exception("Missing text %s in metadata file %s" % (target, metadata_path))
    finally:
        if fd:
            fd.close()

def __get_dump_metadata_path(context, dump_dir):
    filename = "gp_dump_1_1_%s.gz" % context.backup_timestamp
    metadata_path = os.path.join(dump_dir, "db_dumps", context.backup_timestamp[0:8], filename)
    return metadata_path

def get_comment_keys(line):
    try:
        temp = line[len_start_comment_expr:]
        tokens = temp.strip().split(';')
        name = tokens[0].split(':')[0].strip()
        type = tokens[1].split(':')[0].strip()
        schema = tokens[2].split(':')[0].strip()
    except:
        return (None, None, None)
    return (name, type, schema) 

def get_comment_values(line):
    try:
        temp = line[len_start_comment_expr:]
        tokens = temp.strip().split(';')
        name = tokens[0].split(':')[1].strip()
        type = tokens[1].split(':')[1].strip()
        schema = tokens[2].split(':')[1].strip()
    except:
        return (None, None, None)
    return (name, type, schema) 

@given('{command} should print {out_msg} to stdout {num} times')
@when('{command} should print {out_msg} to stdout {num} times')
@then('{command} should print {out_msg} to stdout {num} times')
def impl(context, command, out_msg, num):
    msg_list = context.stdout_message.split('\n')
    msg_list = [x.strip() for x in msg_list]

    count = msg_list.count(out_msg)
    if count != int(num):
        raise Exception("Expected %s to occur %s times. Found %d" % (out_msg, num, count))

@given('verify that {filetype} file is generated in {dir}')
@when('verify that {filetype} file is generated in {dir}')
@then('verify that {filetype} file is generated in {dir}')
def impl(context, filetype, dir):
    if dir == 'master_data_directory':
        dir = master_data_dir
    if filetype == 'report':
        filename = '%s/gp_restore_%s.rpt' % (dir, context.backup_timestamp)
        if not os.path.isfile(filename):
            raise Exception('Report file %s is not present in master data directory' % filename)
    elif filetype == 'status':
        gparray = GpArray.initFromCatalog(dbconn.DbURL())
        if dir == 'segment_data_directory':
            primary_segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary()]
            for seg in primary_segs:
                host = seg.getSegmentHostName()
                seg_data_dir = seg.getSegmentDataDirectory()
                cmd = Command('check status file', "ls %s/gp_restore_status_*_%s" % (seg_data_dir, context.backup_timestamp), ctxt=REMOTE, remoteHost=host)
                cmd.run(validateAfter=True)
                results = cmd.get_results()
                if not results.stdout.strip():
                    raise Exception('Status file ending with timestamp %s is not present in segment %s data directory' % (context.backup_timestamp, host))
        else:
            count = 0
            primary_segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary()]
            for seg in primary_segs:
                host = seg.getSegmentHostName()
                cmd = Command('check status file', "ls %s/gp_restore_status_*_%s" % (dir, context.backup_timestamp), ctxt=REMOTE, remoteHost=host)
                cmd.run(validateAfter=True)
                results = cmd.get_results()
                if results.stdout.strip():
                    count += 1
                else:
                    raise Exception('Status file not found in segment: %s' % host)
            segs = len(primary_segs)
            if count != segs:
                raise Exception('Expected %d status file but found %d' % (segs, count))

@given('there are no {filetype} files in "{dir}"')
@when('there are no {filetype} files in "{dir}"')
@then('there are no {filetype} files in "{dir}"')
def impl(context, filetype, dir):
    if filetype == 'report':
        if dir == 'master_data_directory':
            dir = master_data_dir
        filenames = os.listdir(dir)
        for filename in filenames:
            if filename.startswith('gp_restore') and filename.endswith('.rpt'):
                filename = '%s/%s' % (dir, filename)
                os.remove(filename)
    if filetype == 'status':
        gparray = GpArray.initFromCatalog(dbconn.DbURL())
        if dir == 'segment_data_directory':
            primary_segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary()]
            for seg in primary_segs:
                host = seg.getSegmentHostName()
                seg_data_dir = seg.getSegmentDataDirectory()
                cmd = Command('remove status file', "rm -f %s/gp_restore_status_*" % (seg_data_dir), ctxt=REMOTE, remoteHost=host)
                cmd.run(validateAfter=True)
        else:
            primary_segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary()]
            for seg in primary_segs:
                host = seg.getSegmentHostName()
                cmd = Command('remove status file', "rm -f %s/gp_restore_status_*" % dir, ctxt=REMOTE, remoteHost=host)
                cmd.run(validateAfter=True)

@given('the mail_contacts file does not exist')
@then('the mail_contacts file does not exist')
def impl(context):
    if "HOME" in os.environ:
        home_mail_file = os.path.join(os.environ["HOME"], "mail_contacts")
        if CheckFile(home_mail_file).run():
            os.remove(home_mail_file)
    if "GPHOME" in os.environ:
        mail_file = os.path.join(os.environ["GPHOME"], "bin", "mail_contacts")
        if CheckFile(mail_file).run():
            os.remove(mail_file)

@given('the mail_contacts file exists')
def impl(context):
    context.email_contact = "example_test@gopivotal.com"
    if "HOME" in os.environ:
        home_mail_file = os.path.join(os.environ["HOME"], "mail_contacts")
        mail_contact = home_mail_file
    elif "GPHOME" in os.environ:
        mail_file = os.path.join(os.environ["GPHOME"], "bin", "mail_contacts")
        mail_contact = mail_file
    f = file(mail_contact, 'w+')
    f.write(context.email_contact)
    f.close

@given('the yaml file "{email_file_path}" stores email details is in proper format')
def impl(context, email_file_path):
    try:
        validate_parse_email_file(context, email_file_path)
    except Exception as e:
        raise Exception(str(e))

@given('the yaml file "{email_file_path}" stores email details is not in proper format')
def impl(context, email_file_path):
    exception_raised = False
    try:
        validate_parse_email_file(context, email_file_path)
    except Exception as e:
        exception_raised = True
    if exception_raised == False:
        raise Exception("File is in proper format")

@then('verify that emails are sent to the given contacts with appropriate messages after backup of "{dblist}"')
def impl(context, dblist):
    cmd_list = []
    sending_email_list = []
    database_list = dblist.split(',')
    stdout = context.stdout_message
    for line in stdout.splitlines():
        if "Sending mail to" in line:
            str = line.split(':-')[1]
            sending_email_list.append(str.strip())
        if "Email command string=" in line:
            log_msg, delim, txt = line.partition('=')
            cmd_list.append(txt.strip())
    if len(sending_email_list) != len(database_list):
        raise Exception("Emails are not sent properly")
    count = 0
    for dbname in database_list:
        #expected email details
        for email in context.email_details:
            if dbname in email['DBNAME']:
                expected_from  = email['FROM']
                expected_sub = email['SUBJECT']
            else:
                expected_sub = "Report from gpcrondump on host %s [COMPLETED]" % socket.gethostname()
        #original email details
        result_cmd = cmd_list[count]
        str = result_cmd[result_cmd.find("-s")+4:]
        result_sub = (str[:str.find('"')]).strip()
        if expected_sub != result_sub:
            raise Exception("Subject of the sent email is not correct")
        if result_cmd.find("-- -f") >= 0:
            result_from = result_cmd[result_cmd.find("-- -f")+6:]
            if expected_from != result_from:
                raise Exception("ef : RF", expected_from, result_from, count)
                #raise Exception("Sender of the sent email is not correct")
        count += 1

@then('gpcrondump should print unable to send dump email notification to stdout as warning')
def impl(context):
    stdout = context.stdout_message
    found = False
    for line in stdout.splitlines():
        if "Unable to send dump email notification" in line:
           found = True
    if found is False:
        raise Exception("'Unable to send dump email notification' exception is not raised")

@then('verify that function is backedup correctly in "{dumpfile}"')
def impl(context, dumpfile):
    buf = """CREATE ORDERED AGGREGATE agg_array(anyelement) (
    SFUNC = array_append,
    STYPE = anyarray,
    INITCOND = '{}'
);"""
    if not buf in open(dumpfile).read():
        raise Exception("pg_dump did not backup aggregate functions correctly.")

@given('verify that a role "{role_name}" exists in database "{dbname}"')
@then('verify that a role "{role_name}" exists in database "{dbname}"')
def impl(context, role_name, dbname):
    query = "select rolname from pg_roles where rolname = '%s'" % role_name
    conn = dbconn.connect(dbconn.DbURL(dbname=dbname))
    try:
        result = getRows(dbname, query)[0][0]
        if result != role_name:
            raise Exception("Role %s does not exist in database %s." % (role_name, dbname))
    except:
        raise Exception("Role %s does not exist in database %s." % (role_name, dbname))

@given('there is a list of files "{filenames}" of tables "{table_list}" in "{dbname}" exists for validation')
@when('there is a list of files "{filenames}" of tables "{table_list}" in "{dbname}" exists for validation')
@then('there is a list of files "{filenames}" of tables "{table_list}" in "{dbname}" exists for validation')
def impl(context, filenames, table_list, dbname):
    files = [f for f in filenames.split(',')]
    tables = [t for t in table_list.split(',')]
    for t,f in zip(tables,files):
        backup_data_to_file(context, t, dbname, f)

@when('verify with backedup file "{filename}" that there is a "{table_type}" table "{tablename}" in "{dbname}" with data')
@then('verify with backedup file "{filename}" that there is a "{table_type}" table "{tablename}" in "{dbname}" with data')
def impl(context, filename, table_type, tablename, dbname):
    if not check_table_exists(context, dbname=dbname, table_name=tablename, table_type=table_type):
        raise Exception("Table '%s' does not exist when it should" % tablename)
    validate_restore_data_in_file(context, tablename, dbname, filename)

@then('verify that the owner of "{dbname}" is "{expected_owner}"')
def impl(context, dbname, expected_owner):
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        query = "SELECT pg_catalog.pg_get_userbyid(d.datdba) FROM pg_catalog.pg_database d WHERE d.datname = '%s';" % dbname
        actual_owner = dbconn.execSQLForSingleton(conn, query)
    if actual_owner != expected_owner:
        raise Exception("Database %s has owner %s when it should have owner %s" % (dbname, actual_owner, expected_owner))
