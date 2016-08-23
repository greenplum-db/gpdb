#!/usr/bin/env python

from gppylib.gpparseopts import OptParser, OptChecker
from gppylib.operations.backup_utils import split_fqn, checkAndRemoveEnclosingDoubleQuote, removeEscapingDoubleQuoteInSQLString,\
                                            escapeDoubleQuoteInSQLString
import re
import os
import sys

search_path_expr = 'SET search_path = '
set_start = 'S'
set_assignment = '='
len_search_path_expr = len(search_path_expr)
copy_expr = 'COPY '
copy_start = 'C'
copy_expr_end = 'FROM stdin;\n'
len_copy_expr = len(copy_expr)
copy_end_expr = '\\.'
copy_end_start = '\\'
set_expr = 'SET '
drop_start = 'D'
drop_expr = 'DROP '
drop_table_expr = 'DROP TABLE '
drop_external_table_expr = 'DROP EXTERNAL TABLE '
alter_table_only_expr = 'ALTER TABLE ONLY '
alter_table_expr = 'ALTER TABLE '

comment_start_expr = '--'
comment_expr = '-- Name: '
type_expr = '; Type: '
schema_expr = '; Schema: '
owner_expr = '; Owner: '
comment_data_expr_a = '-- Data: '
comment_data_expr_b = '-- Data for Name: '


def get_table_info(line, cur_comment_expr):
    """
    It's complex to split when table name/schema name/user name/ tablespace name
    contains full context of one of others', which is very unlikely, but in
    case it happens, return None.

    Since we only care about table name, type, and schema name, strip the input
    is safe here.

    line: contains the true (un-escaped) schema name, table name, and user name.
    """
    temp = line.strip('\n')
    type_start = find_all_expr_start(temp, type_expr)
    schema_start = find_all_expr_start(temp, schema_expr)
    owner_start = find_all_expr_start(temp, owner_expr)
    if len(type_start) != 1 or len(schema_start) != 1 or len(owner_start) != 1:
        return (None, None, None)
    name = temp[len(cur_comment_expr) : type_start[0]]
    type = temp[type_start[0] + len(type_expr) : schema_start[0]]
    schema = temp[schema_start[0] + len(schema_expr) : owner_start[0]]
    return (name, type, schema)

def get_table_from_alter_table(line, alter_expr):
    """
    Parse the content and return full qualified schema.table from the line if
    schema provided, else return the table name.
    Fact: if schema name or table name contains any special chars, each should be
    double quoted already in dump file.
    """

    dot_separator_idx = line.find('.')
    last_double_quote_idx = line.rfind('"')

    has_schema_table_fmt = True if dot_separator_idx != -1 else False
    has_special_chars = True if last_double_quote_idx != -1 else False

    if not has_schema_table_fmt and not has_special_chars:
        return line[len(alter_expr):].split()[0]
    elif has_schema_table_fmt and not has_special_chars:
        full_table_name = line[len(alter_expr):].split()[0]
        _, table = split_fqn(full_table_name)
        return table
    elif not has_schema_table_fmt and has_special_chars:
        return line[len(alter_expr) + 1 : last_double_quote_idx + 1]
    else:
        if dot_separator_idx < last_double_quote_idx:
            # table name is double quoted
            full_table_name = line[len(alter_expr) : last_double_quote_idx + 1]
        else:
            # only schema name double quoted
            ending_space_idx = line.find(' ', dot_separator_idx)
            full_table_name = line[len(alter_expr) : ending_space_idx]
        _, table = split_fqn(full_table_name)
        return table

def find_all_expr_start(line, expr):
    """
    Find all overlapping matches
    """
    return [m.start() for m in re.finditer('(?=%s)' % expr, line)]

