-- MPP-18179 CLONE (4.2.3) - Unable to add partitions if the partitioned column has been altered from timestamp to date
-- start_ignore
drop table if exists mpp18179;
-- end_ignore

create table mpp18179 (a int, b int, i int)
distributed by (a)
partition by list (a,b) 
   ( PARTITION ab1 VALUES ((1,1)),
     PARTITION ab2 values ((1,2)),
     default partition other
   );

alter table mpp18179 alter column a type varchar(20);
alter table mpp18179 alter column b type varchar(20);

-- start_ignore
drop table if exists mpp18179;
-- end_ignore
