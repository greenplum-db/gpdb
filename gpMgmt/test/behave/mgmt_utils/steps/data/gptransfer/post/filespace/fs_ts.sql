SELECT * FROM  pg_filespace;

\c gptest;

SELECT * FROM tbl1_ts;

\c db_ts;

\l+;

SELECT * FROM tbl2_ts;

SELECT spcname FROM  pg_tablespace where spcname='ts';

DROP TABLE tbl2_ts;

\c template1;

SELECT pg_sleep(2);

DROP DATABASE db_ts;

DROP TABLESPACE ts;

DROP FILESPACE fs;
