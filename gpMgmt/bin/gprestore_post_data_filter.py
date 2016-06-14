#!/usr/bin/env python

from gppylib.gpparseopts import OptParser, OptChecker
from gppylib.operations.backup_utils import split_fqn, checkAndRemoveEnclosingDoubleQuote, removeEscapingDoubleQuoteInSQLString, \
                                            escapeDoubleQuoteInSQLString
import re
import sys
import os

search_path_expr = 'SET search_path = '
set_start = 'S'
len_search_path_expr = len(search_path_expr)
set_expr = 'SET '

extra_rule_keyword = [' WHERE ', ' DO ']
comment_start_expr = '--'
comment_expr = '-- Name: '
comment_data_expr = '-- Data: '
type_expr = '; Type: '
schema_expr = '; Schema: '

command_start_expr = 'CREATE '

def get_type(line):
    temp = line.strip()
    type_start = find_all_expr_start(temp, type_expr)
    schema_start = find_all_expr_start(temp, schema_expr)
    if len(type_start) != 1 or len(schema_start) != 1:
        return None
    type = temp[type_start[0] + len(type_expr) : schema_start[0]]
    return type

def find_all_expr_start(line, expr):
    """
    Find all overlapping matches
    """
    return [m.start() for m in re.finditer('(?=%s)' % expr, line)]

def locate_unquoted_keyword(line, keyword):
    indexes = find_all_expr_start(line, keyword)
    if len(indexes) > 0:
        for idx in indexes:
            if len(find_all_expr_start(line[ : idx], '"')) % 2 == 0 and len(find_all_expr_start(line[idx + len(keyword) : ], '"')) % 2 == 0:
                return idx
    else:
        return -1

def process_schema(dump_schemas, dump_tables, fdin, fdout, change_schema_name=None, schema_level_restore_list=None):
    """
    Filter the dump file line by line from restore
    dump_schemas: set of schemas to restore
    dump_tables: set of (schema, table) tuple to restore
    fdin: stdin from dump file
    fdout: to write filtered content to stdout
    change_schema_name: different schema name to restore
    schema_level_restore_list: list of schemas to restore all tables under them
    """
    schema = None
    schema_wo_escaping = None
    type = None
    schema_buff = ''
    output = False
    further_investigation_required = False
    search_path = False
    line_buf = None
    for line in fdin:
        if (line[0] == set_start) and line.startswith(search_path_expr):
            output = False
            further_investigation_required = False
            # schema in set search_path line is already escaped in dump file
            schema = extract_schema(line)
            schema_wo_escaping = removeEscapingDoubleQuoteInSQLString(schema, False)
            if ((dump_schemas and schema_wo_escaping in dump_schemas) or 
                (schema_level_restore_list and schema_wo_escaping in schema_level_restore_list)):
                if change_schema_name and len(change_schema_name) > 0:
                    # change schema name can contain special chars including white space, double quote that.
                    # if original schema name is already quoted, replaced it with quoted change schema name
                    quoted_schema = '"' + schema + '"'
                    if quoted_schema in line:
                        line = line.replace(quoted_schema, escapeDoubleQuoteInSQLString(change_schema_name))
                    else:
                        line = line.replace(schema, escapeDoubleQuoteInSQLString(change_schema_name))
                search_path = True
                schema_buff = line
        elif (line[0] == set_start) and line.startswith(set_expr):
            output = True
        elif line[:2] == comment_start_expr:
            if line.startswith(comment_expr):
                type = get_type(line)
            output = False
        elif type and (line[:7] == 'CREATE ' or line[:8] == 'REPLACE '):
            if type == 'RULE':
                output = check_table(schema_wo_escaping, line, ' TO ', dump_tables, schema_level_restore_list, is_rule=True)
            elif type == 'INDEX':
                output = check_table(schema_wo_escaping, line, ' ON ', dump_tables, schema_level_restore_list)
            elif type == 'TRIGGER':
                line_buf = line
                further_investigation_required = True
        elif type and type in ['CONSTRAINT', 'FK CONSTRAINT'] and line[:12] == 'ALTER TABLE ':
            if line.startswith('ALTER TABLE ONLY'):
                output = check_table(schema_wo_escaping, line, ' ONLY ', dump_tables, schema_level_restore_list)
            else:
                output = check_table(schema_wo_escaping, line, ' TABLE ', dump_tables, schema_level_restore_list)
        elif further_investigation_required:
            if type == 'TRIGGER':
                output = check_table(schema_wo_escaping, line, ' ON ', dump_tables, schema_level_restore_list)
                if not output:
                    line_buf = None
                further_investigation_required = False

        if output:
            if search_path:
                fdout.write(schema_buff)
                schema_buff = None
                search_path = False
            if line_buf:
                fdout.write(line_buf)
                line_buf = None
            fdout.write(line)

# Given a line like 'ALTER TABLE ONLY tablename\n' and a search_str like ' ONLY ',
# extract everything between the search_str and the next space or the end of the string, whichever comes first.
def check_table(schema, line, search_str, dump_tables, schema_level_restore_list=None, is_rule=False):
    if schema_level_restore_list and schema in schema_level_restore_list:
        return True

    if dump_tables:
        try:
            comp_set = set()
            start = line.index(search_str) + len(search_str)
            if is_rule:
                # cut the line nicely based on extra keyword for create rule statement
                # in case [WHERE condition] clause contains any special chars, cut before WHERE
                end = locate_unquoted_keyword(line, extra_rule_keyword[0])
                if end == -1:
                    end = locate_unquoted_keyword(line, extra_rule_keyword[1])
                line = line[:end]

            dot_separator_idx = line.find('.')
            last_double_quote_idx = line.rfind('"')

            has_schema_table_fmt = True if dot_separator_idx != -1 else False
            has_special_chars = True if last_double_quote_idx != -1 else False

            if not has_schema_table_fmt and not has_special_chars:
                table = line[start:].split()[0]
            elif has_schema_table_fmt and not has_special_chars:
                full_table_name = line[start:].split()[0]
                _, table = split_fqn(full_table_name)
            elif not has_schema_table_fmt and has_special_chars:
                table = line[start : last_double_quote_idx + 1]
            else:
                if dot_separator_idx < last_double_quote_idx:
                    # table name is double quoted
                    full_table_name = line[start : last_double_idx + 1]
                else:
                    # only schema name double quoted
                    ending_space_idx = line.find(' ', dot_separator_idx)
                    full_table_name = line[start : ending_space_idx]
                _, table = split_fqn(full_table_name)

            table = checkAndRemoveEnclosingDoubleQuote(table)
            table = removeEscapingDoubleQuoteInSQLString(table, False)
            comp_set.add((schema, table))

            if comp_set.issubset(dump_tables):
                return True
            return False
        except:
            return False
    else:
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
        return None
    schema = temp[:idx]
    return checkAndRemoveEnclosingDoubleQuote(schema)

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


if __name__ == "__main__":
    parser = OptParser(option_class=OptChecker)
    parser.remove_option('-h')
    parser.add_option('-h', '-?', '--help', action='store_true')
    parser.add_option('-t', '--tablefile', type='string', default=None)
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

    process_schema(schemas, tables, sys.stdin, sys.stdout, change_schema_name, schema_level_restore_list)
