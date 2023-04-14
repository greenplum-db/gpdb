-- Test ALTER TABLE ADD COLUMN / DROP COLUMN, on AOCO tables, with different
-- storage options.

-- Setup test tables

-- AOCO multiple insert to create multiple var-block
DROP TABLE IF EXISTS multivarblock_tab;
CREATE TABLE multivarblock_tab (
    c_custkey integer,
    c_name character varying(25),
    c_comment text,
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp
)
WITH (checksum=true, appendonly=true, orientation=column) DISTRIBUTED BY (c_custkey);
insert into multivarblock_tab values( 1, 'aa','this is a looong text' , 3.5, '12121212',1000.34,'2015/10/10',now());
insert into multivarblock_tab values( 2, 'ab','this is also a looong text' , 4.5, '3456789',3000.45,'2014/08/10',now());
insert into multivarblock_tab values( 3, 'ac','this  too is a looong text' , 1.5, '878787',500.54,'2014/04/04',now());

-- AOCO multiple insert to create multiple var-block for table with btree index
DROP TABLE IF EXISTS multivarblock_bitab;
CREATE TABLE multivarblock_bitab (
    c_custkey integer,
    c_name character varying(25),
    c_comment text,
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp
)
WITH (checksum=true, appendonly=true, orientation=column) DISTRIBUTED BY (c_custkey);
CREATE INDEX multivarblock_btree_idx ON multivarblock_bitab USING btree (c_custkey);
insert into multivarblock_bitab values( 1, 'aa','this is a looong text' , 3.5, '12121212',1000.34,'2015/10/10',now());
insert into multivarblock_bitab values( 2, 'ab','this is also a looong text' , 4.5, '3456789',3000.45,'2014/08/10',now());
insert into multivarblock_bitab values( 3, 'ac','this  too is a looong text' , 1.5, '878787',500.54,'2014/04/04',now());

-- AOCO multiple insert to create multiple var-block for table with partitions
DROP TABLE IF EXISTS multivarblock_parttab;
CREATE TABLE multivarblock_parttab (
    c_custkey integer,
    c_name character varying(25),
    c_comment text,
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp
)
WITH (checksum=true, appendonly=true, orientation=column) DISTRIBUTED BY (c_custkey)
partition by range(c_custkey)  subpartition by range( c_rating)
subpartition template ( default subpartition subothers,start (0.0) end(1.9) every (2.0) )
(default partition others, partition p1 start(1) end(5000), partition p2 start(5000) end(10000), partition p3 start(10000) end(15000));
insert into multivarblock_parttab values( 1, 'aa','this is a looong text' , 4.5, '12121212',1000.34,'2015/10/10',now());
insert into multivarblock_parttab values( 2, 'ab','this is also a looong text' , 7.5, '3456789',3000.45,'2014/08/10',now());
insert into multivarblock_parttab values( 3, 'ac','this  too is a looong text' , 1.5, '878787',4000.25,'2014/08/10',now());

-- AOCO multiple insert to create multiple var-block
DROP TABLE IF EXISTS multivarblock_toast;
CREATE TABLE multivarblock_toast (
    c_custkey integer,
    c_name character varying(25),
    c_comment text,
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp
)
WITH (checksum=true, appendonly=true, orientation=column) DISTRIBUTED BY (c_custkey);
insert into multivarblock_toast values( 1, 'aa','this is a looong text' , 3.5, '12121212',1000.34,'2015/10/10',now());
insert into multivarblock_toast values( 2, 'ab','this is also a looong text' , 4.5, '3456789',3000.45,'2014/08/10',now());
insert into multivarblock_toast values( 3, 'ac','this  too is a looong text' , 1.5, '878787',500.54,'2014/04/04',now());

