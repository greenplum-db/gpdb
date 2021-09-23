create extension if not exists gp_debug_numsegments;
select gp_debug_set_create_table_default_numsegments(1);

--only partition table can be expanded partition prepare
drop table if exists t_hash_expand_prepare;
create table t_hash_expand_prepare (c1 int, c2 int, c3 int, c4 int) distributed by (c1, c2);
alter table t_hash_expand_prepare expand partition prepare;
drop table t_hash_expand_prepare;

--partition table distributed by hash
drop table if exists t_hash_partition;
create table t_hash_partition(a int,b int,c int)
 partition by range (a)
 ( start (1) end (20) every(10),
   default partition extra
 );
 
insert into t_hash_partition select i,i,i from generate_series(1,30) i;

--only parent of partition table can be expanded partition prepare
alter table t_hash_partition_1_prt_2 expand partition prepare;
--master policy info
select localoid::regclass, policytype, numsegments, distkey, distclass
	from gp_distribution_policy where localoid in (
		't_hash_partition_1_prt_2'::regclass, 't_hash_partition_1_prt_3'::regclass,
		't_hash_partition_1_prt_extra'::regclass, 't_hash_partition'::regclass);
--segment policy info
select gp_segment_id, localoid::regclass, policytype, numsegments, distkey, distclass
	from gp_dist_random('gp_distribution_policy') where localoid in (
		't_hash_partition_1_prt_2'::regclass, 't_hash_partition_1_prt_3'::regclass,
		't_hash_partition_1_prt_extra'::regclass, 't_hash_partition'::regclass);

alter table t_hash_partition expand partition prepare;

--master policy info
select localoid::regclass, policytype, numsegments, distkey, distclass
	from gp_distribution_policy where localoid in (
		't_hash_partition_1_prt_2'::regclass, 't_hash_partition_1_prt_3'::regclass,
		't_hash_partition_1_prt_extra'::regclass, 't_hash_partition'::regclass);
--segment policy info
select gp_segment_id, localoid::regclass, policytype, numsegments, distkey, distclass
	from gp_dist_random('gp_distribution_policy') where localoid in (
		't_hash_partition_1_prt_2'::regclass, 't_hash_partition_1_prt_3'::regclass,
		't_hash_partition_1_prt_extra'::regclass, 't_hash_partition'::regclass);


alter table t_hash_partition expand partition prepare;

--dml of parent table
select count(*) from t_hash_partition;
select count(*) from t_hash_partition where a=1;
select count(*) from t_hash_partition where a=5;

insert into t_hash_partition select i,i,i from generate_series(1,30) i;

select count(*) from t_hash_partition;
select count(*) from t_hash_partition where a=1;
select count(*) from t_hash_partition where a=3;

delete from t_hash_partition where a=1;
select count(*) from t_hash_partition where a=1;
select count(*) from t_hash_partition;

update t_hash_partition set a = a+1;
select count(*) from t_hash_partition where a=3;
select count(*) from t_hash_partition; 

--dml of child table
select count(*) from t_hash_partition_1_prt_2;
select count(*) from t_hash_partition_1_prt_2 where a=2;
insert into t_hash_partition_1_prt_2 values(8,1,1);
select count(*) from t_hash_partition_1_prt_2;
select count(*) from t_hash_partition;

drop table t_hash_partition;

--partition table distributed randomly 

select gp_debug_set_create_table_default_numsegments(2);
drop table if exists t_randomly_partition;
create table t_randomly_partition(a int,b int,c int) distributed randomly
 partition by range (a)
 ( start (1) end (20) every(10),
   default partition extra
 );
 
insert into t_randomly_partition select i,i,i from generate_series(1,30) i;

--only parent of partition table can be expanded partition prepare
alter table t_randomly_partition_1_prt_2 expand partition prepare;
--master policy info
select localoid::regclass, policytype, numsegments, distkey, distclass
	from gp_distribution_policy where localoid in (
		't_randomly_partition_1_prt_2'::regclass, 't_randomly_partition_1_prt_3'::regclass,
		't_randomly_partition_1_prt_extra'::regclass, 't_randomly_partition'::regclass);
