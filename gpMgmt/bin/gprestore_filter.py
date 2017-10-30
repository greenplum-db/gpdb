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
copy_end_expr = '\\.\n'
copy_end_start = '\\'
set_expr = 'SET '
drop_start = 'D'
drop_expr = 'DROP '
drop_table_expr = 'DROP TABLE '
drop_external_table_expr = 'DROP EXTERNAL TABLE '
alter_table_only_expr = 'ALTER TABLE ONLY '
alter_table_expr = 'ALTER TABLE '

line_end_expr = ';\n'
comment_start_expr = '--'
comment_expr = '-- Name: '
type_expr = '; Type: '
schema_expr = '; Schema: '
owner_expr = '; Owner: '
comment_data_expr_a = '-- Data: '
comment_data_expr_b = '-- Data for Name: '
begin_start = 'B'
begin_expr = 'BEGIN'
end_start = 'E'
end_expr = 'END'


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
    data_type = temp[type_start[0] + len(type_expr) : schema_start[0]]
    schema = temp[schema_start[0] + len(schema_expr) : owner_start[0]]
    return (name, data_type, schema)

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

class ParserState:

    def __init__(self):
        self.output = False # to help decide whether or not to filter out
        self.function_ddl = False  # to help exclude SET clause within a function's ddl statement
        self.further_investigation_required = False
        # we need to set search_path to true after every ddl change due to the
        # fact that the schema "set search_path" may change on the next ddl command
        self.cast_func_schema = None
        self.change_cast_func_schema = False
        self.in_block = False
        self.line_buff = ''
        self.schema = None

def _handle_begin_end_block(state, line, _):

    if (line[0] == begin_start) and line.startswith(begin_expr) and not state.function_ddl:
        state.in_block = True
        state.output = True
    elif (line[0] == end_start) and line.startswith(end_expr) and not state.function_ddl:
        state.in_block = False
        state.output = True
    elif state.in_block:
        state.output = True
    else:
        return False, state, line
    return True, state, line

def _handle_change_schema(schema_to_replace, change_schema, line):
    if change_schema and len(change_schema) > 0:
        # change schema name can contain special chars including white space, double quote that.
        # if original schema name is already quoted, replaced it with quoted change schema name
        quoted_schema = '"' + schema_to_replace + '"'
        if quoted_schema in line:
            line = line.replace(quoted_schema, escapeDoubleQuoteInSQLString(change_schema))
        else:
            line = line.replace(schema_to_replace, escapeDoubleQuoteInSQLString(change_schema))
    return line

def _handle_set_statement(state, line, arguments):
    schemas_in_table_file = arguments.schemas
    change_schema = arguments.change_schema_name
    schemas_in_schema_file = arguments.schemas_in_schema_file

    if (line[0] == set_start) and line.startswith(search_path_expr) and line.endswith(line_end_expr):
        # NOTE: The goal is to output the correct mapping to the search path
        # for the schema

        state.further_investigation_required = False
        # schema in set state.search_path line is already escaped in dump file
        state.schema = extract_schema(line)
        schema_wo_escaping = removeEscapingDoubleQuoteInSQLString(state.schema, False)
        if state.schema == "pg_catalog":
            state.output = True
        elif (schemas_in_table_file and schema_wo_escaping in schemas_in_table_file or
                      schemas_in_schema_file and schema_wo_escaping in schemas_in_schema_file):
            line = _handle_change_schema(state.schema, change_schema, line)
            state.cast_func_schema = state.schema # Save the schema in case we need to replace a cast's function's schema later
            state.output = True
        else:
            state.output = False
        return True, state, line
    return False, state, line

def _handle_set_assignment(state, line, _):
    # set_assignment must be in the line to filter out dump line: SET SUBPARTITION TEMPLATE
    if (line[0] == set_start) and line.startswith(set_expr) and set_assignment in line and not state.function_ddl:
        state.output = True
        return True, state, line
    return False, state, line

