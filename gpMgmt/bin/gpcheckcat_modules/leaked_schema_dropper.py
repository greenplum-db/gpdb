from gppylib.operations.backup_utils import escapeDoubleQuoteInSQLString

class LeakedSchemaDropper:

    # This query does a union of all the leaked temp schemas on the master as well as all the segments.
    # The first part of the query uses gp_dist_random which gets the leaked schemas from only the segments
    # The second part of the query gets the leaked temp schemas from just the master
    # The simpler form of this query that pushed the union into the
    # inner select does not run correctly on 3.2.x
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
        WHERE x.sess_id is null
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
        WHERE x.sess_id is null
    """

    def __get_leaked_schemas(self, db_connection):
        leaked_schemas = db_connection.query(self.leaked_schema_query)

        if not leaked_schemas:
            return []

        return [row[0] for row in leaked_schemas.getresult() if row[0]]

    def drop_leaked_schemas(self, db_connection):
        leaked_schemas = self.__get_leaked_schemas(db_connection)
        for leaked_schema in leaked_schemas:
            escaped_schema_name = escapeDoubleQuoteInSQLString(leaked_schema)
            db_connection.query('DROP SCHEMA IF EXISTS %s CASCADE;' % (escaped_schema_name))
        return leaked_schemas