-- AOCO multiple insert to create multiple var-block
DROP TABLE IF EXISTS multivarblock_zlibtab;
CREATE TABLE multivarblock_zlibtab (
    c_custkey integer,
    c_name character varying(25),
    c_comment text,
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp
)
WITH (checksum=true, appendonly=true, orientation=column, compresstype=zlib, compresslevel=9) DISTRIBUTED BY (c_custkey);
insert into multivarblock_zlibtab values( 1, 'aa','this is a looong text' , 3.5, '12121212',1000.34,'2015/10/10',now());
insert into multivarblock_zlibtab values( 2, 'ab','this is also a looong text' , 4.5, '3456789',3000.45,'2014/08/10',now());
insert into multivarblock_zlibtab values( 3, 'ac','this  too is a looong text' , 1.5, '878787',500.54,'2014/04/04',now());

DROP TABLE IF EXISTS multi_segfile_tab;
CREATE TABLE multi_segfile_tab (
    c_custkey integer,
    c_name character varying(25),
    c_comment text,
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp
)
WITH (checksum=true, appendonly=true, orientation=column, compresstype=zlib, compresslevel=1) DISTRIBUTED BY (c_custkey);
insert into multi_segfile_tab values( 1, 'aa','this is a looong text' , 3.5, '12121212',1000.34,'2015/10/10',now());
insert into multi_segfile_tab values( 2, 'ab','this is also a looong text' , 4.5, '3456789',3000.45,'2014/08/10',now());
update multi_segfile_tab set c_name = 'bcx' where c_custkey = 2;
vacuum multi_segfile_tab;
insert into multi_segfile_tab values( 3, 'ac','this  too is a looong text' , 1.5, '878787',500.54,'2014/04/04',now());


DROP TABLE IF EXISTS multi_segfile_bitab;
CREATE TABLE multi_segfile_bitab (
    c_custkey integer,
    c_name character varying(25),
    c_comment text,
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp
)
WITH (checksum=true, appendonly=true, orientation=column, compresstype=zlib, compresslevel=1) DISTRIBUTED BY (c_custkey);
CREATE INDEX multi_segfile_btree_idx ON multi_segfile_bitab USING btree (c_custkey);
insert into multi_segfile_bitab values( 1, 'aa','this is a looong text' , 3.5, '12121212',1000.34,'2015/10/10',now());
insert into multi_segfile_bitab values( 2, 'ab','this is also a looong text' , 4.5, '3456789',3000.45,'2014/08/10',now());
update multi_segfile_bitab set c_name = 'bcx' where c_custkey = 2;
vacuum multi_segfile_bitab;
insert into multi_segfile_bitab values( 3, 'ac','this  too is a looong text' , 1.5, '878787',500.54,'2014/04/04',now());

DROP TABLE IF EXISTS multi_segfile_zlibtab;
CREATE TABLE multi_segfile_zlibtab (
    c_custkey integer,
    c_name character varying(25),
    c_comment text,
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp
)
WITH (checksum=true, appendonly=true, orientation=column, compresstype=zlib, compresslevel=9) DISTRIBUTED BY (c_custkey);
insert into multi_segfile_zlibtab values( 1, 'aa','this is a looong text' , 3.5, '12121212',1000.34,'2015/10/10',now());
insert into multi_segfile_zlibtab values( 2, 'ab','this is also a looong text' , 4.5, '3456789',3000.45,'2014/08/10',now());
update multi_segfile_zlibtab set c_name = 'bcx' where c_custkey = 2;
vacuum multi_segfile_zlibtab;
insert into multi_segfile_zlibtab values( 3, 'ac','this  too is a looong text' , 1.5, '878787',500.54,'2014/04/04',now());