def process_schema(dump_schemas, dump_tables, fdin, fdout, change_schema=None, schema_level_restore_list=None):
    """
    Filter the dump file line by line from restore
    dump_schemas: set of schemas to restore
    dump_tables: set of (schema, table) tuple to restore
    fdin: stdin from dump file
    fdout: to write filtered content to stdout
    change_schema_name: different schema name to restore
    schema_level_restore_list: list of schemas to restore all tables under them
    """

    schema, table = None, None
    line_buff = ''

    # to help decide whether or not to filter out
    output = False

    # to help exclude SET clause within a function's ddl statement
    function_ddl = False

    further_investigation_required = False
    # we need to set search_path to true after every ddl change due to the
    # fact that the schema "set search_path" may change on the next ddl command
    search_path = True
    passedDropSchemaSection = False

    for line in fdin:
        # NOTE: We are checking the first character before actually verifying
        # the line with "startswith" due to the performance gain.
        if search_path and (line[0] == set_start) and line.startswith(search_path_expr):
            # NOTE: The goal is to output the correct mapping to the search path
            # for the schema

            further_investigation_required = False
            # schema in set search_path line is already escaped in dump file
            schema = extract_schema(line)
            schema_wo_escaping = removeEscapingDoubleQuoteInSQLString(schema, False)
            if (dump_schemas and schema_wo_escaping in dump_schemas or
                schema_level_restore_list and schema_wo_escaping in schema_level_restore_list):
                if change_schema and len(change_schema) > 0:
                    # change schema name can contain special chars including white space, double quote that.
                    # if original schema name is already quoted, replaced it with quoted change schema name
                    quoted_schema = '"' + schema + '"'
                    if quoted_schema in line:
                        line = line.replace(quoted_schema, escapeDoubleQuoteInSQLString(change_schema))
                    else:
                        line = line.replace(schema, escapeDoubleQuoteInSQLString(change_schema))
                output = True
                search_path = False
            else:
                output = False
        # set_assignment must be in the line to filter out dump line: SET SUBPARTITION TEMPLATE
        elif (line[0] == set_start) and line.startswith(set_expr) and set_assignment in line and not function_ddl:
            output = True
        elif (line[0] == drop_start) and line.startswith(drop_expr):
            if line.startswith(drop_table_expr) or line.startswith(drop_external_table_expr):
                if passedDropSchemaSection:
                    output = False
                else:
                    if line.startswith(drop_table_expr):
                        output = check_dropped_table(line, dump_tables, schema_level_restore_list, drop_table_expr)
                    else:
                        output = check_dropped_table(line, dump_tables, schema_level_restore_list,
                                                     drop_external_table_expr)
            else:
                output = False
        elif line[:2] == comment_start_expr and line.startswith(comment_expr):
            # Parse the line using get_table_info for SCHEMA relation type as well,
            # if type is SCHEMA, then the value of name returned is schema's name, and returned schema is represented by '-'
            name, type, schema = get_table_info(line, comment_expr)
            output = False
            function_ddl = False
            passedDropSchemaSection = True

            if type in ['SCHEMA']:
                # Make sure that schemas are created before restoring the desired tables.
                output = check_valid_schema(name, dump_schemas, schema_level_restore_list)
            elif type in ['TABLE', 'EXTERNAL TABLE', 'VIEW', 'SEQUENCE']:
                further_investigation_required = False
                output = check_valid_relname(schema, name, dump_tables, schema_level_restore_list)
            elif type in ['CONSTRAINT']:
                further_investigation_required = True
                if check_valid_schema(schema, dump_schemas, schema_level_restore_list):
                    line_buff = line
            elif type in ['ACL']:
                output = check_valid_relname(schema, name, dump_tables, schema_level_restore_list)
            elif type in ['FUNCTION']:
                function_ddl = True
                output = check_valid_relname(schema, name, dump_tables, schema_level_restore_list)

            if output:
                search_path = True

        elif (line[:2] == comment_start_expr) and (line.startswith(comment_data_expr_a) or line.startswith(comment_data_expr_b)):
            passedDropSchemaSection = True
            further_investigation_required = False
            if line.startswith(comment_data_expr_a):
                name, type, schema = get_table_info(line, comment_data_expr_a)
            else:
                name, type, schema = get_table_info(line, comment_data_expr_b)
            if type == 'TABLE DATA':
                output = check_valid_relname(schema, name, dump_tables, schema_level_restore_list)
                if output:
                    search_path = True
            else:
                output = False
        elif further_investigation_required:
            if line.startswith(alter_table_only_expr) or line.startswith(alter_table_expr):
                further_investigation_required = False

                # Get the full qualified table name with the correct split
                if line.startswith(alter_table_only_expr):
                    tablename = get_table_from_alter_table(line, alter_table_only_expr)
                else:
                    tablename = get_table_from_alter_table(line, alter_table_expr)

                tablename = checkAndRemoveEnclosingDoubleQuote(tablename)
                tablename = removeEscapingDoubleQuoteInSQLString(tablename, False)
                output = check_valid_relname(schema, tablename, dump_tables, schema_level_restore_list)

                if output:
                    if line_buff:
                        fdout.write(line_buff)
                        line_buff = ''
                    search_path = True
        else:
            further_investigation_required = False

        if output:
            fdout.write(line)

