#!/usr/bin/env python

from gppylib.gpparseopts import OptParser, OptChecker
import sys

search_path_expr = 'SET search_path = '
set_start = 'S'
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

comment_start_expr = '-- '
comment_expr = '-- Name: '
comment_data_expr_a = '-- Data: '
comment_data_expr_b = '-- Data for Name: '
len_start_comment_expr = len(comment_start_expr)

def get_table_info(line):
    try:
        temp = line[len_start_comment_expr:]
        tokens = temp.strip().split(';')
        name = tokens[0].split(':')[1].strip()
        type = tokens[1].split(':')[1].strip()
        schema = tokens[2].split(':')[1].strip()
    except:
        return (None, None, None)
    return (name, type, schema) 

def process_schema(dump_schemas, dump_tables, fdin, fdout, change_schema):
    """
    Filter the dump file line by line from restore
    dump_schemas: set of schemas to restore
    dump_tables: set of (schema, table) tuple to restore
    fdin: stdin from dump file
    fdout: to write filtered content to stdout
    change_schema: different schema name to restore
    """

    schema, table = None, None
    line_buff = ''

    # to help decide whether or not to filter out
    output = False

    # to help exclude SET clause within a function's ddl statement
    function_ddl = False

    further_investigation_required = False
    search_path = True
    passedDropSchemaSection = False
    for line in fdin:
        if search_path and (line[0] == set_start) and line.startswith(search_path_expr):
            further_investigation_required = False
            schema = extract_schema(line)
            if schema in dump_schemas:
                if change_schema:
                    line = line.replace(schema, change_schema)
                output = True
                search_path = False
            else:
                output = False
        elif (line[0] == set_start) and line.startswith(set_expr) and not function_ddl:
            output = True
        elif (line[0] == drop_start) and line.startswith(drop_expr):
            if line.startswith('DROP TABLE') or line.startswith('DROP EXTERNAL TABLE'):
                if passedDropSchemaSection:
                    output = False
                else:
                    output = check_dropped_table(line, dump_tables)
            else:
                output = False
        elif line[:3] == comment_start_expr and line.startswith(comment_expr):
            # Parse the line using get_table_info for SCHEMA relation type as well,
            # if type is SCHEMA, then the value of name returned is schema's name, and returned schema is represented by '-'
            name, type, schema = get_table_info(line)
            output = False
            function_ddl = False
            passedDropSchemaSection = True
            if type in ['TABLE', 'EXTERNAL TABLE']:
                further_investigation_required = False
                output = check_valid_table(schema, name, dump_tables)
                if output:
                    search_path = True
            elif type in ['CONSTRAINT']:
                further_investigation_required = True
                if schema in dump_schemas:
                    line_buff = line 
            elif type in ['ACL']:
                output = check_valid_table(schema, name, dump_tables)
                if output:
                    search_path = True
            elif type in ['SCHEMA']:
                output = check_valid_schema(name, dump_schemas)
                if output:
                    search_path = True
            elif type in ['FUNCTION']:
                function_ddl = True
        elif (line[:3] == comment_start_expr) and (line.startswith(comment_data_expr_a) or line.startswith(comment_data_expr_b)):
            passedDropSchemaSection = True
            further_investigation_required = False
            name, type, schema = get_table_info(line)
            if type == 'TABLE DATA':
                output = check_valid_table(schema, name, dump_tables)
                if output:
                    search_path = True
            else:
                output = False  
        elif further_investigation_required:
            if line.startswith('ALTER TABLE'):
                further_investigation_required = False
                name = line.split()[-1]
                output = check_valid_table(schema, name, dump_tables)
                if output:
                    if line_buff:
                        fdout.write(line_buff)
                        line_buff = ''
                    search_path = True
        else:
            further_investigation_required = False


        if output:
            fdout.write(line)

def check_valid_schema(name, dump_schemas):
    if name in dump_schemas:
        output = True
    else:
        output = False
    return output

def check_valid_table(schema, name, dump_tables):
    """
    check if table is valid (can be from schema level restore)
    """

    if (schema, name) in dump_tables or (schema, '*') in dump_tables:
        output = True
    else:
        output = False
    return output

def get_table_schema_set(filename):
    dump_schemas = set()
    dump_tables = set()

    with open(filename) as fd:
        contents = fd.read()
        tables = contents.splitlines()
        for t in tables:
            parts = t.split('.')
            if len(parts) != 2:
                raise Exception("Bad table in filter list")
            schema = parts[0].strip()
            table = parts[1].strip()
            dump_tables.add((schema, table))
            dump_schemas.add(schema)

    return (dump_schemas, dump_tables)

def extract_schema(line):
    temp = line[len_search_path_expr:]
    idx = temp.find(",")
    if idx == -1:
        return None
    schema = temp[:idx]
    return schema.strip('"')

def extract_table(line):
    temp = line[len_copy_expr:]
    idx = temp.find(" ")
    if idx == -1:
        return None
    table = temp[:idx]
    return table

def check_dropped_table(line, dump_tables):
    """
    check if table to drop is valid (can be dropped from schema level restore)
    """
    temp = line.split()[-1][:-1]
    (schema, table) = temp.split('.')
    if (schema, table) in dump_tables or (schema, '*') in dump_tables:
        return True
    return False

def process_data(dump_schemas, dump_tables, fdin, fdout, change_schema):
    schema, table = None, None
    output = False
    #PYTHON PERFORMANCE IS TRICKY .... THIS CODE IS LIKE THIS BECAUSE ITS FAST
    for line in fdin:
        if (line[0] == set_start) and line.startswith(search_path_expr):
            schema = extract_schema(line)
            if schema and schema in dump_schemas:
                if change_schema:
                    line = line.replace(schema, change_schema)
                fdout.write(line)
        elif (line[0] == copy_start) and line.startswith(copy_expr) and line.endswith(copy_expr_end):
            table = extract_table(line)
            table = table.strip('"')
            if table and (schema, table) in dump_tables or (schema, '*') in dump_tables:
                output = True
        elif output and (line[0] == copy_end_start) and line.startswith(copy_end_expr):
            table = None
            output = False
            fdout.write(line)

        if output:
            fdout.write(line)



if __name__ == "__main__":
    parser = OptParser(option_class=OptChecker)
    parser.remove_option('-h')
    parser.add_option('-h', '-?', '--help', action='store_true')
    parser.add_option('-t', '--tablefile', type='string', default=None)
    parser.add_option('-m', '--master_only', action='store_true')
    parser.add_option('-c', '--change_schema', type='string', default=None)
    (options, args) = parser.parse_args()
    if not options.tablefile:
        raise Exception('-t table file name has to be specified')
    (schemas, tables) = get_table_schema_set(options.tablefile)
    if options.master_only:
        process_schema(schemas, tables, sys.stdin, sys.stdout, options.change_schema)
    else:
        process_data(schemas, tables, sys.stdin, sys.stdout, options.change_schema)

