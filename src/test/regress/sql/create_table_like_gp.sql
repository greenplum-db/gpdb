-- Verify the copied STORAGE, ENCODING, RELOPT, and AM options

-- create a source heap table
CREATE TABLE ctlt_heap (a text) WITH (fillfactor = 50);
ALTER TABLE ctlt_heap ALTER COLUMN a SET STORAGE MAIN;
-- create table like the heap table
CREATE TABLE like_heap_storage (LIKE ctlt_heap INCLUDING STORAGE); -- succeeds
CREATE TABLE like_heap_encoding (LIKE ctlt_heap INCLUDING ENCODING); -- succeeds, but ignored
CREATE TABLE like_heap_relopt (LIKE ctlt_heap INCLUDING RELOPT); -- succeeds
SET default_table_access_method = ao_row;
CREATE TABLE like_heap_am (LIKE ctlt_heap INCLUDING AM); -- succeeds
CREATE TABLE like_heap_am_relopt (LIKE ctlt_heap INCLUDING AM INCLUDING relopt); -- succeeds
CREATE TABLE like_heap_all (LIKE ctlt_heap INCLUDING ALL); -- succeeds
CREATE TABLE like_heap_all_use_heap (LIKE ctlt_heap INCLUDING ALL) USING heap; -- error, AM already explicitly specified
CREATE TABLE like_heap_all_use_ao (LIKE ctlt_heap INCLUDING ALL) USING ao_row; -- error, AM already explicitly specified
CREATE TABLE like_heap_all_use_ao (LIKE ctlt_heap INCLUDING ALL EXCLUDING AM) USING ao_row; -- error, unrecognized parameter "fillfactor"
CREATE TABLE like_heap_all_with_relopt (LIKE ctlt_heap INCLUDING ALL) WITH (fillfactor = 50); -- error, relopt already explicitly specified
RESET default_table_access_method;

-- create a source ao_row table
CREATE TABLE ctlt_aorow (b text) USING ao_row WITH (compresstype=zstd,compresslevel=5,blocksize=65536);
ALTER TABLE ctlt_aorow ALTER COLUMN b SET STORAGE MAIN;
-- create table like the ao_row table
CREATE TABLE like_aorow_storage (LIKE ctlt_aorow INCLUDING STORAGE); -- succeeds
CREATE TABLE like_aorow_encoding (LIKE ctlt_aorow INCLUDING ENCODING); -- succeeds, but ignored
CREATE TABLE like_aorow_relopt (LIKE ctlt_aorow INCLUDING RELOPT); -- errors, unrecognized parameter "blocksize"
CREATE TABLE like_aorow_relopt_use_aorow (LIKE ctlt_aorow INCLUDING RELOPT) USING ao_row; -- succeeds
CREATE TABLE like_aorow_am (LIKE ctlt_aorow INCLUDING AM); -- succeeds
CREATE TABLE like_aorow_am_relopt (LIKE ctlt_aorow INCLUDING AM INCLUDING relopt); -- succeeds
CREATE TABLE like_aorow_all (LIKE ctlt_aorow INCLUDING ALL); -- succeeds
CREATE TABLE like_aorow_all_use_aorow (LIKE ctlt_aorow INCLUDING ALL) USING ao_row; -- errors, AM already explicitly specified
CREATE TABLE like_aorow_all_exc_am_use_aorow (LIKE ctlt_aorow INCLUDING ALL EXCLUDING AM) USING ao_row; -- succeeds
CREATE TABLE like_aorow_all_with_relopt (LIKE ctlt_aorow INCLUDING ALL) WITH (compresstype=zstd,compresslevel=5,blocksize=65536); -- errors, relopt already explicitly specified
CREATE TABLE like_aorow_all_exc_relopt_with_relopt (LIKE ctlt_aorow INCLUDING ALL EXCLUDING RELOPT) WITH (compresstype=zstd,compresslevel=5,blocksize=65536); -- errors, relopt already explicitly specified

