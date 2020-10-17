drop schema if exists misc_jiras;
create schema misc_jiras;

--
-- Test backward scanning of tuplestore spill files.
--
-- When tuplestore cannot store all the data in memory it will spill some of
-- the data to temporary files.  In gpdb we used to disable the backward
-- scanning from these spill files because we could not determine the tuple
-- type, memtup or heaptup, correctly.  The issue is fixed, the backward
-- scanning should be supported now.
--

create table misc_jiras.t1 (c1 int, c2 text, c3 smallint) distributed by (c1);
insert into misc_jiras.t1 select i % 13, md5(i::text), i % 3
  from generate_series(1, 20000) i;

-- tuplestore uses work_mem to control the in-memory data size, set a small
-- value to trigger the spilling.
set work_mem to '64kB';

set extra_float_digits=0; -- the last decimal digits are somewhat random

select sum(cc) from (
    select c1
         , c2
         , case when count(c3) = 0 then -1.0
                else cume_dist() over (partition by c1,
                                       case when count(c3) > 0 then 1 else 0 end
                                       order by count(c3), c2)
           end as cc
      from misc_jiras.t1
     group by 1, 2
) tt;

reset work_mem;
