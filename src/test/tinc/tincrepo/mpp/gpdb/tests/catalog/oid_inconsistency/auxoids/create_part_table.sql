set gp_default_storage_options='appendonly=true,orientation=column';
drop table if exists co1;
CREATE TABLE co1 (
 c1 int4,
 c2 int4) distributed by (c1)
 partition by list (c1)
 ( partition aa values (1,2,3,4,5),
   partition bb values (6,7,8,9,10),
   partition cc values (11,12,13,14,15),
   default partition default_part );

select inhrelid::regclass from pg_inherits
 where inhparent = 'co1'::regclass;
