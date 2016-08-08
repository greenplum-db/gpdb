DROP EXTERNAL TABLE IF EXISTS exttab_mpp22914_1;
DROP EXTERNAL TABLE IF EXISTS exttab_mpp22914_2;

-- Generate the file with very few errors
\! python @script@ 10 2 > @data_dir@/exttab_mpp22914_1.tbl

-- does not reach reject limit
CREATE EXTERNAL TABLE exttab_mpp22914_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_mpp22914_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

-- Generate the file with lot of errors
\! python @script@ 200 50 > @data_dir@/exttab_mpp22914_2.tbl

-- reaches reject limit, use the same err table
CREATE EXTERNAL TABLE exttab_mpp22914_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_mpp22914_2.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 2;

-- mpp22914
DROP TABLE IF EXISTS test_mpp22914;

CREATE TABLE test_mpp22914( i int, j text);

-- Turn off stats 
SET gp_autostats_mode = 'NONE';

INSERT INTO test_mpp22914 SELECT i, i || '_number' FROM generate_series(1, 10) i;

SELECT COUNT(*) FROM test_mpp22914, exttab_mpp22914_1;

SELECT COUNT(*) FROM gp_read_error_log('exttab_mpp22914_1');