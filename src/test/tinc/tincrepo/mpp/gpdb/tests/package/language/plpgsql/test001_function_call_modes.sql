-- Install procedure language PL/PGSQL
-- CREATE LANGUAGE plpgsql;

-- Data preparation
\echo '-- start_ignore'
DROP FUNCTION IF EXISTS func_plpgsql();
DROP TABLE IF EXISTS func_call_modes_data CASCADE;
\echo '-- end_ignore'

CREATE TABLE func_call_modes_data ( c1 INT, c2 INT ) DISTRIBUTED BY (c1);
INSERT INTO func_call_modes_data SELECT i, i FROM generate_series(1,3) i;

-- PL/PGSQL
CREATE OR REPLACE FUNCTION func_plpgsql() RETURNS SETOF func_call_modes_data
AS $$
        DECLARE r
                func_call_modes_data%rowtype;
        BEGIN
                FOR r IN SELECT * FROM func_call_modes_data ORDER BY c1
                LOOP
                RETURN NEXT r;
                END LOOP;
        RETURN;
END
$$ LANGUAGE PLPGSQL;

SELECT func_plpgsql();

SELECT * FROM func_plpgsql();

-- Clean up
DROP FUNCTION IF EXISTS func_plpgsql();
DROP TABLE IF EXISTS func_call_modes_data CASCADE;
