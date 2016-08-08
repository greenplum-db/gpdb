--Create the table with 4 partitions
--start_ignore
drop table if exists mpp13806 cascade;
--end_ignore
CREATE TABLE mpp13806 (id int, date date, amt decimal(10,2))
DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
( START (date '2008-01-01') INCLUSIVE
 END (date '2008-01-05') EXCLUSIVE
 EVERY (INTERVAL '1 day') );

-- Adding unbound partition right before the start succeeds
alter table mpp13806 add partition test end (date '2008-01-01') exclusive;
select partitiontablename, partitionrangestart, partitionstartinclusive, partitionrangeend, partitionendinclusive from pg_partitions where tablename like 'mpp13806%' order by partitionrank;


--Drop the partition
alter TABLE mpp13806 drop partition test;

-- Adding unbound partition with a gap succeeds
alter table mpp13806 add partition test end (date '2007-12-31') exclusive;
select partitiontablename, partitionrangestart, partitionstartinclusive, partitionrangeend, partitionendinclusive from pg_partitions where tablename like 'mpp13806%' order by partitionrank;

-- Filling the gap succeeds/adding immediately before the first partition succeeds

alter table mpp13806 add partition test1 start (date '2007-12-31') inclusive end (date '2008-01-01') exclusive;
select partitiontablename, partitionrangestart, partitionstartinclusive, partitionrangeend, partitionendinclusive from pg_partitions where tablename like 'mpp13806%' order by partitionrank; 
