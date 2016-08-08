-- start_ignore
drop external table if exists ext_lineitem;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINEITEM (L_COMMENT      VARCHAR(1000) )
location ('file://@hostname@@gpfdist_datadir@/lineitem.tbl-dir' )
FORMAT 'csv' (delimiter '=');

-- start_ignore
drop external table if exists ext_lineitem1;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINEITEM1 (L_COMMENT      VARCHAR(1000) )
location ('file://@hostname@@gpfdist_datadir@/lineitem.tbl-dir' )
FORMAT 'csv' (delimiter '=' header);

-- start_ignore
drop external table if exists ext_lineitem2;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINEITEM2 (L_COMMENT      VARCHAR(1000) )
location ('gpfdist://@hostname@:@gp_port@/lineitem.tbl-dir' )
FORMAT 'csv' (delimiter '=');

-- start_ignore
drop external table if exists ext_lineitem3;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINEITEM3 (L_COMMENT      VARCHAR(1000) )
location ('gpfdist://@hostname@:@gp_port@/lineitem.tbl-dir' )
FORMAT 'csv' (delimiter '=' header);

-- start_ignore
drop external table if exists ext_lineitem4;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINEITEM4 (L_COMMENT      VARCHAR(1000) )
location ('file://@hostname@@gpfdist_datadir@/lineitem.tbl-dir' )
FORMAT 'text' (delimiter '=');

-- start_ignore
drop external table if exists ext_lineitem5;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINEITEM5 (L_COMMENT      VARCHAR(1000) )
location ('file://@hostname@@gpfdist_datadir@/lineitem.tbl-dir' )
FORMAT 'text' (delimiter '=' header);

-- start_ignore
drop external table if exists ext_lineitem6;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINEITEM6 (L_COMMENT      VARCHAR(1000) )
location ('gpfdist://@hostname@:@gp_port@/lineitem.tbl-dir' )
FORMAT 'text' (delimiter '=');

-- start_ignore
drop external table if exists ext_lineitem7;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINEITEM7 (L_COMMENT      VARCHAR(1000) )
location ('gpfdist://@hostname@:@gp_port@/lineitem.tbl-dir' )
FORMAT 'text' (delimiter '=' header);

select count(*) from ext_lineitem;
select count(*) from ext_lineitem1;
select count(*) from ext_lineitem2;
select count(*) from ext_lineitem3;
select count(*) from ext_lineitem4;
select count(*) from ext_lineitem5;
select count(*) from ext_lineitem6;
select count(*) from ext_lineitem7;
