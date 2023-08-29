#!/usr/bin/env python3

import os
import time

from contextlib import closing

from gppylib import gplog
from gppylib.db import dbconn
from gppylib.mainUtils import ExceptionNoStackTraceNeeded
from gppylib.userinput import ask_yesno


logger = gplog.get_default_logger()

class GpReload:
    def __init__(self, options, args):
        self.table_file = options.table_file
        self.port = options.port
        self.database = options.database
        self.interactive = options.interactive
        self.table_list = []
        self.parent_partition_map = {}

    def validate_table(self, schema_name, table_name):
        conn = dbconn.connect(dbconn.DbURL(dbname=self.database, port=self.port))
        try:
            c = dbconn.querySingleton(conn,
                                    """SELECT count(*)
                                       FROM pg_class, pg_namespace
                                       WHERE pg_namespace.nspname = '{schema}'
                                       AND pg_class.relname = '{table}'
                                       AND pg_class.relnamespace = pg_namespace.oid
                                       AND pg_class.relkind != 'v'""".format(schema=schema_name, table=table_name))
            if not c:
                raise ExceptionNoStackTraceNeeded('Table {schema}.{table} does not exist'
                                                  .format(schema=schema_name, table=table_name))
        finally:
            conn.close()

    def validate_columns(self, schema_name, table_name, sort_column_list):
        columns = []
        conn = dbconn.connect(dbconn.DbURL(dbname=self.database, port=self.port))
        try:
            res = dbconn.query(conn,
                          """SELECT attname
                             FROM pg_attribute
                             WHERE attrelid = (SELECT pg_class.oid
                                               FROM pg_class, pg_namespace
                                               WHERE pg_class.relname = '{table}' AND pg_namespace.nspname = '{schema}'
                                               AND pg_class.relnamespace = pg_namespace.oid
                                               AND pg_class.relkind != 'v')"""
                                 .format(table=table_name, schema=schema_name))
            for cols in res.fetchall():
                columns.append(cols[0].strip())
            for c in sort_column_list:
                if c[0] not in columns:
                    raise ExceptionNoStackTraceNeeded('Table {schema}.{table} does not have column {col}'
                                                       .format(schema=schema_name, table=table_name, col=c[0]))
        finally:
            conn.close()

    def validate_mid_level_partitions(self, schema_name, table_name):
        partition_level, max_level = None, None
        conn = dbconn.connect(dbconn.DbURL(dbname=self.database, port=self.port))
        try:
            parent_schema, parent_table = self.parent_partition_map[(schema_name, table_name)]
            if (parent_schema, parent_table) == (schema_name, table_name):
                return
            try:
                res = dbconn.query(conn,
                                   """ SELECT isleaf from pg_partition_tree('%s.%s') """ % (schema_name, table_name))
            except Exception as e:
                logger.debug('Unable to get the partition information for table %s: (%s)' % (table_name, str(e)))

            for isleaf in res.fetchall():
                if not isleaf[0]:
                    raise Exception('Mid Level partition %s.%s is not supported by gpreload. Please specify only leaf partitions or parent table name' % (schema_name, table_name))
        finally:
            conn.close()

    def validate_options(self):
        if self.table_file is None:
            raise ExceptionNoStackTraceNeeded('Please specify table file')

        if not os.path.exists(self.table_file):
            raise ExceptionNoStackTraceNeeded('Unable to find table file "{table_file}"'.format(table_file=self.table_file))

        if self.database is None:
            raise ExceptionNoStackTraceNeeded('Please specify the correct database')

        if self.port is None:
            if 'PGPORT' not in os.environ:
                raise ExceptionNoStackTraceNeeded('Please specify PGPORT using -p option or set PGPORT in the environment')
            self.port = os.environ['PGPORT']

    def parse_columns(self, columns):
        sort_column_list = []
        for c in columns.split(','):
            toks = c.strip().split()
            if not toks:
                raise Exception('Empty column')
            col = toks[0].strip()
            if len(toks) == 1:
                sort_order = 'asc'
            elif len(toks) == 2:
                sort_order = toks[1].strip()
            else:
                raise Exception('Invalid sort order specified')

            if sort_order and (sort_order != 'asc' and sort_order != 'desc'):
                raise Exception('Invalid sort order {so}'.format(so=sort_order))
            sort_column_list.append((col, sort_order))
        return sort_column_list

    def parse_line(self, line):
        table, sort_columns = line.split(':')
        schema_name, table_name = [t.strip() for t in table.split('.')]
        sort_column_list = self.parse_columns(sort_columns)

        if not schema_name or not table_name:
            raise Exception()

        return schema_name, table_name, sort_column_list

    def validate_table_file(self):
        table_list = []
        with open(self.table_file) as fp:
            for line in fp:
                line = line.strip()
                try:
                    schema_name, table_name, sort_column_list = self.parse_line(line)
                except Exception as e:
                    raise ExceptionNoStackTraceNeeded("Line '{line}' is not formatted correctly: {ex}".format(line=line, ex=e))
                table_list.append((schema_name, table_name, sort_column_list))
        return table_list

    def validate_tables(self):
        for schema_name, table_name, sort_column_list in self.table_list:
            self.validate_mid_level_partitions(schema_name, table_name)
            self.validate_table(schema_name, table_name)
            self.validate_columns(schema_name, table_name, sort_column_list)

    def get_row_count(self, table_name):
        with closing(dbconn.connect(dbconn.DbURL(dbname=self.database, port=self.port))) as conn:
            c = dbconn.querySingleton(conn, 'SELECT count(*) FROM {table}'.format(table=table_name))
        return c

    def check_indexes(self, schema_name, table_name):
        with closing(dbconn.connect(dbconn.DbURL(dbname=self.database, port=self.port))) as conn:
            c = dbconn.querySingleton(conn, """SELECT count(*)
                                             FROM pg_index
                                             WHERE indrelid = (SELECT pg_class.oid
                                                               FROM pg_class, pg_namespace
                                                               WHERE pg_class.relname='{table}' AND pg_namespace.nspname='{schema}' AND pg_class.relnamespace = pg_namespace.oid)""".format(table=table_name, schema=schema_name))
            if c != 0:
                if self.interactive:
                    return ask_yesno(None,
                                    'Table {schema}.{table} has indexes. This might slow down table reload. Do you still want to continue ?'
                                    .format(schema=schema_name, table=table_name),
                                    'N')
        return True

    def get_table_size(self, schema_name, table_name):
        with closing(dbconn.connect(dbconn.DbURL(dbname=self.database, port=self.port))) as conn:
            size = dbconn.querySingleton(conn,
                                       """SELECT pg_size_pretty(pg_relation_size('{schema}.{table}'))"""
                                       .format(schema=schema_name, table=table_name))
        return size

    def get_parent_partitions(self):
        with closing(dbconn.connect(dbconn.DbURL(dbname=self.database, port=self.port))) as conn:
            for schema, table, col_list in self.table_list:
                PARENT_PARTITION_TABLENAME = """SELECT nspname, relname
                                                FROM pg_catalog.pg_class AS c
                                                JOIN pg_catalog.pg_namespace AS ns
                                                ON c.relnamespace = ns.oid
                                                WHERE c.oid = pg_partition_root('%s.%s')
                                                """ % (schema, table)
                res = dbconn.query(conn, PARENT_PARTITION_TABLENAME)
                for r in res:
                    self.parent_partition_map[(schema, table)] = (r[0], r[1]) 

                if (schema, table) not in self.parent_partition_map:
                    self.parent_partition_map[(schema, table)] = (schema, table)

        return self.parent_partition_map

    def reload_tables(self):
        conn =  dbconn.connect(dbconn.DbURL(dbname=self.database, port=self.port))
        try:
            conn.commit()   #Commit the implicit transaction started by connect
            for schema_name, table_name, sort_column_list in self.table_list:
                logger.info('Starting reload for table {schema}.{table}'.format(schema=schema_name, table=table_name))
                logger.info('Table {schema}.{table} has {rows} rows and {size} size'
                        .format(schema=schema_name, table=table_name,
                         rows=self.get_row_count('%s.%s' % (schema_name, table_name)),
                         size=self.get_table_size(schema_name, table_name)))
                if not self.check_indexes(schema_name, table_name):
                    logger.info('Skipping reload for {schema}.{table}'.format(schema=schema_name, table=table_name))
                    continue
                start = time.time()
                dbconn.execSQL(conn, 'BEGIN')
                dbconn.execSQL(conn, """CREATE TEMP TABLE temp_{table} AS SELECT * FROM {schema}.{table}"""
                                     .format(schema=schema_name, table=table_name))
                temp_row_count = dbconn.querySingleton(conn, """SELECT count(*) FROM temp_{table}""".format(table=table_name))
                table_row_count = dbconn.querySingleton(conn, """SELECT count(*) from {schema}.{table}"""
                                                                    .format(table=table_name, schema=schema_name))
                if temp_row_count != table_row_count:
                    raise Exception('Row count for temp table(%s) does not match(%s)' % (temp_row_count, table_row_count))
                dbconn.execSQL(conn, 'TRUNCATE TABLE {schema}.{table}'.format(schema=schema_name, table=table_name))
                sort_order = ['%s %s' % (c[0], c[1]) for c in sort_column_list]
                parent_schema_name, parent_table_name = self.parent_partition_map[(schema_name, table_name)]
                dbconn.execSQL(conn, """INSERT INTO {parent_schema}.{parent_table} SELECT * FROM temp_{table} ORDER BY {column_list}"""
                                     .format(parent_schema=parent_schema_name, parent_table=parent_table_name, 
                                             table=table_name, column_list=','.join(sort_order)))
                conn.commit()
                end = time.time()
                logger.info('Finished reload for table {schema}.{table} in time {sec} seconds'
                            .format(schema=schema_name, table=table_name, sec=(end-start)))
        finally:
            conn.close()

    def run(self):
        logger.warning("gpreload is deprecated and may be removed in a future release. consider using ALTER TABLE REPACK BY statements instead.")
        self.validate_options()
        logger.info('Validating table file {table_file}'.format(table_file=self.table_file))
        self.table_list = self.validate_table_file()
        logger.info('Obtaining parent partitions')
        self.get_parent_partitions()
        logger.info('Validating tables')
        self.validate_tables()
        logger.info('Reloading tables')
        self.reload_tables()

    def cleanup(self):
        pass

