-- Install procedure language PL/JAVAU
-- CREATE LANGUAGE pljavau;

-- Data preparation
\echo '-- start_ignore'
DROP FUNCTION IF EXISTS func_pljavau();
\echo '-- end_ignore'

SELECT set_config('pljava_classpath', 'pljavatest.jar', false);

-- PL/JAVAU
CREATE OR REPLACE FUNCTION func_pljavau() RETURNS SETOF VARCHAR
AS $$
        pljavatest.PLJavaTest.getNames
$$ LANGUAGE pljavau;

SELECT func_pljavau();
SELECT * FROM func_pljavau();

-- Clean up
DROP FUNCTION IF EXISTS func_pljavau();
