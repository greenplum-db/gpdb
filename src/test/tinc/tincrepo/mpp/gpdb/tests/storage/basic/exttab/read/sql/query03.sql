-- start_ignore
drop external table if exists ext_whois;
-- end_ignore
create external table ext_whois (
source_lineno			int,
domain_name			varchar(350),
subdomain			varchar(150),
tld				varchar(50),
ip_address			inet,
ip_address_int			bigint,
reverse_dns			varchar(512),
reverse_domain			varchar(350),
registrar			varchar(200),
registrar_referral		varchar(512),
whois_server			varchar(512),
harvest_date			timestamp with time zone,
created_on			timestamp with time zone,
update_date			timestamp with time zone,
expire_date			timestamp with time zone,
rank				int,
status				char(1),
update_status			int,
nameserver1			varchar(512),
nameserver1_domain_name		varchar(350),
nameserver1_subdomain		varchar(150),
nameserver1_tld			varchar(50),
nameserver2			varchar(512),
nameserver2_domain_name		varchar(350),
nameserver2_subdomain		varchar(150),
nameserver2_tld			varchar(50),
nameserver3			varchar(512),
nameserver3_domain_name		varchar(350),
nameserver3_subdomain		varchar(150),
nameserver3_tld			varchar(50),
nameserver4			varchar(512),
nameserver4_domain_name		varchar(350),
nameserver4_subdomain		varchar(150),
nameserver4_tld			varchar(50),
nameserver5			varchar(512),
nameserver5_domain_name		varchar(350),
nameserver5_subdomain		varchar(150),
nameserver5_tld			varchar(50),
registrant_name			varchar(200),
registrant_organization		varchar(200),
registrant_email		varchar(512),
registrant_email_domain		varchar(350),
registrant_email_subdomain	varchar(150),
registrant_email_tld		varchar(50),
registrant_phone		varchar(50),
registrant_fax			varchar(50),
registrant_addrall		varchar(1024),
registrant_street1		varchar(200),
registrant_street2		varchar(200),
registrant_street3		varchar(200),
registrant_city			varchar(200),
registrant_state_province	varchar(100),
registrant_postal_code		varchar(50),
registrant_country		varchar(100),
tech_name			varchar(200),
tech_organization		varchar(200),
tech_email			varchar(512),
tech_email_domain		varchar(350),
tech_email_subdomain		varchar(150),
tech_email_tld			varchar(50),
tech_phone			varchar(50),
tech_fax			varchar(50),
tech_addrall			varchar(1024),
tech_street1			varchar(200),
tech_street2			varchar(200),
tech_street3			varchar(200),
tech_city			varchar(200),
tech_state_province		varchar(100),
tech_postal_code		varchar(50),
tech_country			varchar(100),
admin_name			varchar(200),
admin_organization		varchar(200),
admin_email			varchar(512),
admin_email_domain		varchar(350),
admin_email_subdomain		varchar(150),
admin_email_tld			varchar(50),
admin_phone			varchar(50),
admin_fax			varchar(50),
admin_addrall			varchar(1024),
admin_street1			varchar(200),
admin_street2			varchar(200),
admin_street3			varchar(200),
admin_city			varchar(200),
admin_state_province		varchar(100),
admin_postal_code		varchar(50),
admin_country			varchar(100),
rec_path			varchar(512),
raw_record			text
)
location ('gpfdist://@hostname@:@gp_port@/whois.csv' )
format 'csv' (header quote as '"');

