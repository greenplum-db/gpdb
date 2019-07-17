import re
import os
import shutil
from gppylib.db import dbconn
from test.behave_utils.utils import check_schema_exists, check_table_exists, drop_table_if_exists
from behave import given, when, then

CREATE_MULTI_PARTITION_TABLE_SQL = """
CREATE TABLE %s.%s (trans_id int, date date, amount decimal(9,2), region text)
WITH (appendonly=true, orientation=column)
DISTRIBUTED BY (trans_id)
PARTITION BY RANGE (date)
SUBPARTITION BY LIST (region)
SUBPARTITION TEMPLATE
( SUBPARTITION usa VALUES ('usa'),
  SUBPARTITION asia VALUES ('asia'),
  SUBPARTITION europe VALUES ('europe'),
  DEFAULT SUBPARTITION other_regions)
  (START (date '2011-01-01') INCLUSIVE
  END (date '2012-01-01') EXCLUSIVE
  EVERY (INTERVAL '5 month'),
  DEFAULT PARTITION outlying_dates)
"""

CREATE_PARTITION_TABLE_SQL = """
CREATE TABLE %s.%s (id int, date date) WITH (appendonly=true, orientation=column)
DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
( START (date '2008-01-01') INCLUSIVE
END (date '2008-01-04') EXCLUSIVE
EVERY (INTERVAL '1 day'),
DEFAULT PARTITION default_dates);
"""


@given('there is a regular "{storage_type}" table "{tablename}" with column name list "{col_name_list}" and column type list "{col_type_list}" in schema "{schemaname}"')
def impl(context, storage_type, tablename, col_name_list, col_type_list, schemaname):
    schemaname_no_quote = schemaname
    if '"' in schemaname:
        schemaname_no_quote = schemaname[1:-1]
    if not check_schema_exists(context, schemaname_no_quote, context.dbname):
        raise Exception("Schema %s does not exist in database %s" % (schemaname_no_quote, context.dbname))
    drop_table_if_exists(context, '.'.join([schemaname, tablename]), context.dbname)
    create_table_with_column_list(context.conn, storage_type, schemaname, tablename, col_name_list, col_type_list)
    check_table_exists(context, context.dbname, '.'.join([schemaname, tablename]), table_type=storage_type)


@given('there is a hard coded ao partition table "{tablename}" with 4 child partitions in schema "{schemaname}"')
def impl(context, tablename, schemaname):
    if not check_schema_exists(context, schemaname, context.dbname):
        raise Exception("Schema %s does not exist in database %s" % (schemaname, context.dbname))
    drop_table_if_exists(context, '.'.join([schemaname, tablename]), context.dbname)
    dbconn.execSQL(context.conn, CREATE_PARTITION_TABLE_SQL % (schemaname, tablename))
    context.conn.commit()
    check_table_exists(context, context.dbname, '.'.join([schemaname, tablename]), table_type='ao')


@given('there is a hard coded multi-level ao partition table "{tablename}" with 4 mid-level and 16 leaf-level partitions in schema "{schemaname}"')
def impl(context, tablename, schemaname):
    if not check_schema_exists(context, schemaname, context.dbname):
        raise Exception("Schema %s does not exist in database %s" % (schemaname, context.dbname))
    drop_table_if_exists(context, '.'.join([schemaname, tablename]), context.dbname)
    dbconn.execSQL(context.conn, CREATE_MULTI_PARTITION_TABLE_SQL % (schemaname, tablename))
    context.conn.commit()
    check_table_exists(context, context.dbname, '.'.join([schemaname, tablename]), table_type='ao')


@given('no state files exist for database "{dbname}"')
def impl(context, dbname):
    analyze_dir = get_analyze_dir(dbname)
    if os.path.exists(analyze_dir):
        shutil.rmtree(analyze_dir)


@then('"{number}" analyze directories exist for database "{dbname}"')
def impl(context, number, dbname):
    dirs_found = get_list_of_analyze_dirs(dbname)
    if str(number) != str(len(dirs_found)):
        raise Exception("number of directories expected, %s, didn't match number found: %s" % (
            str(number), str(len(dirs_found))))