def check_valid_schema(schema, dump_schemas, schema_level_restore_list=None):
    if ((schema_level_restore_list and schema in schema_level_restore_list) or
        (dump_schemas and schema in dump_schemas)):
        return True
    return False

def check_valid_relname(schema, relname, dump_tables, schema_level_restore_list=None):
    """
    check if relation is valid (can be from schema level restore)
    """

    if ((schema_level_restore_list and schema in schema_level_restore_list) or
       (dump_tables and (schema, relname) in dump_tables)):
        return True
    return False

def get_table_schema_set(filename):
    """
    filename: file with true schema and table name (none escaped), don't strip white space
    on schema and table name in case it's part of the name
    """
    dump_schemas = set()
    dump_tables = set()

    with open(filename) as fd:
        contents = fd.read()
        tables = contents.splitlines()
        for t in tables:
            schema, table = split_fqn(t)
            dump_tables.add((schema, table))
            dump_schemas.add(schema)
    return (dump_schemas, dump_tables)

def extract_schema(line):
    """
    Instead of searching ',' in forwarding way, search ', pg_catalog;'
    reversely, in case schema name contains comma.

    Remove enclosing double quotes only, in case quote is part of the
    schema name
    """
    temp = line[len_search_path_expr:]
    idx = temp.rfind(", pg_catalog;")
    if idx == -1:
        raise Exception('Failed to extract schema name from line %s' % line)
    schema = temp[:idx]
    return checkAndRemoveEnclosingDoubleQuote(schema)

def extract_table(line):
    """
    Instead of looking for table name ending index based on
    empty space, find it in the reverse way based on the ' ('
    whereas the column definition starts.

    Removing the enclosing double quote only, don't do strip('"') in case table name has double quote
    """
    temp = line[len_copy_expr:]
    idx = temp.rfind(" (")
    if idx == -1:
        raise Exception('Failed to extract table name from line %s' % line)
    table = temp[:idx]
    return checkAndRemoveEnclosingDoubleQuote(table)

def check_dropped_table(line, dump_tables, schema_level_restore_list, drop_table_expr):
    """
    check if table to drop is valid (can be dropped from schema level restore)
    """
    temp = line[len(drop_table_expr):].strip()[:-1]
    (schema, table) = split_fqn(temp)
    schema = removeEscapingDoubleQuoteInSQLString(checkAndRemoveEnclosingDoubleQuote(schema), False)
    table = removeEscapingDoubleQuoteInSQLString(checkAndRemoveEnclosingDoubleQuote(table), False)
    if (schema_level_restore_list and schema in schema_level_restore_list) or ((schema, table) in dump_tables):
        return True
    return False

