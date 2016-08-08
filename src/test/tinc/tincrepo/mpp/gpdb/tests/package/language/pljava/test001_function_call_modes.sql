-- Install procedure language PL/JAVA
-- CREATE LANGUAGE pljava;

-- Data preparation
\echo '-- start_ignore'
DROP FUNCTION IF EXISTS func_pljava();
\echo '-- end_ignore'

SELECT set_config('pljava_classpath', 'pljavatest.jar', false);

-- PL/JAVA
CREATE OR REPLACE FUNCTION func_pljava() RETURNS SETOF VARCHAR
AS $$
        pljavatest.PLJavaTest.getNames
$$ LANGUAGE pljava;

SELECT func_pljava();
SELECT * FROM func_pljava();

-- Clean up
DROP FUNCTION IF EXISTS func_pljava();
