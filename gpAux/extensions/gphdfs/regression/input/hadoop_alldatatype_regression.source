\echo -- start_ignore
drop  external table alldatatype_regression;
\echo -- end_ignore


create readable external table alldatatype_regression(
datatype_all varchar,xcount_bigint bigint,
col1_time time,col2_time time, col3_time time, col4_time time, col5_time time, col6_time time, col7_time time, col8_time time, col9_time time, nullcol_time time,
col1_timestamp timestamp,col2_timestamp timestamp, col3_timestamp timestamp, nullcol_timestamp timestamp,
col1_date date,col2_date date, col3_date date, col4_date date, col5_date date, col6_date date, col7_date date, nullcol_date date,
max_bigint bigint, min_bigint bigint, x_bigint bigint, reverse_bigint bigint, increment_bigint bigint, nullcol_bigint bigint,
max_int int, min_int int, x_int int, reverse_int int, increment_int int, nullcol_int int,
max_smallint smallint, min_smallint smallint, x_smallint smallint, reverse_smallint smallint, increment_smallint smallint, nullcol_smallint smallint,
max_real real, min_real real, pi_real real, piX_real real, nullcol_real real,
max_float float, min_float float, pi_float float, piX_float float, nullcol_float float,
col1_boolean boolean, nullcol_boolean boolean,
col1_bpchar bpchar,col2_bpchar bpchar, nullcol_bpchar bpchar,
col1_varchar varchar,col2_varchar varchar, nullcol_varchar varchar,
max_numeric numeric, min_numeric numeric, x_numeric numeric, reverse_numeric numeric, increment_numeric numeric, nullcol_numeric numeric,
col1_text text,col2_text text, nullcol_text text
) location ('gphdfs://%HADOOP_HOST%/plaintext/all_100.txt')format 'TEXT';


SELECT count(*) FROM alldatatype_regression;
select * from alldatatype_regression order by xcount_bigint limit 10;
