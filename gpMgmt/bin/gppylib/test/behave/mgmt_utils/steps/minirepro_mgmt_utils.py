import os, mmap
from gppylib.test.behave_utils.utils import drop_database_if_exists, drop_table_if_exists

@given('database "{dbname}" does not exist')
def impl(context, dbname):
    drop_database_if_exists(context, dbname)

@given('the file "{file_name}" does not exist')
def impl(context, file_name):
    if os.path.isfile(file_name):
        os.remove(file_name)

@given('the file "{file_name}" exists and contains "{sql_query}"')
def impl(context, file_name, sql_query):
    if os.path.isfile(file_name):
        os.remove(file_name)
    file_dir = os.path.dirname(file_name)
    if not os.path.exists(file_dir):
        os.makedirs(file_dir)
    with open(file_name, 'w') as query_f:
            query_f.writelines(sql_query)

@given('the table "{rel_name}" does not exist in database "{db_name}"')
def impl(context, rel_name, db_name):
    drop_table_if_exists(context, rel_name, db_name)

@then('minirepro error should contain {output}')
def impl(context, output):
    pat = re.compile(output)
    if not pat.search(context.error_message):
        err_str = "Expected stderr string '%s', but found:\n'%s'" % (output, context.error_message)
        raise Exception(err_str)

@then('the output file "{output_file}" should exist')
def impl(context, output_file):
    if not os.path.isfile(output_file):
        err_str = "The output file '%s' does not exist.\n" % output_file
        raise Exception(err_str)

@then('the output file "{output_file}" should contain "{str_before}" before "{str_after}"')
def impl(context, output_file, str_before, str_after):
    with open(output_file, 'r') as output_f:
        s = mmap.mmap(output_f.fileno(), 0, access=mmap.ACCESS_READ)
        pos_before = s.find(str_before)
        pos_after = s.find(str_after)
        if pos_before == -1:
            raise Exception('%s not found.' % str_before)
        if pos_after == -1:
            raise Exception('%s not found.' % str_after)
        if pos_before >= pos_after:
            raise Exception('%s not before %s.' % (str_before, str_after))

@then('the output file "{output_file}" should contain "{search_str}"')
def impl(context, output_file, search_str):
    with open(output_file, 'r') as output_f:
        s = mmap.mmap(output_f.fileno(), 0, access=mmap.ACCESS_READ)
        if s.find(search_str) == -1:
            raise Exception('%s not found.' % search_str)

@then('the output file "{output_file}" should not contain "{search_str}"')
def impl(context, output_file, search_str):
    with open(output_file, 'r') as output_f:
        s = mmap.mmap(output_f.fileno(), 0, access=mmap.ACCESS_READ)
        if s.find(search_str) != -1:
            raise Exception('%s should not exist.' % search_str)

@then('the output file "{output_file}" should be loaded to database "{db_name}" without error')
def impl(context, output_file, db_name):
    drop_database_if_exists(context, db_name)
    create_database_if_not_exists(context, db_name)
    with open(output_file, "r") as fin:
        sql_command = fin.read().replace('\\connect ', '--\\connect ')
    with open(output_file, "w") as fout:
        fout.writelines(sql_command)
    run_gpcommand(context, 'psql -d %s -f %s' % (db_name, output_file))
    if 'ERROR:' in context.error_message:
        raise Exception('Database %s failed to run %s, error message: %s' % (db_name, output_file, context.error_message))

@then('the file "{query_file}" should be executed in database "{db_name}" without error')
def impl(context, query_file, db_name):
    run_gpcommand(context, 'psql -d %s -f %s' % (db_name, query_file))
    if 'ERROR:' in context.error_message:
        raise Exception('Database %s failed to run %s, error message: %s' % (db_name, query_file, context.error_message))
    drop_database_if_exists(context, db_name)
