-- Install procedure language PL/PYTHONU
-- CREATE LANGUAGE plpythonu;

-- Data preparation
\echo '-- start_ignore'
DROP FUNCTION IF EXISTS func_plpythonu();
DROP TABLE IF EXISTS func_call_modes_data CASCADE;
\echo '-- end_ignore'

CREATE TABLE func_call_modes_data ( c1 INT, c2 INT ) DISTRIBUTED BY (c1);
INSERT INTO func_call_modes_data SELECT i, i FROM generate_series(1,3) i;


-- PL/PYTHONU
CREATE OR REPLACE FUNCTION func_plpythonu() RETURNS SETOF func_call_modes_data
AS $$
        return plpy.execute('SELECT * FROM func_call_modes_data ORDER BY c1;');
$$ LANGUAGE plpythonu;

SELECT func_plpythonu();
SELECT * FROM func_plpythonu();

-- Clean up
DROP FUNCTION IF EXISTS func_plpythonu();
DROP TABLE IF EXISTS func_call_modes_data CASCADE;