@given('a view "{view_name}" exists on table "{table_name}" in schema "{schema_name}"')
def impl(context, view_name, table_name, schema_name):
    create_view_on_table(context.conn, schema_name, table_name, view_name)


@given('"{qualified_table}" appears in the latest state files')
@then('"{qualified_table}" should appear in the latest state files')
def impl(context, qualified_table):
    found, filename = table_found_in_state_file(context.dbname, qualified_table)
    if not found:
        if filename == '':
            assert False, "no state files found for database %s" % context.dbname
        else:
            assert False, "table %s not found in state file %s" % (qualified_table, os.path.basename(filename))


@given('"{expected_result}" should appear in the latest ao_state file in database "{dbname}"')
@then('"{expected_result}" should appear in the latest ao_state file in database "{dbname}"')
def impl(context, expected_result, dbname):
    latest_file = get_latest_aostate_file(dbname)
    with open(latest_file, 'r') as f:
        for line in f:
            if expected_result in line:
                return True
    raise Exception("couldn't find %s in %s" % (expected_result, latest_file))


@given('columns "{col_name_list}" of table "{qualified_table}" appear in the latest column state file')
@then('columns "{col_name_list}" of table "{qualified_table}" should appear in the latest column state file')
def impl(context, col_name_list, qualified_table):
    found, column, filename = column_found_in_state_file(context.dbname, qualified_table, col_name_list)
    if not found:
        if filename == '':
            assert False, "no column state file found for database %s" % context.dbname
        else:
            assert False, "column(s) %s of table %s not found in state file %s" % (
                column, qualified_table, os.path.basename(filename))


@given('column "{col_name}" of table "{qualified_table}" does not appear in the latest column state file')
@then('column "{col_name}" of table "{qualified_table}" should not appear in the latest column state file')
def impl(context, col_name, qualified_table):
    found, column, filename = column_found_in_state_file(context.dbname, qualified_table, col_name)
    if found:
        if filename == '':
            assert False, "no column state file found for database %s" % context.dbname
        else:
            assert False, "unexpected column %s of table %s found in state file %s" % (
                column, qualified_table, os.path.basename(filename))


@given('"{qualified_table}" appears in the latest report file')
@then('"{qualified_table}" should appear in the latest report file')
def impl(context, qualified_table):
    found, filename = table_found_in_report_file(context.dbname, qualified_table)
    if not found:
        assert False, "table %s not found in report file %s" % (qualified_table, os.path.basename(filename))


@then('output should contain either "{output1}" or "{output2}"')
def impl(context, output1, output2):
    pat1 = re.compile(output1)
    pat2 = re.compile(output2)
    if not pat1.search(context.stdout_message) and not pat2.search(context.stdout_message):
        err_str = "Expected stdout string '%s' or '%s', but found:\n'%s'" % (output1, output2, context.stdout_message)
        raise Exception(err_str)


@then('output should not contain "{output1}"')
def impl(context, output1):
    pat1 = re.compile(output1)
    if pat1.search(context.stdout_message):
        err_str = "Unexpected stdout string '%s', found:\n'%s'" % (output1, context.stdout_message)
        raise Exception(err_str)


@then('output should contain both "{output1}" and "{output2}"')
def impl(context, output1, output2):
    pat1 = re.compile(output1)
    pat2 = re.compile(output2)
    if not pat1.search(context.stdout_message) or not pat2.search(context.stdout_message):
        err_str = "Expected stdout string '%s' and '%s', but found:\n'%s'" % (output1, output2, context.stdout_message)
        raise Exception(err_str)


@given('table "{qualified_table}" does not appear in the latest state files')
def impl(context, qualified_table):
    found, filename = table_found_in_state_file(context.dbname, qualified_table)
    if found:
        delete_table_from_state_files(context.dbname, qualified_table)


