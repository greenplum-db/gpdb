drop table if exists bms_ao_bug;
create table bms_ao_bug
(c1 int not null,
c2 int not null,
c3 char(100) not null)
with (appendonly=true, orientation=column, compresstype=zlib)
distributed by (c1)
;

insert into bms_ao_bug select 1, 1, a.c1::char(100) from generate_series(1, 2000000) a(c1);

create index bms_ao_bug_ix1 on bms_ao_bug (c2);

set enable_seqscan=off;
set work_mem=256;

select 
a.c1,
count(*) row_cnt
from bms_ao_bug a 
where a.c2 = 1
group by 
a.c1
;

drop table bms_ao_bug;