def _handle_expressions_in_comments(state, line, arguments):
    schemas_in_table_file = arguments.schemas
    tables_in_table_file = arguments.tables
    schemas_in_schema_file = arguments.schemas_in_schema_file

    if line[:2] == comment_start_expr and line.startswith(comment_expr):
        # Parse the line using get_table_info for SCHEMA relation type as well,
        # if type is SCHEMA, then the value of name returned is schema's name, and returned schema is represented by '-'
        name, data_type, state.schema = get_table_info(line, comment_expr)
        state.output = False
        state.function_ddl = False

        if data_type in ['SCHEMA']:
            # Make sure that schemas are created before restoring the desired tables.
            state.output = check_valid_schema(name, schemas_in_table_file, schemas_in_schema_file)
        elif data_type in ['TABLE', 'EXTERNAL TABLE', 'VIEW', 'SEQUENCE']:
            state.further_investigation_required = False
            state.output = check_valid_relname(state.schema, name, tables_in_table_file, schemas_in_schema_file)
        elif data_type in ['CONSTRAINT']:
            state.further_investigation_required = True
            if check_valid_schema(state.schema, schemas_in_table_file, schemas_in_schema_file):
                state.line_buff = line
        elif data_type in ['ACL']:
            relname_valid = check_valid_relname(state.schema, name, tables_in_table_file, schemas_in_schema_file)
            schema_valid = False
            if state.schema == "-":
                schema_valid = check_valid_schema(name, schemas_in_table_file, schemas_in_schema_file)
            state.output = relname_valid or schema_valid
        elif data_type in ['FUNCTION']:
            state.function_ddl = True
            state.output = check_valid_schema(state.schema, schemas_in_table_file, schemas_in_schema_file)
        elif data_type in ['CAST', 'PROCEDURAL LANGUAGE']:  # Restored to pg_catalog, so always filtered in
            state.output = True
            state.change_cast_func_schema = True  # When changing schemas, we need to ensure that functions used in casts reference the new schema
        return True, state, line
    return False, state, line

def _handle_data_expressions_in_comments(state, line, arguments):
    tables_in_table_file = arguments.tables
    schemas_in_schema_file = arguments.schemas_in_schema_file

    if (line[:2] == comment_start_expr) and (line.startswith(comment_data_expr_a) or line.startswith(comment_data_expr_b)):
        state.further_investigation_required = False
        if line.startswith(comment_data_expr_a):
            name, data_type, state.schema = get_table_info(line, comment_data_expr_a)
        else:
            name, data_type, state.schema = get_table_info(line, comment_data_expr_b)
        if data_type == 'TABLE DATA':
            state.output = check_valid_relname(state.schema, name, tables_in_table_file, schemas_in_schema_file)
        else:
            state.output = False
        return True, state, line
    return False, state, line

def _handle_further_investigation(state, line, arguments):
    tables_in_table_file = arguments.tables
    schemas_in_schema_file = arguments.schemas_in_schema_file

    if state.further_investigation_required:
        if line.startswith(alter_table_expr):
            state.further_investigation_required = False

            # Get the full qualified table name with the correct split
            if line.startswith(alter_table_only_expr):
                tablename = get_table_from_alter_table(line, alter_table_only_expr)
            else:
                tablename = get_table_from_alter_table(line, alter_table_expr)

            tablename = checkAndRemoveEnclosingDoubleQuote(tablename)
            tablename = removeEscapingDoubleQuoteInSQLString(tablename, False)
            state.output = check_valid_relname(state.schema, tablename, tables_in_table_file, schemas_in_schema_file)
            if state.output:
                if state.line_buff:
                    line = state.line_buff + line
                    state.line_buff = ''
        return True, state, line
    return False, state, line


def _handle_cast_function_schema(state, line, arguments):
    change_schema = arguments.change_schema_name

    if state.change_cast_func_schema:
        if "CREATE CAST" in line and "WITH FUNCTION" in line:
            state.change_cast_func_schema = False
            line = _handle_change_schema(state.cast_func_schema, change_schema, line)
            state.cast_func_schema = None
        return True, state, line
    return False, state, line

def process_line(state, line, arguments):
    # NOTE: We are checking the first character before actually verifying
    # the line with "startswith" due to the performance gain.

    fns = [ _handle_begin_end_block,
            _handle_set_statement,
            _handle_set_assignment,
            _handle_expressions_in_comments,
            _handle_data_expressions_in_comments,
            _handle_further_investigation,
            _handle_cast_function_schema ]
    for fn in fns:
        result, state , line= fn(state, line, arguments)
        if result:
            return state, line

    state.further_investigation_required = False
    return state, line


def process_schema(arguments, fdin, fdout):
    state = ParserState()
    for line in fdin:
        state, output_line = process_line(state, line, arguments)
        if state.output:
            fdout.write(output_line)

def check_valid_schema(schema, schemas_in_table_file, schemas_in_schema_file=None):
    if ((schemas_in_schema_file and schema in schemas_in_schema_file) or
        (schemas_in_table_file and schema in schemas_in_table_file)):
        return True
    return False

def check_valid_relname(schema, relname, tables_in_table_file, schemas_in_schema_file=None):
    """
    check if relation is valid (can be from schema level restore)
    """
    if ((schemas_in_schema_file and schema in schemas_in_schema_file) or
       (tables_in_table_file and (schema, relname) in tables_in_table_file)):
        return True
    return False