DROP TABLE IF EXISTS multi_segfile_parttab;
CREATE TABLE multi_segfile_parttab (
    c_custkey integer,
    c_name character varying(25),
    c_comment text,
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp
)
WITH (checksum=true, appendonly=true, orientation=column, compresstype=zlib, compresslevel=1) DISTRIBUTED BY (c_custkey)
partition by range(c_custkey)  subpartition by range( c_rating)
subpartition template ( default subpartition subothers,start (0.0) end(1.9) every (2.0) )
(default partition others, partition p1 start(1) end(5000), partition p2 start(5000) end(10000), partition p3 start(10000) end(15000));
insert into multi_segfile_parttab values( 1, 'aa','this is a looong text' , 4.5, '12121212',1000.34,'2015/10/10',now());
insert into multi_segfile_parttab values( 2, 'ab','this is also a looong text' , 7.5, '3456789',3000.45,'2014/08/10',now());
update multi_segfile_parttab set c_name = 'bcx' where c_custkey = 2;
vacuum multi_segfile_parttab;
insert into multi_segfile_parttab values( 3, 'ac','this  too is a looong text' , 1.5, '878787',4000.25,'2014/08/10',now());

DROP TABLE IF EXISTS multi_segfile_toast;
CREATE TABLE multi_segfile_toast (
    c_custkey integer,
    c_name character varying(25),
    c_comment text,
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp
)
WITH (checksum=true, appendonly=true, orientation=column, compresstype=zlib, compresslevel=1) DISTRIBUTED BY (c_custkey);
insert into multi_segfile_toast values( 1, 'aa','this is a looong text' , 3.5, '12121212',1000.34,'2015/10/10',now());
insert into multi_segfile_toast values( 2, 'ab','this is also a looong text' , 4.5, '3456789',3000.45,'2014/08/10',now());
update multi_segfile_toast set c_name = 'bcx' where c_custkey = 2;
vacuum multi_segfile_toast;
insert into multi_segfile_toast values( 3, 'ac','this  too is a looong text' , 1.5, '878787',500.54,'2014/04/04',now());


--
-- @description AOCO multi_segfile table : add column with default value NULL

alter table multi_segfile_tab ADD COLUMN added_col3 character varying(35) default NULL;
select count(*) as added_col3 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_tab' and attname='added_col3';

--
-- @description AOCO multi_segfile table : add column with default value non NULL

alter table multi_segfile_tab ADD COLUMN added_col1 character varying(35) default 'this is default value of non null';
select count(*) as added_col1 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_tab' and attname='added_col1';

--
-- @description AOCO multivarblock table : add column with default value non NULL then add index on new column
alter table multi_segfile_tab ADD COLUMN added_col50 TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW() ;
select count(*) as added_col50 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_tab' and attname='added_col50';
insert into multi_segfile_tab (c_custkey, c_name, c_comment, c_rating, c_phone, c_acctbal,c_date, c_timestamp)
                     values( 500, 'acz','yet another  looong text' , 11.5, '778777',550.54,'2014/05/04',now());
create index multi_segfile_tab_idx2 on multi_segfile_tab(added_col50);

--
-- @description AOCO multi_segfile table : add column with default value NULL

alter table multi_segfile_bitab ADD COLUMN added_col2 character varying(35) default NULL;
select count(*) as added_col2 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_bitab' and attname='added_col2';

--
-- @description AOCO multi_segfile table : add column with default value non NULL
alter table multi_segfile_bitab ADD COLUMN added_col1 character varying(35) default 'this is default value of non null';
select count(*) as added_col1 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_bitab' and attname='added_col1';

--
-- @description AOCO multi_segfile table : drop column with default value non NULL
alter table multi_segfile_bitab ADD COLUMN added_col22 character varying(35) default NULL;
select count(*) as added_col22 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_bitab' and attname='added_col22';
alter table multi_segfile_bitab DROP COLUMN added_col22;
select count(*) as added_col22 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_bitab' and attname='added_col22';
VACUUM multi_segfile_bitab;

--
-- @description AOCO multi_segfile table : drop column with default value non NULL

alter table multi_segfile_bitab ADD COLUMN added_col11 character varying(35) default 'this is default value of non null';
select count(*) as added_col11 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_bitab' and attname='added_col11';
alter table multi_segfile_bitab DROP COLUMN added_col11;
select count(*) as added_col11 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_bitab' and attname='added_col11';
VACUUM multi_segfile_bitab;

