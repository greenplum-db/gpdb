-- Create two table mpp14613_range( range partitioned) and table mpp14613_list(list partitioned) with 5 partitions( including default partition) and 3 subpartitions (including default subpartition) each
--start_ignore
drop table if exists mpp14613_list cascade;
drop table if exists mpp14613_range cascade;
--end_ignore

create table mpp14613_list( 
  a int, 
  b int, 
  c int, 
  d int)
  partition by range(b)
  subpartition by list( c)
  subpartition template
 ( 
    default subpartition subothers,
    subpartition s1 values(1,2,3), 
    subpartition s2 values(4,5,6)
 ) 
 (
    default partition others,
    start(1) end(5) every(1)
 );

create table mpp14613_range(
  a int, 
  b int, 
  c int,
  d int 
 )  
  partition by range(b) 
  subpartition by range( c )
  subpartition template
 ( 
     default subpartition subothers,
     start (1) end(7) every (3)
 )   
 ( 
     default partition others, 
     start(1) end(5) every(1)
 );

-- Check the partition and subpartition details

    select tablename,partitiontablename, partitionname from pg_partitions where tablename in ('mpp14613_list','mpp14613_range');   

-- SPLIT partition
alter table mpp14613_list alter partition others split partition subothers at (10) into (partition b1, partition b2);
alter table mpp14613_range alter partition others split partition subothers at (10) into (partition b1, partition b2);


