#!/usr/bin/env python
from pygresql import pg
from gppylib.operations.backup_utils import escapeDoubleQuoteInSQLString

class UniqueIndexViolationCheck(object):

    # in order to check for index duplication , we need to create a function and a type
    # we chose  the gp_toolkit schema because we do not want to create objects in the default/public schema
    schema_to_use = 'gp_toolkit'
    readindex_function_name = 'readindex'
    readindex_type_name= 'readindex_type'

    unique_indexes_query = """
        select table_oid, index_name, table_name, array_agg(attname) as column_names
        from pg_attribute, (
            select pg_index.indrelid as table_oid, index_class.relname as index_name, table_class.relname as table_name, unnest(pg_index.indkey) as column_index
            from pg_index, pg_class index_class, pg_class table_class
            where pg_index.indisunique='t'
            and index_class.relnamespace = (select oid from pg_namespace where nspname = 'pg_catalog')
            and index_class.relkind = 'i'
            and index_class.oid = pg_index.indexrelid
            and table_class.oid = pg_index.indrelid
        ) as unique_catalog_index_columns
        where attnum = column_index
        and attrelid = table_oid
        group by table_oid, index_name, table_name;
    """

    check_gp_toolkit_schema_exists_sql = "select nspname from pg_namespace where nspname='{0}';".format(schema_to_use)

    get_gp_toolkit_oid_sql = """
        select oid from pg_namespace where nspname ='%s'
        """ % schema_to_use

    # check if the function readindex(oid, bool) exists in the schema
    check_readindex_function_exists_sql = """
            select oid from pg_proc where proname='{0}' and
            proargtypes[0]='oid'::regtype and proargtypes[1]='bool'::regtype
            and pronargs=2 and pronamespace=({1});
            """.format(readindex_function_name, get_gp_toolkit_oid_sql)

    check_readindex_type_exists_sql = """
             SELECT oid FROM pg_type WHERE typname='{0}' and typnamespace=({1});
            """.format(readindex_type_name, get_gp_toolkit_oid_sql)

    def run_check_table_duplication(self, db_connection):
        violated_segments_query = """
            select distinct(gp_segment_id) from (
            (select gp_segment_id, {0}
            from gp_dist_random('{1}')
            where ({0}) is not null
            group by gp_segment_id, {0}
            having count(*) > 1)
            union
            (select gp_segment_id, {0}
            from {2}
            where ({0}) is not null
            group by gp_segment_id, {0}
            having count(*) > 1)
            ) as violations
                                          """
        return self.__get_violations(db_connection, violated_segments_query,
                                         self.__get_violated_segments_query_table_duplication)

    def run_check_index_duplication(self, db_connection):
        # make sure that the following conditions are met before running the check
        #   1 gp_toolkit schema exists
        #   2 there is no function named readindex with args (oid,bool) in the schema gp_toolkit
        #   3 there is no type named readindex_type in the schema gp_toolkit
        # if all the checks pass, create the type and the function and drop them at the end

        self.check_gp_toolkit_schema_exists(db_connection)
        self.check_readindex_function_exists(db_connection)
        self.check_readindex_type_exists(db_connection)
        self.__create_readindex_function_and_type_in_sql(db_connection)

        # The following query returns master and segments that have duplicate index references:
        # Example violation output on master(duplication in hctid column):
        # select * from readindex('pg_compression_compname_index'::regclass) as (ictid tid, hctid tid, aotid text,istatus text, hstatus text);
        #
        # ictid | hctid | aotid | istatus |           hstatus
        # -------+-------+-------+---------+-----------------------------
        # (1,1) | (0,4) | N/A   | USED    | XMIN_COMITTTED XMAX_INVALID
        # (1,2) | (0,4) | N/A   | USED    | XMIN_COMITTTED XMAX_INVALID

        violated_segments_query = """
            SELECT -1 AS sid, count(hctid) FROM {0}.{1}('{2}'::regclass, true) GROUP BY hctid HAVING count(hctid) > 1

            UNION

            SELECT sid , count(sid) FROM
              (SELECT sid, (res).hctid FROM
                (SELECT gp_segment_id AS sid , {0}.{1}('{2}'::regclass, true)
                AS res FROM gp_dist_random('gp_id')) x) foo

            GROUP BY (sid,hctid) HAVING count(sid)>1;
            """

        violations = self.__get_violations(db_connection, violated_segments_query,
                                           self.__get_violated_segments_query_index_duplication)

        self.__drop_readindex_type_and_function(db_connection)

        return violations

    def check_gp_toolkit_schema_exists(self, db_connection):
        gp_toolkit_schema_exists = db_connection.query(self.check_gp_toolkit_schema_exists_sql).getresult()

        if not gp_toolkit_schema_exists:
            raise Exception("Schema %s does not exist in database '%s'. "
                            "Please contact support to create this schema and re-run gpcheckcat."
                            % (self.schema_to_use, db_connection.db))

    def check_readindex_type_exists(self, db_connection):
        readindex_return_type_exists = db_connection.query(self.check_readindex_type_exists_sql).getresult()

        # this check creates a type with name readindex_type, so we throw if one already exists
        if readindex_return_type_exists:
            raise Exception("Type %s already exists in schema %s in database '%s'. "
                            "Please drop the type and re-run gpcheckcat."
                            % (self.readindex_type_name, self.schema_to_use, db_connection.db))

    def check_readindex_function_exists(self, db_connection):
        readindex_function_exists = db_connection.query(self.check_readindex_function_exists_sql).getresult()

        # this check creates a function with name readindex, so we throw if one already exists
        if readindex_function_exists:
            raise Exception("Function %s(oid,bool) already exists in schema %s in database '%s'. "
                            "Please drop the function and re-run gpcheckcat." %
                            (self.readindex_function_name, self.schema_to_use, db_connection.db))


    def __get_violations(self, db_connection, violated_segments_query, get_query_function):
        unique_indexes = db_connection.query(self.unique_indexes_query).getresult()
        violations = []
        for (table_oid, index_name, table_name, column_names) in unique_indexes:
            column_names = column_names[1:-1]
            sql = get_query_function(violated_segments_query, table_name, column_names, index_name)
            violated_segments = db_connection.query(sql).getresult()
            if violated_segments:
                violations.append(dict(table_oid=table_oid,
                                       table_name=table_name,
                                       index_name=index_name,
                                       column_names=column_names,
                                       violated_segments=[row[0] for row in violated_segments]))
        return violations

    def __get_violated_segments_query_table_duplication(self, violated_segments_query, table_name, column_names,
                                                        index_name):
        return violated_segments_query.format(column_names, pg.escape_string(table_name),
                                              escapeDoubleQuoteInSQLString(table_name))

    def __get_violated_segments_query_index_duplication(self, violated_segments_query, table_name, column_names,
                                                        index_name):
        return violated_segments_query.format(self.schema_to_use, self.readindex_function_name,
                                              pg.escape_string(index_name))

    def __create_readindex_function_and_type_in_sql(self, db_connection):
        create_readindex_type_sql = """
        CREATE TYPE {0}.{1} AS (ictid tid, hctid tid, aotid text,istatus text, hstatus text);
        """.format(self.schema_to_use, self.readindex_type_name)

        create_readindex_function_sql = """
        CREATE OR REPLACE FUNCTION {0}.{1}(oid, bool) RETURNS SETOF {0}.{2} AS '$libdir/indexscan', 'readindex' LANGUAGE C STRICT;
        """.format(self.schema_to_use, self.readindex_function_name, self.readindex_type_name)

        db_connection.query(create_readindex_type_sql)
        db_connection.query(create_readindex_function_sql)

    def __drop_readindex_type_and_function(self, db_connection):
        drop_readindex_function_sql = """
        DROP FUNCTION IF EXISTS {0}.{1}(oid,bool);
        """.format(self.schema_to_use, self.readindex_function_name)

        drop_readindex_type_sql = """
        DROP TYPE IF EXISTS {0}.{1} cascade;
        """.format(self.schema_to_use, self.readindex_type_name)

        db_connection.query(drop_readindex_function_sql)
        db_connection.query(drop_readindex_type_sql)

