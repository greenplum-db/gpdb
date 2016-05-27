DROP TABLE IF EXISTS test_zlib_hashjoin;
CREATE TABLE test_zlib_hashjoin (i1 int, i2 int, i3 int, i4 int, i5 int, i6 int, i7 int, i8 int) WITH (APPENDONLY=true) DISTRIBUTED BY (i1) ; 
INSERT INTO test_zlib_hashjoin SELECT i,i,i,i,i,i,i,i FROM generate_series (0,999999) i; 

SET gp_workfile_type_hashjoin=bfz;
SET gp_workfile_compress_algorithm=zlib;
SET statement_mem=5000;

--start_ignore
\! gpfaultinjector -f workfile_creation_failure -y reset --seg_dbid 2
\! gpfaultinjector -f workfile_creation_failure -y error --seg_dbid 2
--end_ignore

SELECT COUNT(t1.*) FROM test_zlib_hashjoin AS t1, test_zlib_hashjoin AS t2 WHERE t1.i1=t2.i2;

RESET statement_mem;
DROP TABLE IF EXISTS test_zlib_hagg; 
CREATE TABLE test_zlib_hagg (i1 int, i2 int, i3 int, i4 int);
INSERT INTO test_zlib_hagg SELECT i,i,i,i from generate_series(0,299999) i;

SET statement_mem=2000;

--start_ignore
\! gpfaultinjector -f workfile_creation_failure -y reset --seg_dbid 2
\! gpfaultinjector -f workfile_creation_failure -y error --seg_dbid 2
--end_ignore

SELECT MAX(i1) FROM test_zlib_hagg GROUP BY i2;