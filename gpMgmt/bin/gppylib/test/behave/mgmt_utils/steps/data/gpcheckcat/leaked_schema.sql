SELECT distinct nspname as schema
    FROM (
      SELECT nspname, replace(nspname, 'pg_temp_','')::int as sess_id
      FROM   gp_dist_random('pg_namespace')
      WHERE  nspname ~ '^pg_temp_[0-9]+'
    ) n LEFT OUTER JOIN pg_stat_activity x using (sess_id)
    WHERE x.sess_id is null
    UNION
    SELECT nspname as schema
    FROM (
      SELECT nspname, replace(nspname, 'pg_temp_','')::int as sess_id
      FROM   pg_namespace
      WHERE  nspname ~ '^pg_temp_[0-9]+'
    ) n LEFT OUTER JOIN pg_stat_activity x using (sess_id)
    WHERE x.sess_id is null;
