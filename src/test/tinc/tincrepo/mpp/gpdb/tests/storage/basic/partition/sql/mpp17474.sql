--start_ignore
drop table if exists mpp17740 cascade;
--end_ignore

create table mpp17740 (a varchar(100), b varchar(100),c varchar(100), d int, e date)WITH (appendonly=true, orientation=column, compresstype=zlib, compresslevel=5) 
DISTRIBUTED BY (d) 
PARTITION BY RANGE(e)  
( 
    partition mpp17740_20120523 start ('2012-05-23'::date) inclusive end ('2012-05-24'::date) exclusive WITH (tablename='mpp17740_20120523', orientation=column, appendonly=true, compresstype=zlib, compresslevel=5 ) ,
 
    partition mpp17740_20120524 start ('2012-05-24'::date) inclusive end ('2012-05-25'::date) exclusive  WITH (tablename='mpp17740_20120524', orientation=column, appendonly=true, compresstype=zlib, compresslevel=5 )

);


\d+ mpp17740

select partitiontablename,partitionrangestart,partitionrangeend from pg_partitions where tablename='mpp17740' order by partitiontablename;

-- AO tables and zlib

alter table mpp17740 add partition  mpp17740_20120520 start ('2012-05-20'::date) inclusive end ('2012-05-21'::date) exclusive WITH (tablename='mpp17740_20120520',  appendonly=true, compresstype=zlib, compresslevel=5 );

alter table mpp17740 add partition  mpp17740_20120521 start ('2012-05-21'::date) inclusive end ('2012-05-22'::date) exclusive WITH (tablename='mpp17740_20120521', appendonly=true, compresstype=zlib, compresslevel=9 );

select partitiontablename,partitionrangestart,partitionrangeend from pg_partitions where tablename='mpp17740' order by partitiontablename;
-- CO table and zlib

alter table mpp17740 add partition  mpp17740_20120525 start ('2012-05-25'::date) inclusive end ('2012-05-26'::date) exclusive WITH (tablename='mpp17740_20120525', orientation=column, appendonly=true, compresstype=zlib, compresslevel=5 );

alter table mpp17740 add partition  mpp17740_20120522 start ('2012-05-22'::date) inclusive end ('2012-05-23'::date) exclusive WITH (tablename='mpp17740_20120522', orientation=column, appendonly=true, compresstype=zlib, compresslevel=9 );

select partitiontablename,partitionrangestart,partitionrangeend from pg_partitions where tablename='mpp17740' order by partitiontablename;
-- AO and none

alter table mpp17740 add partition  mpp17740_20120519 start ('2012-05-19'::date) inclusive end ('2012-05-20'::date) exclusive WITH (tablename='mpp17740_20120519', appendonly=true, compresstype=none );

--CO and None
alter table mpp17740 add partition  mpp17740_20120526 start ('2012-05-26'::date) inclusive end ('2012-05-27'::date) exclusive WITH (tablename='mpp17740_20120526', orientation=column, appendonly=true, compresstype=none );

select partitiontablename,partitionrangestart,partitionrangeend from pg_partitions where tablename='mpp17740' order by partitiontablename;

-- AO and quicklz

alter table mpp17740 add partition  mpp17740_20120517 start ('2012-05-17'::date) inclusive end ('2012-05-18'::date) exclusive WITH (tablename='mpp17740_20120517',  appendonly=true, compresstype= quicklz, compresslevel=1 );

-- CO and quicklz

alter table mpp17740 add partition  mpp17740_20120518 start ('2012-05-18'::date) inclusive end ('2012-05-19'::date) exclusive WITH (tablename='mpp17740_20120518', orientation=column, appendonly=true, compresstype= quicklz, compresslevel=1 );

select partitiontablename,partitionrangestart,partitionrangeend from pg_partitions where tablename='mpp17740' order by partitiontablename;
-- AO and CO
alter table mpp17740 add partition  mpp17740_20120527 start ('2012-05-27'::date) inclusive end ('2012-05-28'::date) exclusive WITH (tablename='mpp17740_20120527', orientation=column, appendonly=true);

alter table mpp17740 add partition  mpp17740_20120229 start ('2012-05-28'::date) inclusive end ('2012-05-29'::date) exclusive WITH (tablename='mpp17740_20120528', appendonly=true);

select partitiontablename,partitionrangestart,partitionrangeend from pg_partitions where tablename='mpp17740' order by partitiontablename;


