-- AO/AOCS
CREATE TABLE t_ao (a integer, b text) WITH (appendonly=true, orientation=column);
CREATE TABLE t_ao_enc (a integer, b text ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768)) WITH (appendonly=true, orientation=column);

CREATE TABLE t_ao_a (LIKE t_ao INCLUDING ALL);
CREATE TABLE t_ao_b (LIKE t_ao INCLUDING STORAGE);
CREATE TABLE t_ao_c (LIKE t_ao); -- Should create a heap table

CREATE TABLE t_ao_enc_a (LIKE t_ao_enc INCLUDING STORAGE);

-- Verify gp_default_storage_options GUC doesn't get used
SET gp_default_storage_options = "appendonly=true, orientation=row";
CREATE TABLE t_ao_d (LIKE t_ao INCLUDING ALL);
RESET gp_default_storage_options;

-- Verify created tables and attributes
SELECT
	c.relname,
	c.relstorage,
	a.columnstore,
	a.compresstype,
	a.compresslevel
FROM
	pg_catalog.pg_class c
		LEFT OUTER JOIN pg_catalog.pg_appendonly a ON (c.oid = a.relid)
WHERE
	c.relname LIKE 't_ao%';

SELECT
	c.relname,
	a.attnum,
	a.attoptions
FROM
	pg_catalog.pg_class c
		JOIN pg_catalog.pg_attribute_encoding a ON (a.attrelid = c.oid)
WHERE
	c.relname like 't_ao_enc%';

-- EXTERNAL TABLE
CREATE EXTERNAL TABLE t_ext (a integer) LOCATION ('file://127.0.0.1/tmp/foo') FORMAT 'text';
CREATE EXTERNAL TABLE t_ext_a (LIKE t_ext INCLUDING ALL) LOCATION ('file://127.0.0.1/tmp/foo') FORMAT 'text';
CREATE EXTERNAL TABLE t_ext_b (LIKE t_ext) LOCATION ('file://127.0.0.1/tmp/foo') FORMAT 'text';

-- Verify that an external table can be dropped and then recreated in consecutive attempts
CREATE OR REPLACE FUNCTION drop_and_recreate_external_table()
	RETURNS void
	LANGUAGE plpgsql
	VOLATILE
AS $function$
DECLARE
BEGIN
DROP EXTERNAL TABLE IF EXISTS t_ext_r;
CREATE EXTERNAL TABLE t_ext_r (
	name varchar
)
LOCATION ('GPFDIST://127.0.0.1/tmp/dummy') ON ALL
FORMAT 'CSV' ( delimiter ' ' null '' escape '"' quote '"' )
ENCODING 'UTF8';
END;
$function$;

do $$
begin
  for i in 1..5 loop
	PERFORM drop_and_recreate_external_table();
  end loop;
end;
$$;

-- Verify created tables
SELECT
	c.relname,
	c.relstorage,
	f.ftoptions
FROM
	pg_catalog.pg_class c
		LEFT OUTER JOIN pg_catalog.pg_foreign_table f ON (c.oid = f.ftrelid)
WHERE
	c.relname LIKE 't_ext%';