-- start_ignore
drop table if exists cpy_whois;
-- end_ignore
create table cpy_whois (
source_lineno			int,
domain_name			varchar(350),
subdomain			varchar(150),
tld				varchar(50),
ip_address			inet,
ip_address_int			bigint,
reverse_dns			varchar(512),
reverse_domain			varchar(350),
registrar			varchar(200),
registrar_referral		varchar(512),
whois_server			varchar(512),
harvest_date			timestamp with time zone,
created_on			timestamp with time zone,
update_date			timestamp with time zone,
expire_date			timestamp with time zone,
rank				int,
status				char(1),
update_status			int,
nameserver1			varchar(512),
nameserver1_domain_name		varchar(350),
nameserver1_subdomain		varchar(150),
nameserver1_tld			varchar(50),
nameserver2			varchar(512),
nameserver2_domain_name		varchar(350),
nameserver2_subdomain		varchar(150),
nameserver2_tld			varchar(50),
nameserver3			varchar(512),
nameserver3_domain_name		varchar(350),
nameserver3_subdomain		varchar(150),
nameserver3_tld			varchar(50),
nameserver4			varchar(512),
nameserver4_domain_name		varchar(350),
nameserver4_subdomain		varchar(150),
nameserver4_tld			varchar(50),
nameserver5			varchar(512),
nameserver5_domain_name		varchar(350),
nameserver5_subdomain		varchar(150),
nameserver5_tld			varchar(50),
registrant_name			varchar(200),
registrant_organization		varchar(200),
registrant_email		varchar(512),
registrant_email_domain		varchar(350),
registrant_email_subdomain	varchar(150),
registrant_email_tld		varchar(50),
registrant_phone		varchar(50),
registrant_fax			varchar(50),
registrant_addrall		varchar(1024),
registrant_street1		varchar(200),
registrant_street2		varchar(200),
registrant_street3		varchar(200),
registrant_city			varchar(200),
registrant_state_province	varchar(100),
registrant_postal_code		varchar(50),
registrant_country		varchar(100),
tech_name			varchar(200),
tech_organization		varchar(200),
tech_email			varchar(512),
tech_email_domain		varchar(350),
tech_email_subdomain		varchar(150),
tech_email_tld			varchar(50),
tech_phone			varchar(50),
tech_fax			varchar(50),
tech_addrall			varchar(1024),
tech_street1			varchar(200),
tech_street2			varchar(200),
tech_street3			varchar(200),
tech_city			varchar(200),
tech_state_province		varchar(100),
tech_postal_code		varchar(50),
tech_country			varchar(100),
admin_name			varchar(200),
admin_organization		varchar(200),
admin_email			varchar(512),
admin_email_domain		varchar(350),
admin_email_subdomain		varchar(150),
admin_email_tld			varchar(50),
admin_phone			varchar(50),
admin_fax			varchar(50),
admin_addrall			varchar(1024),
admin_street1			varchar(200),
admin_street2			varchar(200),
admin_street3			varchar(200),
admin_city			varchar(200),
admin_state_province		varchar(100),
admin_postal_code		varchar(50),
admin_country			varchar(100),
rec_path			varchar(512),
raw_record			text
)
;

select count(*) from ext_whois;
select count(*) from ext_whois where domain_name like '%viagra%';

copy cpy_whois FROM '@abs_srcdir@/data/whois.csv' csv header quote '"';
-- "

select count(*) from cpy_whois;
select count(*) from cpy_whois where domain_name like '%viagra%';

-- start_ignore
drop table if exists v_w;
-- end_ignore
create table v_w as select * from cpy_whois where domain_name like '%viagra%';

-- mpp-1561: sort distinct spill to disk/external sort
select count(*) from (select * from v_w union select * from cpy_whois) as foo;
select count(*) from (select * from v_w union all select * from cpy_whois) as foo;
select count(*) from (select distinct * from cpy_whois) as foo;
select count(*) from (select * from cpy_whois order by 1 limit 300000 offset 30000) as foo;
select count(*) from (select * from cpy_whois order by 1 limit 300000 offset 1000) as foo;
select source_lineno, domain_name from cpy_whois order by 
source_lineno	,
domain_name		,
subdomain		,
tld				,
ip_address		,
ip_address_int	,
reverse_dns		,
reverse_domain	,
registrar		,
registrar_referral,
whois_server		,
harvest_date		,
created_on			,
update_date			,
expire_date			,
rank				,
status				,
update_status		,
nameserver1			,
nameserver1_domain_name,
nameserver1_subdomain	,
nameserver1_tld			,
nameserver2			,
nameserver2_domain_name	,
nameserver2_subdomain	,
nameserver2_tld			,
nameserver3			,
nameserver3_domain_name	,
nameserver3_subdomain	,
nameserver3_tld			,
nameserver4			,
nameserver4_domain_name	,
nameserver4_subdomain	,
nameserver4_tld			,
nameserver5		,
nameserver5_domain_name,
nameserver5_subdomain	,
nameserver5_tld			,
registrant_name			,
registrant_organization	,
registrant_email		,
registrant_email_domain	,
registrant_email_subdomain,
registrant_email_tld		,
registrant_phone		,
registrant_fax			,
registrant_addrall		,
registrant_street1		,
registrant_street2		,
registrant_street3		,
registrant_city			,
registrant_state_province,
registrant_postal_code		,
registrant_country,
tech_name			,
tech_organization	,
tech_email			,
tech_email_domain	,
tech_email_subdomain,
tech_email_tld		,
tech_phone			,
tech_fax			,
tech_addrall		,
tech_street1		,
tech_street2		,
tech_street3		,
tech_city			,
tech_state_province	,
tech_postal_code	,
tech_country		,
admin_name			,
admin_organization	,
admin_email			,
admin_email_domain	,
admin_email_subdomain,
admin_email_tld			,
admin_phone		,
admin_fax		,
admin_addrall	,
admin_street1	,
admin_street2	,
admin_street3	,
admin_city		,
admin_state_province,
admin_postal_code	,
admin_country		,
rec_path			,
raw_record			
limit 10 offset 10;