@given('{num_rows} rows are inserted into table "{tablename}" in schema "{schemaname}" with column type list "{column_type_list}"')
@then('{num_rows} rows are inserted into table "{tablename}" in schema "{schemaname}" with column type list "{column_type_list}"')
@when('{num_rows} rows are inserted into table "{tablename}" in schema "{schemaname}" with column type list "{column_type_list}"')
def impl(context, num_rows, tablename, schemaname, column_type_list):
    insert_data_into_table(context.conn, schemaname, tablename, column_type_list, num_rows)

@given('some data is inserted into table "{tablename}" in schema "{schemaname}" with column type list "{column_type_list}"')
@when('some data is inserted into table "{tablename}" in schema "{schemaname}" with column type list "{column_type_list}"')
def impl(context, tablename, schemaname, column_type_list):
    insert_data_into_table(context.conn, schemaname, tablename, column_type_list)

@given('some ddl is performed on table "{tablename}" in schema "{schemaname}"')
def impl(context, tablename, schemaname):
    perform_ddl_on_table(context.conn, schemaname, tablename)


@given('the user starts a transaction and runs "{query}" on "{dbname}"')
@when('the user starts a transaction and runs "{query}" on "{dbname}"')
def impl(context, query, dbname):
    if 'long_lived_conn' not in context:
        create_long_lived_conn(context, dbname)
    dbconn.execSQL(context.long_lived_conn, 'BEGIN; %s' % query)


@given('the user rollsback the transaction')
@when('the user rollsback the transaction')
def impl(context):
    dbconn.execSQL(context.long_lived_conn, 'ROLLBACK;')


@then('the latest state file should have a mod count of {mod_count} for table "{table}" in "{schema}" schema for database "{dbname}"')
def impl(context, mod_count, table, schema, dbname):
    mod_count_in_state_file = get_mod_count_in_state_file(dbname, schema, table)
    if mod_count_in_state_file != mod_count:
        raise Exception(
            "mod_count %s does not match mod_count %s in state file for %s.%s" %
             (mod_count, mod_count_in_state_file, schema, table))


@then('root stats are populated for partition table "{tablename}" for database "{dbname}"')
def impl(context, tablename, dbname):
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        query = "select count(*) from pg_statistic where starelid='%s'::regclass;" % tablename
        num_tuples = dbconn.execSQLForSingleton(conn, query)
        if num_tuples == 0:
            raise Exception("Expected partition table %s to contain root statistics" % tablename)

def get_mod_count_in_state_file(dbname, schema, table):
    file = get_latest_aostate_file(dbname)
    comma_name = ','.join([schema, table])
    with open(file) as fd:
        for line in fd:
            if comma_name in line:
                return line.split(',')[2].strip()
    return -1


def create_long_lived_conn(context, dbname):
    context.long_lived_conn = dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)


def table_found_in_state_file(dbname, qualified_table):
    comma_name = ','.join(qualified_table.split('.'))
    files = get_latest_analyze_state_files(dbname)
    if len(files) == 0:
        return False, ""
    state_file = ""
    for state_file in files:
        found = False
        with open(state_file) as fd:
            for line in fd:
                if comma_name in line:
                    found = True
                    continue
            if not found:
                return False, state_file
    return True, state_file


def table_found_in_report_file(dbname, qualified_table):
    report_file = get_latest_analyze_report_file(dbname)
    with open(report_file) as fd:
        for line in fd:
            if qualified_table == line.strip('\n'):
                return True, report_file

    return False, report_file


def column_found_in_state_file(dbname, qualified_table, col_name_list):
    comma_name = ','.join(qualified_table.split('.'))
    files = get_latest_analyze_state_files(dbname)
    if len(files) == 0:
        return False, "", ""

    for state_file in files:
        if "col_state_file" not in state_file:
            continue
        with open(state_file) as fd:
            for line in fd:
                line = line.strip('\n')
                if comma_name in line:
                    for column in col_name_list.split(','):
                        if column not in line.split(',')[2:]:
                            return False, column, state_file
                    return True, "", state_file
        return False, col_name_list, state_file