--
-- @description AOCO multi_segfile table : drop column with default value NULL
alter table multi_segfile_tab ADD COLUMN added_col33 character varying(35) default NULL;
select count(*) as added_col33 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_tab' and attname='added_col33';
alter table multi_segfile_tab DROP COLUMN added_col33;
select count(*) as added_col33 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_tab' and attname='added_col33';
VACUUM multi_segfile_tab;

--
-- @description AOCO multi_segfile table : drop column with default value non NULL
alter table multi_segfile_tab ADD COLUMN added_col11 character varying(35) default 'this is default value of non null';
select count(*) as added_col11 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_tab' and attname='added_col11';
alter table multi_segfile_tab DROP COLUMN added_col11;
select count(*) as added_col11 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_tab' and attname='added_col11';
VACUUM multi_segfile_tab;

--
-- @description AOCO multi_segfile table : add column with default value NULL
alter table multi_segfile_parttab ADD COLUMN added_col2 character varying(35) default NULL;
select count(*) as added_col2 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_parttab' and attname='added_col2';

--
-- @description AOCO multi_segfile table : add column with default value non NULL
alter table multi_segfile_parttab ADD COLUMN added_col1 character varying(35) default 'this is default value of non null';
select count(*) as added_col1 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_parttab' and attname='added_col1';

--
-- @description AOCO multi_segfile table : drop column with default value NULL
alter table multi_segfile_parttab ADD COLUMN added_col22 character varying(35) default NULL;
select count(*) as added_col22 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_parttab' and attname='added_col22';
alter table multi_segfile_parttab DROP COLUMN added_col22;
select count(*) as added_col22 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_parttab' and attname='added_col22';
VACUUM multi_segfile_parttab;

--
-- @description AOCO multi_segfile table : drop column with default value non NULL
alter table multi_segfile_parttab ADD COLUMN added_col11 character varying(35) default 'this is default value of non null';
select count(*) as added_col11 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_parttab' and attname='added_col11';
alter table multi_segfile_parttab DROP COLUMN added_col11;
select count(*) as added_col11 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_parttab' and attname='added_col11';
VACUUM multi_segfile_parttab;

--
-- @description AOCO multi_segfile table : add column toast with default value NULL

alter table multi_segfile_toast ADD COLUMN added_col3 bytea  default NULL;
select count(*) as added_col3 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_toast' and attname='added_col3';

--
-- @description AOCO multi_segfile table : add column with default value non NULL
alter table multi_segfile_toast ADD COLUMN added_col4  bytea default ("decode"(repeat('1234567890',10000),'escape'));
select count(*) as added_col4 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_toast' and attname='added_col4';

--
-- @description AOCO multi_segfile table : drop column with default value NULL
alter table multi_segfile_toast ADD COLUMN added_col33 bytea  default NULL;
select count(*) as added_col33 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_toast' and attname='added_col33';
alter table multi_segfile_toast DROP COLUMN added_col33;
select count(*) as added_col33 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_toast' and attname='added_col33';
VACUUM multi_segfile_toast;

--
-- @description AOCO multi_segfile table : drop column with default value non NULL
alter table multi_segfile_toast ADD COLUMN added_col44  bytea default ("decode"(repeat('1234567890',10000),'escape'));
select count(*) as added_col44 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_toast' and attname='added_col44';
alter table multi_segfile_toast DROP COLUMN added_col44;
select count(*) as added_col44 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_toast' and attname='added_col44';
VACUUM multi_segfile_toast;

--
-- @description AOCO multi_segfile table : add column with default value NULL
alter table multi_segfile_zlibtab ADD COLUMN added_col2 character varying(35) default NULL;
select count(*) as added_col2 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_zlibtab' and attname='added_col2';

--
-- @description AOCO multi_segfile table : add column with default value non NULL
alter table multi_segfile_zlibtab ADD COLUMN added_col1 character varying(35) default 'this is default value of non null';
select count(*) as added_col1 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_zlibtab' and attname='added_col1';

