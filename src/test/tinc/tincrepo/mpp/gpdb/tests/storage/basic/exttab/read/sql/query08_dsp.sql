-- @product_version gpdb: [4.3.6.0-]

-- Ensure that external tables behave correctly when default storage
-- options are set.

set gp_default_storage_options='appendonly=true';

-- start_ignore
-- In some consecutive test runs, this test is found to fail because some
-- other test that ran earlier set default tablespace, causing diff in
-- the \d output below.
set default_tablespace='';

drop external table if exists ext_line;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINE (a integer,b text)
location ('file://@hostname@@gpfdist_datadir@/lines?')
FORMAT 'text'(delimiter',')
log errors
segment reject limit 40 rows;
\d+ ext_line
select count(*) from ext_line;
select linenum,bytenum,rawdata from gp_read_error_log('ext_line') order by linenum;
-- start_ignore
drop external table if exists ext_line;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINE (a integer,b text)
location ('file://@hostname@@gpfdist_datadir@/lines?')
FORMAT 'text'(delimiter',' header)
log errors
segment reject limit 40 rows;
select count(*) from ext_line;
select linenum,bytenum,rawdata from gp_read_error_log('ext_line') order by linenum;
-- start_ignore
drop external table if exists ext_line;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINE (a integer,b text)
location ('file://@hostname@@gpfdist_datadir@/lines?')
FORMAT 'csv'(delimiter','quote as '"')
log errors
segment reject limit 40 rows;
select count(*) from ext_line;
select linenum,bytenum,rawdata from gp_read_error_log('ext_line') order by linenum;
-- start_ignore
drop external table if exists ext_line;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINE (a integer,b text)
location ('file://@hostname@@gpfdist_datadir@/lines?')
FORMAT 'csv'(delimiter','quote as '"' header)
log errors
segment reject limit 40 rows;
select count(*) from ext_line;
select linenum,bytenum,rawdata from gp_read_error_log('ext_line') order by linenum;
-- start_ignore
drop external table if exists ext_line;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINE (a integer,b text)
location ('gpfdist://@hostname@:@gp_port@/lines?')
FORMAT 'text'(delimiter',')
log errors
segment reject limit 40 rows;
select count(*) from ext_line;
select linenum,bytenum,rawdata from gp_read_error_log('ext_line') order by linenum;
-- start_ignore
drop external table if exists ext_line;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINE (a integer,b text)
location ('gpfdist://@hostname@:@gp_port@/lines?')
FORMAT 'text'(delimiter',' header)
log errors
segment reject limit 40 rows;
select count(*) from ext_line;
select linenum,bytenum,rawdata from gp_read_error_log('ext_line') order by linenum;
-- start_ignore
drop external table if exists ext_line;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINE (a integer,b text)
location ('gpfdist://@hostname@:@gp_port@/lines?')
FORMAT 'csv'(delimiter','quote as '"')
log errors
segment reject limit 40 rows;
select count(*) from ext_line;
select linenum,bytenum,rawdata from gp_read_error_log('ext_line') order by linenum;
-- start_ignore
drop external table if exists ext_line;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINE (a integer,b text)
location ('gpfdist://@hostname@:@gp_port@/lines?')
FORMAT 'csv'(delimiter','quote as '"' header)
log errors
segment reject limit 40 rows;
select count(*) from ext_line;
select linenum,bytenum,rawdata from gp_read_error_log('ext_line') order by linenum;