def process_data(dump_schemas, dump_tables, fdin, fdout, change_schema=None, schema_level_restore_list=None):
    schema, table, schema_wo_escaping = None, None, None
    output = False
    #PYTHON PERFORMANCE IS TRICKY .... THIS CODE IS LIKE THIS BECAUSE ITS FAST
    for line in fdin:
        if (line[0] == set_start) and line.startswith(search_path_expr):
            schema = extract_schema(line)
            schema_wo_escaping = removeEscapingDoubleQuoteInSQLString(schema, False)
            if ((dump_schemas and schema_wo_escaping in dump_schemas) or
                (schema_level_restore_list and schema_wo_escaping in schema_level_restore_list)):
                if change_schema:
                    # change schema name can contain special chars including white space, double quote that.
                    # if original schema name is already quoted, replaced it with quoted change schema name
                    quoted_schema = '"' + schema + '"'
                    if quoted_schema in line:
                        line = line.replace(quoted_schema, escapeDoubleQuoteInSQLString(change_schema))
                    else:
                        line = line.replace(schema, escapeDoubleQuoteInSQLString(change_schema))
                else:
                    schema = schema_wo_escaping
                fdout.write(line)
        elif (line[0] == copy_start) and line.startswith(copy_expr) and line.endswith(copy_expr_end):
            table = extract_table(line)
            table = removeEscapingDoubleQuoteInSQLString(table, False)
            if (schema_level_restore_list and schema_wo_escaping in schema_level_restore_list) or (dump_tables and (schema_wo_escaping, table) in dump_tables):
                output = True
        elif output and (line[0] == copy_end_start) and line.startswith(copy_end_expr):
            table = None
            output = False
            fdout.write(line)

        if output:
            fdout.write(line)

def get_schema_level_restore_list(schema_level_restore_file=None):
    """
    Note: white space in schema and table name is supported now, don't do strip on them
    """
    if not os.path.exists(schema_level_restore_file):
        raise Exception('schema level restore file path %s does not exist' % schema_level_restore_file)
    schema_level_restore_list = []
    with open(schema_level_restore_file) as fr:
        schema_entries = fr.read()
        schema_level_restore_list = schema_entries.splitlines()
    return schema_level_restore_list

def get_change_schema_name(change_schema_file):
    """
    Only strip the '\n' as it is one of the non-supported chars to be part
    of the schema or table name
    """
    if not os.path.exists(change_schema_file):
        raise Exception('change schema file path %s does not exist' % change_schema_file)
    change_schema_name = None
    with open(change_schema_file) as fr:
        line = fr.read()
        change_schema_name = line.strip('\n')
    return change_schema_name

if __name__ == "__main__":
    parser = OptParser(option_class=OptChecker)
    parser.remove_option('-h')
    parser.add_option('-h', '-?', '--help', action='store_true')
    parser.add_option('-t', '--tablefile', type='string', default=None)
    parser.add_option('-m', '--master_only', action='store_true')
    parser.add_option('-c', '--change-schema-file', type='string', default=None)
    parser.add_option('-s', '--schema-level-file', type='string', default=None)
    (options, args) = parser.parse_args()
    if not (options.tablefile or options.schema_level_file):
        raise Exception('-t table file name or -s schema level file name must be specified')
    elif options.schema_level_file and options.change_schema_file:
        raise Exception('-s schema level file option can not be specified with -c change schema file option')

    schemas, tables = None, None
    if options.tablefile:
        (schemas, tables) = get_table_schema_set(options.tablefile)

    change_schema_name = None
    if options.change_schema_file:
        change_schema_name = get_change_schema_name(options.change_schema_file)

    schema_level_restore_list = None
    if options.schema_level_file:
        schema_level_restore_list = get_schema_level_restore_list(options.schema_level_file)

    if options.master_only:
        process_schema(schemas, tables, sys.stdin, sys.stdout, change_schema_name, schema_level_restore_list)
    else:
        process_data(schemas, tables, sys.stdin, sys.stdout, change_schema_name, schema_level_restore_list)

