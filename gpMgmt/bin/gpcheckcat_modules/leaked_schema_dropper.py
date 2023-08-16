from gppylib.utils import escapeDoubleQuoteInSQLString

class LeakedSchemaDropper:

    # This query does a union of all the leaked temp schemas on the coordinator as well as all the segments.
    # The first part of the query uses gp_dist_random which gets the leaked schemas from only the segments
    # The second part of the query gets the leaked temp schemas from just the coordinator
    # The simpler form of this query that pushed the union into the
    # inner select does not run correctly on 3.2.x
    # Also note autovacuum lancher and worker will not generate temp namespace
    leaked_schema_query = """
        SELECT distinct nspname as schema
        FROM (
          SELECT nspname, replace(nspname, 'pg_temp_','')::int as sess_id
          FROM   gp_dist_random('pg_namespace')
          WHERE  nspname ~ '^pg_temp_[0-9]+'
          UNION ALL
          SELECT nspname, replace(nspname, 'pg_toast_temp_','')::int as sess_id
          FROM   gp_dist_random('pg_namespace')
          WHERE  nspname ~ '^pg_toast_temp_[0-9]+'
        ) n LEFT OUTER JOIN pg_stat_activity x using (sess_id)
        WHERE x.sess_id is null OR x.backend_type like 'autovacuum%'
        UNION
        SELECT nspname as schema
        FROM (
          SELECT nspname, replace(nspname, 'pg_temp_','')::int as sess_id
          FROM   pg_namespace
          WHERE  nspname ~ '^pg_temp_[0-9]+'
          UNION ALL
          SELECT nspname, replace(nspname, 'pg_toast_temp_','')::int as sess_id
          FROM   pg_namespace
          WHERE  nspname ~ '^pg_toast_temp_[0-9]+'
        ) n LEFT OUTER JOIN pg_stat_activity x using (sess_id)
        WHERE x.sess_id is null OR x.backend_type like 'autovacuum%'
    """

    def __get_leaked_schemas(self, db_connection):
        with db_connection.cursor() as curs:
            curs.execute(self.leaked_schema_query)
            leaked_schemas = curs.fetchall()

            if not leaked_schemas:
                return []

            return [row[0] for row in leaked_schemas if row[0]]

    def drop_leaked_schemas(self, db_connection):
        leaked_schemas = self.__get_leaked_schemas(db_connection)
        for leaked_schema in leaked_schemas:
            escaped_schema_name = escapeDoubleQuoteInSQLString(leaked_schema)
            with db_connection.cursor() as curs:
                curs.execute('DROP SCHEMA IF EXISTS %s CASCADE;' % (escaped_schema_name))
        return leaked_schemas
