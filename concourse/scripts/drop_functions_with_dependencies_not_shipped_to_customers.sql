CREATE TEMP TABLE probins_not_shipped AS (
  SELECT probin
    FROM pg_catalog.pg_proc
   WHERE probin IS NOT NULL
     AND probin NOT LIKE '$libdir%'
) DISTRIBUTED RANDOMLY;

INSERT INTO probins_not_shipped
  VALUES ('$libdir/gp_replica_check')
       , ('$libdir/gpformatter.so')
       , ('$libdir/gpextprotocol.so')
       , ('$libdir/gppc_test')
       , ('$libdir/gppc_demo')
       , ('$libdir/tabfunc_gppc_demo');

CREATE TEMP TABLE functions_to_be_dropped AS (
  SELECT proname
       , pg_catalog.pg_get_function_arguments(oid) AS args
    FROM pg_catalog.pg_proc
   WHERE probin IN (SELECT * FROM probins_not_shipped)
) DISTRIBUTED RANDOMLY;

\echo "List all the functions that we plan on dropping:"
SELECT * FROM functions_to_be_dropped;

-- The functions that have probin = $libdir/gp_replica_check are part of an extension,
-- therefore DROP the extension explicitly
DROP EXTENSION gp_replica_check;

CREATE OR REPLACE FUNCTION drop_functions_with_dependencies_not_shipped_to_customers() RETURNS void AS $$
DECLARE
       function RECORD;
       sql text;
BEGIN
       FOR function IN
            SELECT * FROM functions_to_be_dropped
       LOOP
            sql := 'DROP FUNCTION IF EXISTS ' || quote_ident(function.proname) || '(' || function.args || ') CASCADE';
            RAISE NOTICE '%', sql;
            EXECUTE sql;
       END LOOP;
       RETURN;
END;
$$ LANGUAGE plpgsql;

SELECT drop_functions_with_dependencies_not_shipped_to_customers();
DROP FUNCTION drop_functions_with_dependencies_not_shipped_to_customers();

\echo "Now there should be no functions left."

SELECT proname
  FROM pg_catalog.pg_proc
 WHERE probin IN (SELECT * FROM probins_not_shipped);