--
-- @description AOCO multi_segfile table : drop column with default value NULL
alter table multi_segfile_zlibtab ADD COLUMN added_col22 character varying(35) default NULL;
select count(*) as added_col22 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_zlibtab' and attname='added_col22';
alter table multi_segfile_zlibtab DROP COLUMN added_col22;
select count(*) as added_col22 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_zlibtab' and attname='added_col22';
VACUUM multi_segfile_zlibtab;

--
-- @description AOCO multi_segfile table : drop column with default value non NULL
alter table multi_segfile_zlibtab ADD COLUMN added_col11 character varying(35) default 'this is default value of non null';
select count(*) as added_col11 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_zlibtab' and attname='added_col11';
alter table multi_segfile_zlibtab DROP COLUMN added_col11;
select count(*) as added_col11 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multi_segfile_zlibtab' and attname='added_col11';
VACUUM multi_segfile_zlibtab;

--
-- @description AOCO multivarblock table : add column with default value NULL
alter table multivarblock_tab ADD COLUMN added_col2 character varying(35) default NULL;
select count(*) as added_col2 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_tab' and attname='added_col2';

--
-- @description AOCO multivarblock table : add column with default value non NULL
alter table multivarblock_tab ADD COLUMN added_col5 TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW() ;
select count(*) as added_col5 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_tab' and attname='added_col5';

--
-- @description AOCO multivarblock table : add column with constraint and default value non NULL
-- Negative test
alter table multivarblock_tab add column added_col66 int CONSTRAINT multivarblock_tab_check1 CHECK (added_col66 < 10)  default 30;
select count(*) as added_col66 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_tab' and attname='added_col66';
-- Positive test
alter table multivarblock_tab add column added_col66 int CONSTRAINT multivarblock_tab_check1 CHECK (added_col66 < 10)  default 5;
select count(*) as added_col66 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_tab' and attname='added_col66';

--
-- @description AOCO multivarblock table : add column with default value non NULL then add index on new column
alter table multivarblock_tab ADD COLUMN added_col50 TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW() ;
select count(*) as added_col50 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_tab' and attname='added_col50';
insert into multivarblock_tab (c_custkey, c_name, c_comment, c_rating, c_phone, c_acctbal,c_date, c_timestamp)
                     values( 500, 'acz','yet another  looong text' , 11.5, '778777',550.54,'2014/05/04',now());
create index multivarblock_tab_idx2 on multivarblock_tab(added_col50);

--
-- @description AOCO multivarblock table : add column with default value NULL
alter table multivarblock_bitab ADD COLUMN added_col2 character varying(35) default NULL;
select count(*) as added_col2 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_bitab' and attname='added_col2';

--
-- @description AOCO multivarblock table : add column with default value non NULL
alter table multivarblock_bitab ADD COLUMN added_col1 character varying(35) default 'this is default value of non null';
select count(*) as added_col1 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_bitab' and attname='added_col1';

--
-- @description AOCO multivarblock table : drop column with default value NULL
alter table multivarblock_bitab ADD COLUMN added_col22 character varying(35) default NULL;
select count(*) as added_col22 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_bitab' and attname='added_col22';

alter table multivarblock_bitab DROP COLUMN added_col22;
select count(*) as added_col22 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_bitab' and attname='added_col22';

VACUUM multivarblock_bitab;

--
-- @description AOCO multivarblock table : drop column with default value non NULL
alter table multivarblock_bitab ADD COLUMN added_col11 character varying(35) default 'this is default value of non null';
select count(*) as added_col11 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_bitab' and attname='added_col11';
alter table multivarblock_bitab DROP COLUMN added_col11;
select count(*) as added_col11 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_bitab' and attname='added_col11';
VACUUM multivarblock_bitab;

--
-- @description AOCO multivarblock table : drop column with default value NULL
alter table multivarblock_tab ADD COLUMN added_col22 character varying(35) default NULL;
select count(*) as added_col22 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_tab' and attname='added_col22';
alter table multivarblock_tab DROP COLUMN added_col22;
select count(*) as added_col22 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_tab' and attname='added_col22';
VACUUM multivarblock_tab;

