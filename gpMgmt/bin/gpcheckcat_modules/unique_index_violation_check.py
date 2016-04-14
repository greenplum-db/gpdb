#!/usr/bin/env python

class UniqueIndexViolationCheck:
    unique_indexes_query = """
        select table_class.oid as tableoid, index_class.relname as indexname, table_class.relname as tablename, substring(pg_get_indexdef(index_class.oid) from '%#(#"%#"#)%' for '#') as columnnames
        from pg_index
        join pg_class table_class on table_class.oid = pg_index.indrelid
        join pg_class index_class on index_class.oid = pg_index.indexrelid
        left join pg_namespace on pg_namespace.oid = table_class.relnamespace
        left join pg_tablespace on pg_tablespace.oid = index_class.reltablespace
        where table_class.relkind = 'r'::"char" and index_class.relkind = 'i'::"char" and pg_namespace.nspname = 'pg_catalog' and pg_index.indisunique='t';
    """

    def __init__(self):
        self.violated_segments_query = """
            select distinct(gp_segment_id) from (
                (select gp_segment_id, %s
                from gp_dist_random('%s')
                where (%s) is not null
                group by gp_segment_id, %s
                having count(*) > 1)
                union
                (select gp_segment_id, %s
                from %s
                where (%s) is not null
                group by gp_segment_id, %s
                having count(*) > 1)
            ) as violations
        """

    def runCheck(self, db_connection):
        unique_indexes = db_connection.query(self.unique_indexes_query).getresult()
        violations = []

        for (table_oid, index_name, table_name, column_names) in unique_indexes:
            column_names = self.__strip_opclass(column_names)
            sql = self.get_violated_segments_query(table_name, column_names)
            violated_segments = db_connection.query(sql).getresult()
            if violated_segments:
                violations.append(dict(table_oid=table_oid,
                                       table_name=table_name,
                                       index_name=index_name,
                                       column_names=column_names,
                                       violated_segments=[row[0] for row in violated_segments]))

        return violations

    def get_violated_segments_query(self, table_name, column_names):
        return self.violated_segments_query % (
            column_names, table_name, column_names, column_names, column_names, table_name, column_names, column_names
        )

    def __strip_opclass(self, index_arguments):
        column_names_with_opclass = index_arguments.split(', ')
        column_names = [string.split(' ')[0] for string in column_names_with_opclass]
        return ', '.join(column_names)