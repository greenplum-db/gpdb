#!/usr/bin/env python

class UniqueIndexViolationCheck:
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
            column_names = column_names[1:-1]
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
