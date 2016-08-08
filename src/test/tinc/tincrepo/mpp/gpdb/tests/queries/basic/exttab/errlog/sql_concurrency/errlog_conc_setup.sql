DROP EXTERNAL TABLE IF EXISTS exttab_conc_1 CASCADE;
DROP EXTERNAL TABLE IF EXISTS exttab_conc_2 CASCADE;

-- Generate the file with very few errors
\! python @script@ 10 2 > @data_dir@/exttab_conc_1.tbl

-- does not reach reject limit
CREATE EXTERNAL TABLE exttab_conc_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_conc_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

-- Generate the file with lot of errors
\! python @script@ 200 50 > @data_dir@/exttab_conc_2.tbl

CREATE EXTERNAL TABLE exttab_conc_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_conc_2.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 100;