--segment policy info
select gp_segment_id, localoid::regclass, policytype, numsegments, distkey, distclass
	from gp_dist_random('gp_distribution_policy') where localoid in (
		't_randomly_partition_1_prt_2'::regclass, 't_randomly_partition_1_prt_3'::regclass,
		't_randomly_partition_1_prt_extra'::regclass, 't_randomly_partition'::regclass);

alter table t_randomly_partition expand partition prepare;

--master policy info
select localoid::regclass, policytype, numsegments, distkey, distclass
	from gp_distribution_policy where localoid in (
		't_randomly_partition_1_prt_2'::regclass, 't_randomly_partition_1_prt_3'::regclass,
		't_randomly_partition_1_prt_extra'::regclass, 't_randomly_partition'::regclass);
--segment policy info
select gp_segment_id, localoid::regclass, policytype, numsegments, distkey, distclass
	from gp_dist_random('gp_distribution_policy') where localoid in (
		't_randomly_partition_1_prt_2'::regclass, 't_randomly_partition_1_prt_3'::regclass,
		't_randomly_partition_1_prt_extra'::regclass, 't_randomly_partition'::regclass);
		
alter table t_randomly_partition expand partition prepare;

--dml of parent table
select count(*) from t_randomly_partition;
select count(*) from t_randomly_partition where a=1;

insert into t_randomly_partition select i,i,i from generate_series(1,30) i;

select count(*) from t_randomly_partition;
select count(*) from t_randomly_partition where a=1;

delete from t_randomly_partition where a=1;
select count(*) from t_randomly_partition where a=1;
select count(*) from t_randomly_partition;

update t_randomly_partition set a = a+1;
select count(*) from t_randomly_partition where a=3;
select count(*) from t_randomly_partition; 

--dml of child table
select count(*) from t_randomly_partition_1_prt_2;
select count(*) from t_randomly_partition_1_prt_2 where a=2;
insert into t_randomly_partition_1_prt_2 values(8,1,1);
select count(*) from t_randomly_partition_1_prt_2;
select count(*) from t_randomly_partition;

drop table t_randomly_partition;

--subpartition table distributed hash
select gp_debug_set_create_table_default_numsegments(2);
drop table if exists t_hash_subpartition;
create table t_hash_subpartition
(
	r_regionkey integer not null,
	r_name char(25)
)
partition by range (r_regionkey)
subpartition by list (r_name) subpartition template
(
	subpartition CHINA values ('CHINA'),
	subpartition america values ('AMERICA')
)
(
	partition region1 start (0),
	partition region2 start (3),
	partition region3 start (5) end (8)
);
 
insert into t_hash_subpartition values(2,'CHINA');
insert into t_hash_subpartition values(4,'CHINA');
insert into t_hash_subpartition values(6,'CHINA');
insert into t_hash_subpartition values(1,'AMERICA');
insert into t_hash_subpartition values(3,'AMERICA');
insert into t_hash_subpartition values(5,'AMERICA');

--only parent of partition table can be expanded partition prepare
alter table t_hash_subpartition_1_prt_region1 expand partition prepare;
alter table t_hash_subpartition_1_prt_region1_2_prt_china expand partition prepare;
--master policy info
select localoid::regclass, policytype, numsegments, distkey, distclass
	from gp_distribution_policy where localoid in (
		't_hash_subpartition'::regclass,
		't_hash_subpartition_1_prt_region1'::regclass,
		't_hash_subpartition_1_prt_region1_2_prt_china'::regclass,
		't_hash_subpartition_1_prt_region1_2_prt_america'::regclass,
		't_hash_subpartition_1_prt_region2'::regclass,
		't_hash_subpartition_1_prt_region2_2_prt_china'::regclass,
		't_hash_subpartition_1_prt_region2_2_prt_america'::regclass,
		't_hash_subpartition_1_prt_region3'::regclass,
		't_hash_subpartition_1_prt_region3_2_prt_china'::regclass,
		't_hash_subpartition_1_prt_region3_2_prt_america'::regclass);
