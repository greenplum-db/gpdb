-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology

\c db_test_bed
    CREATE TABLE test_table(
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric,
    int_col int4,
    float_col float4,
    int_array_col int[],
    before_rename_col int4,
    change_datatype_col numeric,
    a_ts_without timestamp without time zone,
    b_ts_with timestamp with time zone,
    date_column date,
    col_set_default numeric)DISTRIBUTED RANDOMLY;

    insert into test_table values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',0);
    insert into test_table values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001',1);
    insert into test_table values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',2);
    insert into test_table select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',i from generate_series(3,100)i;

    CREATE TABLE test_table1(
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric,
    int_col int4,
    float_col float4,
    int_array_col int[],
    before_rename_col int4,
    change_datatype_col numeric,
    a_ts_without timestamp without time zone,
    b_ts_with timestamp with time zone,
    date_column date,
    col_set_default numeric)DISTRIBUTED RANDOMLY;

CREATE TABLE err_table1 (cmdtime timestamp with time zone, relname text, filename text, linenum integer, bytenum integer, errmsg text, rawdata text, rawbytes bytea) DISTRIBUTED RANDOMLY;    

    insert into test_table1 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',0);
    insert into test_table1 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001',1);
    insert into test_table1 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',2);
    insert into test_table1 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',i from generate_series(3,100)i;


    CREATE TABLE test_table2(
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric,
    int_col int4,
    float_col float4,
    int_array_col int[],
    before_rename_col int4,
    change_datatype_col numeric,
    a_ts_without timestamp without time zone,
    b_ts_with timestamp with time zone,
    date_column date,
    col_set_default numeric) WITH OIDS DISTRIBUTED RANDOMLY;
-- start_ignore
    insert into test_table2 values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000',0);
    insert into test_table2 values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001',1);
    insert into test_table2 values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',2);
    insert into test_table2 select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002',i from generate_series(3,100)i;
-- end_ignore
COPY (select * from test_table) TO '/data/gpdb_p1/anu/dev/cdbfast/main/schema_topology/test1_file_copy' WITH DELIMITER AS ',' NULL AS 'null string' ESCAPE AS E'\n' CSV HEADER QUOTE AS '"' FORCE QUOTE char_vary_col;


COPY (select * from test_table) TO '/data/gpdb_p1/anu/dev/cdbfast/main/schema_topology/test1_file_copy' WITH HEADER DELIMITER AS ',' NULL AS 'null string' ESCAPE AS E'\n' CSV QUOTE AS '"' FORCE QUOTE char_vary_col;


COPY test_table2(   text_col,    bigint_col,    char_vary_col,    numeric_col,    int_col,    float_col,    int_array_col,    before_rename_col,    change_datatype_col,    a_ts_without ,    b_ts_with ,    date_column,    col_set_default)
 TO '/data/gpdb_p1/anu/dev/cdbfast/main/schema_topology/test2_file_copy' WITH OIDS  DELIMITER AS ','  NULL AS 'null string' ESCAPE AS 'OFF' ;


COPY test_table2 (   text_col,    bigint_col,    char_vary_col,    numeric_col,    int_col,    float_col,    int_array_col,    before_rename_col,    change_datatype_col,    a_ts_without ,    b_ts_with ,    date_column,    col_set_default) FROM '/data/gpdb_p1/anu/dev/cdbfast/main/schema_topology/test1_file_copy' WITH DELIMITER AS ',' NULL AS 'null string' ESCAPE AS E'\n' CSV HEADER QUOTE AS '"' FORCE NOT NULL date_column LOG ERRORS INTO err_table1 SEGMENT REJECT LIMIT 10 ROWS  ;

COPY test_table2 (   text_col,    bigint_col,    char_vary_col,    numeric_col,    int_col,    float_col,    int_array_col,    before_rename_col,    change_datatype_col,    a_ts_without ,    b_ts_with ,    date_column,    col_set_default) FROM '/data/gpdb_p1/anu/dev/cdbfast/main/schema_topology/test2_file_copy' WITH OIDS DELIMITER AS ',' NULL AS 'null string' ESCAPE AS 'OFF' LOG ERRORS INTO err_table1 KEEP SEGMENT REJECT LIMIT 10 PERCENT  ;


