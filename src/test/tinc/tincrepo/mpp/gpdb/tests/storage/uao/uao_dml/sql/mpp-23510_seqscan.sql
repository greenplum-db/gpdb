-- @product_version gpdb: [4.3.1.0-4.3.99.99]
-- Bug verification for mpp-23510
drop table  if exists mpp23510 cascade;
create table mpp23510 (a int, b int, c char(128)) with (appendonly=true);
insert into mpp23510 select i, i-2 ,'Hello World'  from generate_series(-10,10) i;
create index mpp23510_idx on mpp23510(a);
-- force an index scan
set enable_seqscan=off;
select count(*) from mpp23510 where a < 5;
delete from mpp23510 where a >= 0 and a < 3;
select count(*) from mpp23510 where a < 5;
-- Restore scan back to sequential/index
set enable_seqscan=on;
select count(*) from mpp23510 where a < 5;
insert into mpp23510 select i, i-2 ,'Hello World2'  from generate_series(-11,-15) i;
select count(*) from mpp23510 where a < 7;
delete from mpp23510 where a >= 0 and a < 7;
select count(*) from mpp23510 where a < 7;
set enable_seqscan=off;
select count(*) from mpp23510 where a < 7;
