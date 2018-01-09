SELECT * FROM tbl1_ts;

\c db_ts;

\l+;

SELECT * FROM tbl2_ts;

select spcname from pg_tablespace where spcname='ts';