--
-- @description AOCO multivarblock table : drop column with default value non NULL
alter table multivarblock_tab ADD COLUMN added_col55 TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW() ;
select count(*) as added_col55 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_tab' and attname='added_col55';
alter table multivarblock_tab DROP COLUMN added_col55;
select count(*) as added_col55 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_tab' and attname='added_col55';
VACUUM multivarblock_tab;

--
-- @description AOCO multivarblock table : add column with default value NULL
alter table multivarblock_parttab ADD COLUMN added_col2 character varying(35) default NULL;
select count(*) as added_col2 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_parttab' and attname='added_col2';

--
-- @description AOCO multivarblock table : add column with default value non NULL
alter table multivarblock_parttab ADD COLUMN added_col1 character varying(35) default 'this is default value of non null';
select count(*) as added_col1 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_parttab' and attname='added_col1';

--
-- @description AOCO multivarblock table : drop column with default value NULL
alter table multivarblock_parttab ADD COLUMN added_col22 character varying(35) default NULL;
select count(*) as added_col22 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_parttab' and attname='added_col22';
alter table multivarblock_parttab DROP COLUMN added_col22;
select count(*) as added_col22 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_parttab' and attname='added_col22';
VACUUM multivarblock_parttab;

--
-- @description AOCO multivarblock partition table : drop column with default value non NULL
alter table multivarblock_parttab ADD COLUMN added_col11 character varying(35) default 'this is default value of non null';
select count(*) as added_col11 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_parttab' and attname='added_col11';
alter table multivarblock_parttab DROP COLUMN added_col11;
select count(*) as added_col11 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_parttab' and attname='added_col11';
VACUUM multivarblock_parttab;

--
-- @description AOCO multivarblock table : add column toast with default value NULL

alter table multivarblock_toast ADD COLUMN added_col3 bytea  default NULL;
select count(*) as added_col3 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_toast' and attname='added_col3';

--
-- @description AOCO multivarblock table : add column with default value non NULL
alter table multivarblock_toast ADD COLUMN added_col4  bytea default ("decode"(repeat('1234567890',10000),'escape'));
select count(*) as added_col4 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_toast' and attname='added_col4';

--
-- @description AOCO multivarblock table : drop column with default value NULL
alter table multivarblock_toast ADD COLUMN added_col33 bytea  default NULL;
select count(*) as added_col33 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_toast' and attname='added_col33';
alter table multivarblock_toast DROP COLUMN added_col33;
select count(*) as added_col33 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_toast' and attname='added_col33';
VACUUM multivarblock_toast;

--
-- @description AOCO multivarblock table : drop column with default value non NULL
alter table multivarblock_toast ADD COLUMN added_col44  bytea default ("decode"(repeat('1234567890',10000),'escape'));
select count(*) as added_col44 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_toast' and attname='added_col44';
alter table multivarblock_toast DROP COLUMN added_col44;
select count(*) as added_col44 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_toast' and attname='added_col44';
VACUUM multivarblock_toast;

--
-- @description AOCO multivarblock table : add column with default value NULL
alter table multivarblock_zlibtab ADD COLUMN added_col2 character varying(35) default NULL;
select count(*) as added_col2 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_zlibtab' and attname='added_col2';

--
-- @description AOCO multivarblock table : add column with default value non NULL
alter table multivarblock_zlibtab ADD COLUMN added_col1 character varying(35) default 'this is default value of non null';
select count(*) as added_col1 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_zlibtab' and attname='added_col1';

--
-- @description AOCO multivarblock table : drop column with default value NULL
alter table multivarblock_zlibtab ADD COLUMN added_col22 character varying(35) default NULL;
select count(*) as added_col22 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_zlibtab' and attname='added_col22';
alter table multivarblock_zlibtab DROP COLUMN added_col22;
select count(*) as added_col22 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_zlibtab' and attname='added_col22';
VACUUM multivarblock_zlibtab;