-- variations on mpp-1247
select count(*) from (select * from v_w union select * from ext_whois) as foo;
select count(*) from (select * from v_w union all select * from ext_whois) as foo;
select count(*) from (select * from v_w intersect select * from ext_whois) as foo;
select count(*) from (select * from v_w intersect all select * from ext_whois) as foo;
select count(*) from (select * from v_w except select * from ext_whois) as foo;
select count(*) from (select * from v_w except all select * from ext_whois) as foo;
select count(*) from (select * from ext_whois except select * from v_w) as foo;
select count(*) from (select * from ext_whois except all select * from v_w) as foo;

--
-- bad csv (quote must be a single char)
--
-- start_ignore
drop external table if exists bad_whois;
-- end_ignore
create external table bad_whois (
source_lineno			int,
domain_name			varchar(350)
)
location ('gpfdist://@hostname@:@gp_port@/whois.csv' )
format 'csv' ( header quote as 'ggg');
select count(*) from bad_whois;

--
-- bad csv (ignore quoting)
--
-- start_ignore
drop external table if exists bad_whois;
-- end_ignore
create external table bad_whois (
source_lineno			int,
domain_name			varchar(350),
subdomain			varchar(150),
tld				varchar(50),
ip_address			inet,
ip_address_int			bigint,
reverse_dns			varchar(512),
reverse_domain			varchar(350),
registrar			varchar(200),
registrar_referral		varchar(512),
whois_server			varchar(512),
harvest_date			timestamp with time zone,
created_on			timestamp with time zone,
update_date			timestamp with time zone,
expire_date			timestamp with time zone,
rank				int,
status				char(1),
update_status			int,
nameserver1			varchar(512),
nameserver1_domain_name		varchar(350),
nameserver1_subdomain		varchar(150),
nameserver1_tld			varchar(50),
nameserver2			varchar(512),
nameserver2_domain_name		varchar(350),
nameserver2_subdomain		varchar(150),
nameserver2_tld			varchar(50),
nameserver3			varchar(512),
nameserver3_domain_name		varchar(350),
nameserver3_subdomain		varchar(150),
nameserver3_tld			varchar(50),
nameserver4			varchar(512),
nameserver4_domain_name		varchar(350),
nameserver4_subdomain		varchar(150),
nameserver4_tld			varchar(50),
nameserver5			varchar(512),
nameserver5_domain_name		varchar(350),
nameserver5_subdomain		varchar(150),
nameserver5_tld			varchar(50),
registrant_name			varchar(200),
registrant_organization		varchar(200),
registrant_email		varchar(512),
registrant_email_domain		varchar(350),
registrant_email_subdomain	varchar(150),
registrant_email_tld		varchar(50),
registrant_phone		varchar(50),
registrant_fax			varchar(50),
registrant_addrall		varchar(1024),
registrant_street1		varchar(200),
registrant_street2		varchar(200),
registrant_street3		varchar(200),
registrant_city			varchar(200),
registrant_state_province	varchar(100),
registrant_postal_code		varchar(50),
registrant_country		varchar(100),
tech_name			varchar(200),
tech_organization		varchar(200),
tech_email			varchar(512),
tech_email_domain		varchar(350),
tech_email_subdomain		varchar(150),
tech_email_tld			varchar(50),
tech_phone			varchar(50),
tech_fax			varchar(50),
tech_addrall			varchar(1024),
tech_street1			varchar(200),
tech_street2			varchar(200),
tech_street3			varchar(200),
tech_city			varchar(200),
tech_state_province		varchar(100),
tech_postal_code		varchar(50),
tech_country			varchar(100),
admin_name			varchar(200),
admin_organization		varchar(200),
admin_email			varchar(512),
admin_email_domain		varchar(350),
admin_email_subdomain		varchar(150),
admin_email_tld			varchar(50),
admin_phone			varchar(50),
admin_fax			varchar(50),
admin_addrall			varchar(1024),
admin_street1			varchar(200),
admin_street2			varchar(200),
admin_street3			varchar(200),
admin_city			varchar(200),
admin_state_province		varchar(100),
admin_postal_code		varchar(50),
admin_country			varchar(100),
rec_path			varchar(512),
raw_record			text
)
location ('gpfdist://@hostname@:@gp_port@/whois.csv' )
format 'csv' ( header quote as 'g');
-- ignore quoting (use g vs double quote)
--
select count(*) from bad_whois;
drop external table bad_whois;
--
-- invalid formatting (drop first column [source_line int])
--
-- start_ignore
drop external table if exists bad2_whois;
-- end_ignore
create external table bad2_whois (
domain_name			varchar(350),
subdomain			varchar(150),
tld				varchar(50),
ip_address			inet,
ip_address_int			bigint,
reverse_dns			varchar(512),
reverse_domain			varchar(350),
registrar			varchar(200),
registrar_referral		varchar(512),
whois_server			varchar(512),
harvest_date			timestamp with time zone,
created_on			timestamp with time zone,
update_date			timestamp with time zone,
expire_date			timestamp with time zone,
rank				int,
status				char(1),
update_status			int,
nameserver1			varchar(512),
nameserver1_domain_name		varchar(350),
nameserver1_subdomain		varchar(150),
nameserver1_tld			varchar(50),
nameserver2			varchar(512),
nameserver2_domain_name		varchar(350),
nameserver2_subdomain		varchar(150),
nameserver2_tld			varchar(50),
nameserver3			varchar(512),
nameserver3_domain_name		varchar(350),
nameserver3_subdomain		varchar(150),
nameserver3_tld			varchar(50),
nameserver4			varchar(512),
nameserver4_domain_name		varchar(350),
nameserver4_subdomain		varchar(150),
nameserver4_tld			varchar(50),
nameserver5			varchar(512),
nameserver5_domain_name		varchar(350),
nameserver5_subdomain		varchar(150),
nameserver5_tld			varchar(50),
registrant_name			varchar(200),
registrant_organization		varchar(200),
registrant_email		varchar(512),
registrant_email_domain		varchar(350),
registrant_email_subdomain	varchar(150),
registrant_email_tld		varchar(50),
registrant_phone		varchar(50),
registrant_fax			varchar(50),
registrant_addrall		varchar(1024),
registrant_street1		varchar(200),
registrant_street2		varchar(200),
registrant_street3		varchar(200),
registrant_city			varchar(200),
registrant_state_province	varchar(100),
registrant_postal_code		varchar(50),
registrant_country		varchar(100),
tech_name			varchar(200),
tech_organization		varchar(200),
tech_email			varchar(512),
tech_email_domain		varchar(350),
tech_email_subdomain		varchar(150),
tech_email_tld			varchar(50),
tech_phone			varchar(50),
tech_fax			varchar(50),
tech_addrall			varchar(1024),
tech_street1			varchar(200),
tech_street2			varchar(200),
tech_street3			varchar(200),
tech_city			varchar(200),
tech_state_province		varchar(100),
tech_postal_code		varchar(50),
tech_country			varchar(100),
admin_name			varchar(200),
admin_organization		varchar(200),
admin_email			varchar(512),
admin_email_domain		varchar(350),
admin_email_subdomain		varchar(150),
admin_email_tld			varchar(50),
admin_phone			varchar(50),
admin_fax			varchar(50),
admin_addrall			varchar(1024),
admin_street1			varchar(200),
admin_street2			varchar(200),
admin_street3			varchar(200),
admin_city			varchar(200),
admin_state_province		varchar(100),
admin_postal_code		varchar(50),
admin_country			varchar(100),
rec_path			varchar(512),
raw_record			text
)
location ('gpfdist://@hostname@:@gp_port@/whois.csv' )
format 'csv' (header quote as '"');
-- "
select count(rec_path) from bad2_whois;
drop external table bad2_whois;
--
-- more invalid formatting -- treat a csv file as a text file
--
-- start_ignore
drop external table if exists bad3_whois;
-- end_ignore
create external table bad3_whois (
source_lineno			int,
ip_address			inet,
ip_address_int			bigint,
reverse_dns			varchar(512)
)
location ('gpfdist://@hostname@:@gp_port@/whois.csv' )
format 'text' (header delimiter '|');
select count(*) from bad3_whois;
drop external table bad3_whois;