def delete_table_from_state_files(dbname, qualified_table):
    comma_name = ','.join(qualified_table.split('.'))
    files = get_latest_analyze_state_files(dbname)
    for filename in files:
        lines = []
        with open(filename) as fd:
            for line in fd:
                lines.append(line.strip('\n'))
        f = open(filename, "w")
        for line in lines:
            if comma_name not in line:
                f.write(line)
        f.close()


def get_list_of_analyze_dirs(dbname):
    analyze_dir = get_analyze_dir(dbname)
    if not os.path.exists(analyze_dir):
        return []

    ordered_list = [os.path.join(analyze_dir, x) for x in sorted(os.listdir(analyze_dir), reverse=True)]
    return filter(os.path.isdir, ordered_list)


def get_latest_analyze_dir(dbname):
    analyze_dir = get_analyze_dir(dbname)
    folders = get_list_of_analyze_dirs(dbname)

    if len(folders) == 0:
        return []
    return os.path.join(analyze_dir, folders[0])


def get_analyze_dir(dbname):
    master_data_dir = os.environ.get('MASTER_DATA_DIRECTORY')
    analyze_dir = os.path.join(master_data_dir, 'db_analyze', dbname)
    return analyze_dir


def get_latest_aostate_file(dbname):
    for path in get_latest_analyze_state_files(dbname):
        if 'ao_state' in path:
            return path
    return None


def get_latest_analyze_state_files(dbname):
    """
    return the latest state files (absolute paths)
    """
    state_files_dir = get_latest_analyze_dir(dbname)
    if not state_files_dir:
        return []
    files = os.listdir(state_files_dir)

    if len(files) != 4:
        raise Exception("Missing or unexpected state files in folder %s" % state_files_dir)
    ret = []
    for f in files:
        if 'report' not in f:
            ret.append(os.path.join(state_files_dir, f))
    return ret


def get_latest_analyze_report_file(dbname):
    """
    return the latest report file (absolute path)
    """
    report_file_dir = get_latest_analyze_dir(dbname)
    if not report_file_dir:
        return []
    files = os.listdir(report_file_dir)

    for f in files:
        if 'report' in f:
            return os.path.join(report_file_dir, f)

    raise Exception("Missing report file in folder %s" % report_file_dir)


def create_table_with_column_list(conn, storage_type, schemaname, tablename, col_name_list, col_type_list):
    col_name_list = col_name_list.strip().split(',')
    col_type_list = col_type_list.strip().split(',')
    col_list = ' (' + ','.join(['%s %s' % (x, y) for x, y in zip(col_name_list, col_type_list)]) + ') '

    if storage_type.lower() == 'heap':
        storage_str = ''
    elif storage_type.lower() == 'ao':
        storage_str = " with (appendonly=true) "
    elif storage_type.lower() == 'co':
        storage_str = " with (appendonly=true, orientation=column) "
    else:
        raise Exception("Invalid storage type")

    query = 'CREATE TABLE %s.%s %s %s DISTRIBUTED RANDOMLY' % (schemaname, tablename, col_list, storage_str)
    dbconn.execSQL(conn, query)
    conn.commit()


def insert_data_into_table(conn, schemaname, tablename, col_type_list, num_rows="100"):
    col_type_list = col_type_list.strip().split(',')
    col_str = ','.join(["(random()*i)::%s" % x for x in col_type_list])
    query = "INSERT INTO " + schemaname + '.' + tablename + " SELECT " + col_str + " FROM generate_series(1," + num_rows + ") i"
    dbconn.execSQL(conn, query)
    conn.commit()


def perform_ddl_on_table(conn, schemaname, tablename):
    query = "ALTER TABLE " + schemaname + '.' + tablename + " ADD COLUMN tempcol int default 0"
    dbconn.execSQL(conn, query)
    query = "ALTER TABLE " + schemaname + '.' + tablename + " DROP COLUMN tempcol"
    dbconn.execSQL(conn, query)
    conn.commit()


def create_view_on_table(conn, schemaname, tablename, viewname):
    query = "CREATE OR REPLACE VIEW " + schemaname + "." + viewname + \
            " AS SELECT * FROM " + schemaname + "." + tablename
    dbconn.execSQL(conn, query)
    conn.commit()
