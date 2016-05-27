DROP TABLE IF EXISTS test_zlib;
CREATE TABLE test_zlib (i1 int, i2 int, i3 int, i4 int, i5 int, i6 int, i7 int, i8 int) WITH (APPENDONLY=true) DISTRIBUTED BY (i1) ; 
INSERT INTO test_zlib SELECT i,i,i,i,i,i,i,i FROM generate_series (0,999999) i; 

set gp_workfile_type_hashjoin=bfz;
SET gp_workfile_compress_algorithm=zlib;
SET statement_mem=5000;

--start_ignore
\! gpfaultinjector -f workfile_creation_failure -y reset --seg_dbid 2
\! gpfaultinjector -f workfile_creation_failure -y error --seg_dbid 2
--end_ignore

SELECT COUNT(t1.*) FROM test_zlib AS t1, test_zlib AS t2 WHERE t1.i1=t2.i2;