--
-- @description AOCO multivarblock zlib compressed table : drop column with default value non NULL
alter table multivarblock_zlibtab ADD COLUMN added_col11 character varying(35) default 'this is default value of non null';
select count(*) as added_col11 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_zlibtab' and attname='added_col11';
alter table multivarblock_zlibtab DROP COLUMN added_col11;
select count(*) as added_col11 from pg_attribute pa, pg_class pc where pa.attrelid = pc.oid and pc.relname='multivarblock_zlibtab' and attname='added_col11';
VACUUM multivarblock_zlibtab;
--
-- Non-bulk dense content header with RLE compression
-- This will  insert more rows than a small content header can accommodate in the same insert statement
drop table if exists nonbulk_rle_tab;
create table nonbulk_rle_tab (a int) with (appendonly=true, orientation=column, compresstype='rle_type', checksum=true);
insert into nonbulk_rle_tab select i/50 from generate_series(1, 1000000)i;
alter table nonbulk_rle_tab add column b int default round(random()*100);
insert into nonbulk_rle_tab values (-1,-5);
ANALYZE nonbulk_rle_tab; -- To avoid NOTICE about missing stats with ORCA.
update nonbulk_rle_tab set b = b + 3 where a = -1;


CREATE FUNCTION descendants_of(rel regclass) RETURNS TABLE(descendant regclass)
SET gp_recursive_cte=ON
LANGUAGE SQL STABLE AS $fn$
WITH RECURSIVE w AS (
  SELECT rel AS descendant
  UNION ALL
  SELECT inhrelid
  FROM pg_inherits JOIN w ON inhparent = descendant
)
SELECT * FROM w;
$fn$;

-- Scenario 1: Parent is AOCO, 1 child is AO, 1 child is heap. AO children require full table rewrite
CREATE TABLE rewrite_optimization_aoco_parent(a int, b int, c int)
WITH (APPENDONLY = true, ORIENTATION = column)
DISTRIBUTED BY (a)
    PARTITION BY RANGE (b)
        (START (0) END (1) EVERY (1) WITH (APPENDONLY = true),
        START (1) END (2) EVERY (1) WITH (APPENDONLY = false));
SELECT $$
SELECT -1 AS segno, oid::regclass AS rel, relfilenode, (select amname from pg_am where pg_am.oid = relam)
FROM pg_class c
WHERE c.oid IN (SELECT descendant FROM descendants_of(:'ROOT_PARTITION_UNDER_TEST'))
UNION ALL
SELECT gp_segment_id, oid::regclass, relfilenode, (select amname from pg_am where pg_am.oid = relam)
FROM gp_dist_random('pg_class') c
WHERE c.oid IN (SELECT descendant FROM descendants_of(:'ROOT_PARTITION_UNDER_TEST'))
$$ AS qry \gset

SELECT $$
SELECT t.segno,
       t.rel,
       t.amname,
       CASE
           WHEN table_relfilenode.segno IS NULL THEN 'full table rewritten'
           ELSE 'ADD COLUMN optimized for table' END AS aoco_add_col_optimized
FROM (:qry) t
         LEFT JOIN table_relfilenode USING (segno, rel, relfilenode)
WHERE t.segno IN (-1, 0)
ORDER BY 1, 2;
$$ AS chk_co_opt_qry \gset

\set ROOT_PARTITION_UNDER_TEST rewrite_optimization_aoco_parent
CREATE MATERIALIZED VIEW table_relfilenode (segno, rel, relfilenode)
AS
:qry
;

INSERT INTO rewrite_optimization_aoco_parent SELECT i, i % 2, i FROM generate_series(1,10)i;

ALTER TABLE rewrite_optimization_aoco_parent ADD COLUMN new_col int DEFAULT 1;

SELECT * FROM rewrite_optimization_aoco_parent;

:chk_co_opt_qry;

