--SELECT catalog_name, schema_name, schema_owner FROM information_schema.schemata;
SELECT catalog_name, schema_name, schema_owner FROM information_schema.schemata
  WHERE schema_name NOT LIKE 'pg_toast_temp%' ORDER BY schema_name;

SELECT schema_name FROM information_schema.schemata WHERE schema_name = 'gptransfer_schema';
