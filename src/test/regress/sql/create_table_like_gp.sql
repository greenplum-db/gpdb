-- AO/AOCS
CREATE TABLE t_ao (a integer, b text) WITH (appendonly=true, orientation=column);
CREATE TABLE t_ao_enc (a integer, b text ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768)) WITH (appendonly=true, orientation=column);

CREATE TABLE t_ao_a (LIKE t_ao INCLUDING ALL);
CREATE TABLE t_ao_b (LIKE t_ao INCLUDING STORAGE);
CREATE TABLE t_ao_c (LIKE t_ao); -- Should create a heap table

CREATE TABLE t_ao_enc_a (LIKE t_ao_enc INCLUDING STORAGE);

-- Verify default_table_access_method GUC doesn't get used
SET default_table_access_method = ao_row;
CREATE TABLE t_ao_d (LIKE t_ao INCLUDING ALL);
RESET gp_default_storage_options;

-- Verify created tables and attributes
select relname, reloptions from pg_class where relname LIKE 't_ao%' order by relname;

SELECT
	c.relname,
	a.attnum,
	a.filenum,
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
	c.relkind,
	f.ftoptions
FROM
	pg_catalog.pg_class c
		LEFT OUTER JOIN pg_catalog.pg_foreign_table f ON (c.oid = f.ftrelid)
WHERE
	c.relname LIKE 't_ext%';

-- TEMP TABLE WITH COMMENTS
-- More details can be found at https://github.com/greenplum-db/gpdb/issues/14649
CREATE TABLE t_comments_a (a integer);
COMMENT ON COLUMN t_comments_a.a IS 'Airflow';
CREATE TEMPORARY TABLE t_comments_b (LIKE t_comments_a INCLUDING COMMENTS);

-- Verify the copied comment
SELECT
	c.column_name,
	pgd.description
FROM pg_catalog.pg_statio_all_tables st
		inner join pg_catalog.pg_description pgd on (pgd.objoid=st.relid)
		inner join information_schema.columns c on (pgd.objsubid=c.ordinal_position and c.table_schema=st.schemaname and c.table_name=st.relname)
WHERE c.table_name = 't_comments_b';

DROP TABLE t_comments_a;
DROP TABLE t_comments_b;