-- ADD COLUMN for AOCO partition children should not trigger full table rewrite
-- Scenario 2: Parent is heap, 1 child is AO, 1 child is AOCO. AO child requires full table rewrite. AOCO child does not.
CREATE TABLE rewrite_optimization_heap_parent(a int, b int, c int) DISTRIBUTED BY (a)
    PARTITION BY RANGE (b)
        (START (0) END (1) EVERY (1) WITH (APPENDONLY = true),
        START (1) END (2) EVERY (1) WITH (APPENDONLY = true, ORIENTATION = column));

\set ROOT_PARTITION_UNDER_TEST rewrite_optimization_heap_parent
DROP MATERIALIZED VIEW table_relfilenode;
CREATE MATERIALIZED VIEW table_relfilenode (segno, rel, relfilenode)
AS
:qry
;

INSERT INTO rewrite_optimization_heap_parent SELECT i, i % 2, i FROM generate_series(1,10)i;

ALTER TABLE rewrite_optimization_heap_parent ADD COLUMN new_col int DEFAULT 1;

SELECT * FROM rewrite_optimization_heap_parent;

:chk_co_opt_qry;

REFRESH MATERIALIZED VIEW table_relfilenode;
ALTER TABLE rewrite_optimization_heap_parent ADD COLUMN new_col2 int DEFAULT 1, ALTER COLUMN c TYPE bigint;

SELECT * FROM rewrite_optimization_heap_parent;

:chk_co_opt_qry;

-- Parent is heap, child is heap, grandchild is AOCO.
CREATE TABLE subpartition_aoco_leaf(a int, b int, c int)
DISTRIBUTED BY (a)
  PARTITION BY RANGE(b) SUBPARTITION BY RANGE(c) 
      (PARTITION intermediate START (0) END (2)
        (SUBPARTITION leaf START (0) END(2) WITH (APPENDONLY = true, ORIENTATION = column))
      );

INSERT INTO subpartition_aoco_leaf SELECT i, i % 2, i % 2 FROM generate_series(1,10)i;
\set ROOT_PARTITION_UNDER_TEST subpartition_aoco_leaf

DROP MATERIALIZED VIEW table_relfilenode;
CREATE MATERIALIZED VIEW table_relfilenode (segno, rel, relfilenode)
AS
:qry
;

-- Scenario 1: ADD COLUMN for AOCO subpartitioned table should not trigger full
-- table rewrite. AOCO leaf only writes new column
ALTER TABLE subpartition_aoco_leaf ADD COLUMN new_col int DEFAULT 1;

SELECT * FROM subpartition_aoco_leaf;

:chk_co_opt_qry;

REFRESH MATERIALIZED VIEW table_relfilenode;

-- Scenario 2: mixing ADD COLUMN with ALTER COLUMN TYPE should trigger full on aoco
-- table rewrite for every level
ALTER TABLE subpartition_aoco_leaf ADD COLUMN new_col2 int DEFAULT 1, ALTER COLUMN new_col TYPE bigint;

SELECT * FROM subpartition_aoco_leaf;

:chk_co_opt_qry;

-- Check if add column doesn't rewrite the table
CREATE TABLE testaddcol(i int) WITH (appendonly=true, orientation=column);
INSERT INTO testaddcol SELECT generate_series(1, 5);

CREATE TEMP TABLE relfilebeforetestaddcol AS
    SELECT -1 segid, relname, relfilenode FROM pg_class WHERE relname LIKE 'testaddcol%'
    UNION SELECT gp_segment_id segid, relname, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'testaddcol%' ORDER BY segid;

ALTER TABLE testaddcol ADD COLUMN j int DEFAULT 5;

CREATE TEMP TABLE relfileaftertestaddcol AS
    SELECT -1 segid, relname, relfilenode FROM pg_class WHERE relname LIKE 'testaddcol%'
    UNION SELECT gp_segment_id segid, relname, relfilenode FROM gp_dist_random('pg_class')
    WHERE relname LIKE 'testaddcol%' ORDER BY segid;

-- table shouldn't be rewritten
SELECT count(*) FROM (SELECT * FROM relfilebeforetestaddcol UNION SELECT * FROM relfileaftertestaddcol)a;

-- data is intact
SELECT * FROM testaddcol;