-- create a source ao_col table
CREATE TABLE ctlt_aocol (c text ENCODING (compresstype=zlib, compresslevel=1, blocksize=8192), d int) USING ao_column WITH (compresstype=zlib, compresslevel=3, blocksize=32768);
ALTER TABLE ctlt_aocol ALTER COLUMN c SET STORAGE MAIN;
-- create table like the ao_col table
CREATE TABLE like_aocol_storage (LIKE ctlt_aocol INCLUDING STORAGE); -- succeeds
CREATE TABLE like_aocol_encoding (LIKE ctlt_aocol INCLUDING ENCODING); -- errors, encoding not supported for non ao_col
CREATE TABLE like_aocol_encoding_use_aocol (LIKE ctlt_aocol INCLUDING ENCODING) USING ao_column; -- errors, encoding not supported for non ao_col
CREATE TABLE like_aocol_relopt (LIKE ctlt_aocol INCLUDING RELOPT); -- errors, unrecognized parameter "compresstype"
CREATE TABLE like_aocol_relopt_with_aocol (LIKE ctlt_aocol INCLUDING RELOPT) WITH (appendonly=true, orientation=column); -- succeeds
CREATE TABLE like_aocol_am (LIKE ctlt_aocol INCLUDING AM); -- succeeds
CREATE TABLE like_aocol_relopt_am (LIKE ctlt_aocol INCLUDING RELOPT INCLUDING AM); -- succeeds
CREATE TABLE like_aocol_all (LIKE ctlt_aocol INCLUDING ALL); -- succeeds
CREATE TABLE like_aocol_all_use_aocol (LIKE ctlt_aocol INCLUDING ALL) USING ao_column; -- errors, AM already explicitly specified
CREATE TABLE like_aocol_all_with_aocol (LIKE ctlt_aocol INCLUDING ALL) WITH (appendonly = true, orientation = column); -- errors, AM already explicitly specified
CREATE TABLE like_aocol_all_exc_am_use_aocol (LIKE ctlt_aocol INCLUDING ALL EXCLUDING AM) USING ao_column; ; -- succeeds
CREATE TABLE like_aocol_all_add_encoding (LIKE ctlt_aocol INCLUDING ALL, i text ENCODING (compresstype=zstd, compresslevel=3, blocksize=65536)); -- succeeds
CREATE TABLE like_aocol_all_with_relopt (LIKE ctlt_aocol INCLUDING ALL) WITH (compresstype=zlib, compresslevel=3, blocksize=32768); -- errors, relopt already explicitly specified

-- Multiple LIKE INCLUDING ENCODING clauses
SET default_table_access_method = ao_column;
CREATE TABLE ctlt_aocol2 (c2 text ENCODING (compresstype=zstd, compresslevel=3, blocksize=8192), d2 int) USING ao_column WITH (compresstype=zlib, compresslevel=3, blocksize=32768);
CREATE TABLE like_aocol_encoding_aocol_encoding (LIKE ctlt_aocol INCLUDING ENCODING, LIKE ctlt_aocol2 INCLUDING ENCODING); -- succeeds
CREATE TABLE like_aocol_encoding_aorow_encoding (LIKE ctlt_aocol INCLUDING ENCODING, LIKE ctlt_aorow INCLUDING ENCODING); -- succeeds
RESET default_table_access_method;

-- Multiple LIKE INCLUDING RELOPT clauses
CREATE TABLE ctlt_heap2 (a2 text) WITH (fillfactor = 70);
CREATE TABLE like_heap_relopt_heap_relopt (LIKE ctlt_heap INCLUDING RELOPT, LIKE ctlt_heap2 INCLUDING RELOPT); -- errors, multiple INCLUDING RELOPTs not allowed

-- Multiple LIKE INCLUDING AM clauses
CREATE TABLE like_heap_am_aocol_am (LIKE ctlt_heap INCLUDING AM, LIKE ctlt_aocol INCLUDING AM); -- errors, multiple INCLUDING AMs not allowed

-- Multiple LIKEs with mixed INCLUDING ENCODING, RELOPT, and AM clauses
CREATE TABLE like_aocol_encoding_aorow_am (LIKE ctlt_aocol INCLUDING ENCODING, LIKE ctlt_aorow INCLUDING AM); -- succeeds, but ignores ENCODING
CREATE TABLE like_aocol_relopt_aorow_am (LIKE ctlt_aocol INCLUDING RELOPT, LIKE ctlt_aorow INCLUDING AM); -- succeeds
CREATE TABLE like_aocol_relopt_heap_am (LIKE ctlt_aocol INCLUDING RELOPT, LIKE ctlt_heap INCLUDING AM); -- errors, unrecognized parameter "compresstype"
CREATE TABLE like_heap_relopt_aocol_am (LIKE ctlt_heap INCLUDING RELOPT, LIKE ctlt_aocol INCLUDING AM); -- errors, unrecognized parameter "fillfactor"
CREATE TABLE ctlt_heap_no_relopt (a2 text);
CREATE TABLE like_heap_relopt_aocol_am2 (LIKE ctlt_heap_no_relopt INCLUDING RELOPT, LIKE ctlt_aocol INCLUDING AM); -- succeeds

-- Verify created tables and attributes
select c.relname, am.amname, c.reloptions from pg_class c JOIN pg_am am ON c.relam = am.oid where relname LIKE 'like_%' order by relname;

SELECT
	c.relname,
	ae.attnum,
	ae.filenum,
	ae.attoptions,
	a.attstorage
FROM
	pg_catalog.pg_class c
		JOIN pg_catalog.pg_attribute_encoding ae ON (ae.attrelid = c.oid)
		JOIN pg_catalog.pg_attribute a ON (a.attrelid = c.oid AND a.attnum = ae.attnum)
WHERE
	c.relname like 'like_%';

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
