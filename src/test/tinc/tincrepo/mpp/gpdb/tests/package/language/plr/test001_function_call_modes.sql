-- Install procedure language PL/R
-- CREATE LANGUAGE plr;

-- Data preparation
\echo '-- start_ignore'
DROP FUNCTION IF EXISTS func_plr();
DROP TABLE IF EXISTS func_call_modes_data CASCADE;
\echo '-- end_ignore'

CREATE TABLE func_call_modes_data ( c1 INT, c2 INT ) DISTRIBUTED BY (c1);
INSERT INTO func_call_modes_data SELECT i, i FROM generate_series(1,3) i;

-- PL/R
CREATE OR REPLACE FUNCTION func_plr() RETURNS SETOF func_call_modes_data
AS $$
        pg.spi.exec('SELECT * FROM func_call_modes_data ORDER BY c1;');
$$ LANGUAGE plr;

SELECT func_plr();
SELECT * FROM func_plr();

-- Clean up
DROP FUNCTION IF EXISTS func_plr();
DROP TABLE IF EXISTS func_call_modes_data CASCADE;