def get_table_schema_set(filename):
    """
    filename: file with true schema and table name (none escaped), don't strip white space
    on schema and table name in case it's part of the name
    """
    schemas_in_table_file = set()
    tables_in_table_file = set()

    with open(filename) as fd:
        contents = fd.read()
        tables = contents.splitlines()
        for t in tables:
            schema, table = split_fqn(t)
            tables_in_table_file.add((schema, table))
            schemas_in_table_file.add(schema)
    return (schemas_in_table_file, tables_in_table_file)

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
        if "SET search_path = pg_catalog;" == line.strip(): # search_path may just be pg_catalog, as in the case of CASTs
            schema = "pg_catalog"
        else:
            raise Exception('Failed to extract schema name from line %s' % line)
    else:
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
    if idx != -1:
        table = temp[:idx]
        return checkAndRemoveEnclosingDoubleQuote(table)

    idx = temp.rfind("  FROM")
    if idx != -1:
        table = temp[:idx]
        return checkAndRemoveEnclosingDoubleQuote(table)
    else:
        raise Exception('Failed to extract table name from line %s' % line)

def check_dropped_table(line, tables_in_table_file, schemas_in_schema_file, drop_table_expr):
    """
    check if table to drop is valid (can be dropped from schema level restore)
    """
    temp = line[len(drop_table_expr):].strip()[:-1]
    (schema, table) = split_fqn(temp)
    schema = removeEscapingDoubleQuoteInSQLString(checkAndRemoveEnclosingDoubleQuote(schema), False)
    table = removeEscapingDoubleQuoteInSQLString(checkAndRemoveEnclosingDoubleQuote(table), False)
    if (schemas_in_schema_file and schema in schemas_in_schema_file) or ((schema, table) in tables_in_table_file):
        return True
    return False

def process_data(arguments, fdin, fdout):
    schemas_in_table_file = arguments.schemas
    tables_in_table_file = arguments.tables
    change_schema = arguments.change_schema_name
    schemas_in_schema_file = arguments.schemas_in_schema_file

    schema, table, schema_wo_escaping = None, None, None
    output = False
    in_copy = False
    #PYTHON PERFORMANCE IS TRICKY .... THIS CODE IS LIKE THIS BECAUSE ITS FAST
    for line in fdin:
        if output and (line[0] == copy_end_start) and line.startswith(copy_end_expr):
            table = None
            output = False
            in_copy = False
            fdout.write(line)
        elif in_copy:
            pass
        elif (line[0] == set_start) and line.startswith(search_path_expr) and line.endswith(line_end_expr):
            schema = extract_schema(line)
            schema_wo_escaping = removeEscapingDoubleQuoteInSQLString(schema, False)
            if ((schemas_in_table_file and schema_wo_escaping in schemas_in_table_file) or
                (schemas_in_schema_file and schema_wo_escaping in schemas_in_schema_file)):
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
            if (schemas_in_schema_file and schema_wo_escaping in schemas_in_schema_file) or (tables_in_table_file and (schema_wo_escaping, table) in tables_in_table_file):
                output = True
                in_copy = True

        if output:
            fdout.write(line)

def get_schemas_in_schema_file(schema_level_restore_file=None):
    """
    Note: white space in schema and table name is supported now, don't do strip on them
    """
    if not os.path.exists(schema_level_restore_file):
        raise Exception('schema level restore file path %s does not exist' % schema_level_restore_file)
    schemas_in_schema_file = []
    with open(schema_level_restore_file) as fr:
        schema_entries = fr.read()
        schemas_in_schema_file = schema_entries.splitlines()
    return schemas_in_schema_file

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

class Arguments:
    """
    schemas_in_table_file: set of schemas to restore
    tables_in_table_file: set of (schema, table) tuple to restore
    change_schema_name: different schema name to restore
    schemas_in_schema_file: list of schemas to restore all tables under them
    """
    def __init__(self, schemas_in_table_file=None, tables_in_table_file=None):
        self.schemas = schemas_in_table_file
        self.tables = tables_in_table_file
        self.change_schema_name = None
        self.schemas_in_schema_file = None

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

    args = Arguments(schemas, tables)

    if options.change_schema_file:
        args.change_schema_name = get_change_schema_name(options.change_schema_file)

    if options.schema_level_file:
        args.schemas_in_schema_file = get_schemas_in_schema_file(options.schema_level_file)

    if options.master_only:
        process_schema(args, sys.stdin, sys.stdout)
    else:
        process_data(args, sys.stdin, sys.stdout)