--segment policy info
select gp_segment_id, localoid::regclass, policytype, numsegments, distkey, distclass
	from gp_dist_random('gp_distribution_policy') where localoid in (
		't_hash_subpartition'::regclass,
		't_hash_subpartition_1_prt_region1'::regclass,
		't_hash_subpartition_1_prt_region1_2_prt_china'::regclass,
		't_hash_subpartition_1_prt_region1_2_prt_america'::regclass,
		't_hash_subpartition_1_prt_region2'::regclass,
		't_hash_subpartition_1_prt_region2_2_prt_china'::regclass,
		't_hash_subpartition_1_prt_region2_2_prt_america'::regclass,
		't_hash_subpartition_1_prt_region3'::regclass,
		't_hash_subpartition_1_prt_region3_2_prt_china'::regclass,
		't_hash_subpartition_1_prt_region3_2_prt_america'::regclass);
alter table t_hash_subpartition expand partition prepare;

--master policy info
select localoid::regclass, policytype, numsegments, distkey, distclass
	from gp_distribution_policy where localoid in (
		't_hash_subpartition'::regclass,
		't_hash_subpartition_1_prt_region1'::regclass,
		't_hash_subpartition_1_prt_region1_2_prt_china'::regclass,
		't_hash_subpartition_1_prt_region1_2_prt_america'::regclass,
		't_hash_subpartition_1_prt_region2'::regclass,
		't_hash_subpartition_1_prt_region2_2_prt_china'::regclass,
		't_hash_subpartition_1_prt_region2_2_prt_america'::regclass,
		't_hash_subpartition_1_prt_region3'::regclass,
		't_hash_subpartition_1_prt_region3_2_prt_china'::regclass,
		't_hash_subpartition_1_prt_region3_2_prt_america'::regclass);
--segment policy info
select gp_segment_id, localoid::regclass, policytype, numsegments, distkey, distclass
	from gp_dist_random('gp_distribution_policy') where localoid in (
		't_hash_subpartition'::regclass,
		't_hash_subpartition_1_prt_region1'::regclass,
		't_hash_subpartition_1_prt_region1_2_prt_china'::regclass,
		't_hash_subpartition_1_prt_region1_2_prt_america'::regclass,
		't_hash_subpartition_1_prt_region2'::regclass,
		't_hash_subpartition_1_prt_region2_2_prt_china'::regclass,
		't_hash_subpartition_1_prt_region2_2_prt_america'::regclass,
		't_hash_subpartition_1_prt_region3'::regclass,
		't_hash_subpartition_1_prt_region3_2_prt_china'::regclass,
		't_hash_subpartition_1_prt_region3_2_prt_america'::regclass);

alter table t_hash_subpartition expand partition prepare;

--dml of parent table
select count(*) from t_hash_subpartition;
select count(*) from t_hash_subpartition where r_regionkey=1;
select count(*) from t_hash_subpartition where r_regionkey=5;

insert into t_hash_subpartition values(1,'CHINA');
insert into t_hash_subpartition values(2,'CHINA');
insert into t_hash_subpartition values(3,'CHINA');
insert into t_hash_subpartition values(4,'AMERICA');
insert into t_hash_subpartition values(5,'AMERICA');
insert into t_hash_subpartition values(6,'AMERICA');

select count(*) from t_hash_subpartition;
select count(*) from t_hash_subpartition where r_regionkey=1;
select count(*) from t_hash_subpartition where r_regionkey=5;

delete from t_hash_subpartition where r_regionkey=1;
select count(*) from t_hash_subpartition where r_regionkey=1;
select count(*) from t_hash_subpartition;

update t_hash_subpartition set r_regionkey = r_regionkey+1;
select count(*) from t_hash_subpartition where r_regionkey=3;
select count(*) from t_hash_subpartition; 

--dml of child table
select count(*) from t_hash_subpartition_1_prt_region1;
insert into t_hash_subpartition_1_prt_region1 values(1,'CHINA');
select count(*) from t_hash_subpartition_1_prt_region1;
select count(*) from t_hash_subpartition;

--dml of subchild table
select * from t_hash_subpartition_1_prt_region1_2_prt_china;
insert into t_hash_subpartition_1_prt_region1_2_prt_china values(1,'CHINA');
select count(*) from t_hash_subpartition_1_prt_region1_2_prt_china;
select count(*) from t_hash_subpartition_1_prt_region1;
select count(*) from t_hash_subpartition;

drop table t_hash_subpartition;

--cleanup
select gp_debug_reset_create_table_default_numsegments();
drop extension gp_debug_numsegments;