--
-- try a bad location
--
-- start_ignore
drop external table if exists badt1;
-- end_ignore
create external table badt1 (x text) 
location ('file://@hostname@@abs_srcdir@/data/no/such/place/badt1.tbl' )
format 'text' (delimiter '|');
select * from badt1;
drop external table badt1;
--
-- try a bad protocol
--
-- start_ignore
drop external table if exists badt2;
-- end_ignore
create external table badt2 (x text) 
location ('bad_protocol://@hostname@@abs_srcdir@/data/no/such/place/badt2.tbl' )
format 'text' (delimiter '|');
--
-- get an error for missing gpfdist
--
create external table ext_missing(a int, b int)
location ('gpfdist://@hostname@:99/missing.csv')
format 'csv';
select count(*) from ext_missing;
drop external table ext_missing;

-- do the csv copy tests (adapted from copy.source)

--- test copying in CSV mode with various styles
--- of embedded line ending characters
-- start_ignore
drop table if exists extcpytest;
-- end_ignore
create temp table extcpytest (
	style	text,
	test 	text,
	filler	int);

insert into extcpytest values('DOS',E'abc\r\ndef',1);
insert into extcpytest values('Unix',E'abc\ndef',2);
insert into extcpytest values('Mac',E'abc\rdef',3);
insert into extcpytest values(E'esc\\ape',E'a\\r\\\r\\\n\\nb',4);

