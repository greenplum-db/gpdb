-- start_ignore
drop external table if exists ext_line;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINE (a integer,b text)
location ('file://@hostname@@gpfdist_datadir@/lines?')
FORMAT 'text'(delimiter',')
log errors
segment reject limit 40 rows;
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
