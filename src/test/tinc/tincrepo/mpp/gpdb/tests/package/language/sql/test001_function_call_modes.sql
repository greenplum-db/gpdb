-- Install procedure language SQL
-- CREATE LANGUAGE sql;

-- Data preparation
\echo '-- start_ignore'
DROP FUNCTION IF EXISTS func_sql();
DROP TABLE IF EXISTS func_call_modes_data CASCADE;
\echo '-- end_ignore'

CREATE TABLE func_call_modes_data ( c1 INT, c2 INT ) DISTRIBUTED BY (c1);
INSERT INTO func_call_modes_data SELECT i, i FROM generate_series(1,3) i;

-- SQL
CREATE OR REPLACE FUNCTION func_sql() RETURNS SETOF func_call_modes_data
AS $$
        SELECT * FROM func_call_modes_data ORDER BY c1;
$$ LANGUAGE SQL;

SELECT func_sql();
SELECT * FROM func_sql();

-- Clean up
DROP FUNCTION IF EXISTS func_sql();
DROP TABLE IF EXISTS func_call_modes_data CASCADE;
