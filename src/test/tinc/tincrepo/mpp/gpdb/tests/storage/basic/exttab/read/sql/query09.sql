drop external table if exists ret_table1;
CREATE EXTERNAL TABLE ret_table1(a integer)
location ('file://@hostname@@abs_srcdir@/sql/query09.sql')
FORMAT 'text';
select * from ret_table1;