copy extcpytest to '@gpfdist_datadir@/output/extcpytest.csv' csv;
copy extcpytest to '@gpfdist_datadir@/output/extcpytest.csv' csv quote 'z' escape E'Z';

-- start_ignore
drop external table if exists extcpytest2;
-- end_ignore
create external table  extcpytest2 (
	style	text,
	test 	text,
	filler	int)
location ('gpfdist://@hostname@:@gp_port@/output/extcpytest.csv')
format 'csv' ( quote as 'z' escape as 'Z');

select * from extcpytest except select * from extcpytest2 order by 1,2,3;

drop external table extcpytest2;

-- test header line feature

-- recreate gpfdist with header (-h) option

-- start_ignore
drop table if exists extcpytest3;
-- end_ignore
create temp table extcpytest3 (
	c1 int, 
--	"col with , comma" text, 
--	"col with "" quote"  int);
	c2 text, 
	c3 int);

copy extcpytest3 from stdin csv header;
this is just a line full of junk that would error out if parsed
1,a,1
2,b,2
\.

copy extcpytest3 to '@gpfdist_datadir@/output/extcpytest3.csv' csv header quote 'z' escape E'Z'  ;

-- start_ignore
drop external web table if exists gpfdist_curl;
-- end_ignore
CREATE EXTERNAL WEB TABLE gpfdist_curl (c1 text)
execute E'(@gpwhich_curl@ http://@hostname@:@gp_port@/output/extcpytest3.csv)'
on SEGMENT 0
FORMAT 'text' (header delimiter '|');

select * from gpfdist_curl;
DROP EXTERNAL TABLE gpfdist_curl;
--
-- MPP-1584: make -h work with small files
--
-- start_ignore
drop external table if exists extcpytest4;
-- end_ignore
create external table extcpytest4 (
c1 int,
c2 text,
c3 int)
location ('gpfdist://@hostname@:@gp_port@/output/extcpytest3.csv')
format 'csv' (header quote as 'z' escape as 'Z');

select * from extcpytest4;

-- start_ignore
drop external table if exists extcpytest5;
-- end_ignore
create external table extcpytest5 (
c1 int,
c2 text,
c3 int)
location ('file://@hostname@@gpfdist_datadir@/output/extcpytest3.csv')
format 'csv' ( header quote as 'z' escape as 'Z');

select * from extcpytest5;

drop external table extcpytest4;
drop external table extcpytest5;
