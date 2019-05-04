-- AO/AOCS
CREATE TABLE t_ao (a integer, b text) WITH (appendonly=true, orientation=column);
CREATE TABLE t_ao_enc (a integer, b text ENCODING (compresstype=zlib,compresslevel=1,blocksize=32768)) WITH (appendonly=true, orientation=column);

CREATE TABLE t_ao_a (LIKE t_ao INCLUDING ALL);
CREATE TABLE t_ao_b (LIKE t_ao INCLUDING STORAGE);
CREATE TABLE t_ao_c (LIKE t_ao); -- Should create a heap table

CREATE TABLE t_ao_enc_a (LIKE t_ao_enc INCLUDING STORAGE);

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
