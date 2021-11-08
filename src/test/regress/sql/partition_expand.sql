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

--------------------------------------------------------
--test for set distributed of partition table
select gp_debug_set_create_table_default_numsegments(2);

--subpartition distributed by hash
drop table if exists t_hash_subpartition;
create table t_hash_subpartition
(
	r_regionkey integer not null,
	r_name char(25),
	r_comment varchar(152)
)
partition by range (r_regionkey)
subpartition by list (r_name) subpartition template
(
	subpartition china values ('CHINA'),
	subpartition america values ('AMERICA')
)
(
	partition region1 start (0),
	partition region2 start (3),
	partition region3 start (5) end (8)
);

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
--can not set distributed for interior parts of partition table
alter table t_hash_subpartition_1_prt_region1 set distributed randomly;

--can not set distributed for interior parts of partition table
alter table t_hash_subpartition_1_prt_region1 set distributed by(r_regionkey);

--error when the policy of leaf is different of parent's
alter table t_hash_subpartition_1_prt_region1_2_prt_china set distributed randomly;
--the policy of leaf is the same as parent's
alter table t_hash_subpartition_1_prt_region1_2_prt_china set distributed by(r_regionkey);
--ok
alter table t_hash_subpartition set distributed randomly;
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
--expand partition prepare
drop table t_hash_subpartition;
create table t_hash_subpartition
(
	r_regionkey integer not null,
	r_name char(25),
	r_comment varchar(152)
)
partition by range (r_regionkey)
subpartition by list (r_name) subpartition template
(
	subpartition china values ('CHINA'),
	subpartition america values ('AMERICA')
)
(
	partition region1 start (0),
	partition region2 start (3),
	partition region3 start (5) end (8)
);
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

alter table t_hash_subpartition expand partition prepare;
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

--can not set distributed for interior parts of partition table
alter table t_hash_subpartition_1_prt_region2 set  distributed randomly;
alter table t_hash_subpartition_1_prt_region2 set  distributed by (r_regionkey);
--the policy of leaf is the same as original
alter table t_hash_subpartition_1_prt_region1_2_prt_china set distributed randomly;
--the policy of leaf is the same as parent's
alter table t_hash_subpartition_1_prt_region1_2_prt_china set distributed by (r_regionkey);
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
--alter root of partition table
alter table t_hash_subpartition set distributed by (r_regionkey);
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

drop table t_hash_subpartition;

--subpartition distributed randomly
drop table if exists t_random_subpartition;
create table t_random_subpartition
(
	r_regionkey integer not null,
	r_name char(25),
	r_comment varchar(152)
) distributed randomly
partition by range (r_regionkey)
subpartition by list (r_name) subpartition template
(
	subpartition china values ('CHINA'),
	subpartition america values ('AMERICA')
)
(
	partition region1 start (0),
	partition region2 start (3),
	partition region3 start (5) end (8)
);
select localoid::regclass, policytype, numsegments, distkey, distclass
	from gp_distribution_policy where localoid in (
		't_random_subpartition'::regclass,
		't_random_subpartition_1_prt_region1'::regclass,
		't_random_subpartition_1_prt_region1_2_prt_china'::regclass,
		't_random_subpartition_1_prt_region1_2_prt_america'::regclass,
		't_random_subpartition_1_prt_region2'::regclass,
		't_random_subpartition_1_prt_region2_2_prt_china'::regclass,
		't_random_subpartition_1_prt_region2_2_prt_america'::regclass,
		't_random_subpartition_1_prt_region3'::regclass,
		't_random_subpartition_1_prt_region3_2_prt_china'::regclass,
		't_random_subpartition_1_prt_region3_2_prt_america'::regclass);

--can not set distributed for interior parts of partition table
alter table t_random_subpartition_1_prt_region1 set distributed randomly;
alter table t_random_subpartition_1_prt_region1 set distributed by(r_regionkey);

--the policy of leaf is the same as original
alter table t_random_subpartition_1_prt_region1_2_prt_china set distributed randomly;
--error, the policy of leaf is different from parent's
alter table t_random_subpartition_1_prt_region1_2_prt_china set distributed by(r_regionkey);
--alter root of partition table
alter table t_random_subpartition set distributed  by(r_regionkey);
select localoid::regclass, policytype, numsegments, distkey, distclass
	from gp_distribution_policy where localoid in (
		't_random_subpartition'::regclass,
		't_random_subpartition_1_prt_region1'::regclass,
		't_random_subpartition_1_prt_region1_2_prt_china'::regclass,
		't_random_subpartition_1_prt_region1_2_prt_america'::regclass,
		't_random_subpartition_1_prt_region2'::regclass,
		't_random_subpartition_1_prt_region2_2_prt_china'::regclass,
		't_random_subpartition_1_prt_region2_2_prt_america'::regclass,
		't_random_subpartition_1_prt_region3'::regclass,
		't_random_subpartition_1_prt_region3_2_prt_china'::regclass,
		't_random_subpartition_1_prt_region3_2_prt_america'::regclass);
--expand partition prepare
drop table t_random_subpartition;
create table t_random_subpartition
(
	r_regionkey integer not null,
	r_name char(25),
	r_comment varchar(152)
)
distributed randomly
partition by range (r_regionkey)
subpartition by list (r_name) subpartition template
(
	subpartition china values ('CHINA'),
	subpartition america values ('AMERICA')
)
(
	partition region1 start (0),
	partition region2 start (3),
	partition region3 start (5) end (8)
);

select localoid::regclass, policytype, numsegments, distkey, distclass
	from gp_distribution_policy where localoid in (
		't_random_subpartition'::regclass,
		't_random_subpartition_1_prt_region1'::regclass,
		't_random_subpartition_1_prt_region1_2_prt_china'::regclass,
		't_random_subpartition_1_prt_region1_2_prt_america'::regclass,
		't_random_subpartition_1_prt_region2'::regclass,
		't_random_subpartition_1_prt_region2_2_prt_china'::regclass,
		't_random_subpartition_1_prt_region2_2_prt_america'::regclass,
		't_random_subpartition_1_prt_region3'::regclass,
		't_random_subpartition_1_prt_region3_2_prt_china'::regclass,
		't_random_subpartition_1_prt_region3_2_prt_america'::regclass);

alter table t_random_subpartition expand partition prepare;
select localoid::regclass, policytype, numsegments, distkey, distclass
	from gp_distribution_policy where localoid in (
		't_random_subpartition'::regclass,
		't_random_subpartition_1_prt_region1'::regclass,
		't_random_subpartition_1_prt_region1_2_prt_china'::regclass,
		't_random_subpartition_1_prt_region1_2_prt_america'::regclass,
		't_random_subpartition_1_prt_region2'::regclass,
		't_random_subpartition_1_prt_region2_2_prt_china'::regclass,
		't_random_subpartition_1_prt_region2_2_prt_america'::regclass,
		't_random_subpartition_1_prt_region3'::regclass,
		't_random_subpartition_1_prt_region3_2_prt_china'::regclass,
		't_random_subpartition_1_prt_region3_2_prt_america'::regclass);

--can not set distributed for interior parts of partition table
alter table t_random_subpartition_1_prt_region3 set  distributed randomly;
alter table t_random_subpartition_1_prt_region3 set  distributed by (r_regionkey);
--the policy of leaf is the same as parent's
alter table t_random_subpartition_1_prt_region1_2_prt_china set distributed randomly;
--error, the policy of leaf is different from parent's
alter table t_random_subpartition_1_prt_region1_2_prt_china set distributed by (r_regionkey);

--alter root of partition table
alter table t_random_subpartition set distributed by (r_regionkey);
select localoid::regclass, policytype, numsegments, distkey, distclass
	from gp_distribution_policy where localoid in (
		't_random_subpartition'::regclass,
		't_random_subpartition_1_prt_region1'::regclass,
		't_random_subpartition_1_prt_region1_2_prt_china'::regclass,
		't_random_subpartition_1_prt_region1_2_prt_america'::regclass,
		't_random_subpartition_1_prt_region2'::regclass,
		't_random_subpartition_1_prt_region2_2_prt_china'::regclass,
		't_random_subpartition_1_prt_region2_2_prt_america'::regclass,
		't_random_subpartition_1_prt_region3'::regclass,
		't_random_subpartition_1_prt_region3_2_prt_china'::regclass,
		't_random_subpartition_1_prt_region3_2_prt_america'::regclass);
drop table t_random_subpartition;

create table t_root_partition_expand (a int, b int) partition by range (b) distributed by (a);
create table t1_partition_expand (a int, b int) distributed by (a); -- same column order as parent
create table t2_partition_expand (x int, b int, a int) distributed by (a); -- different column order from parent
alter table t2_partition_expand drop column x;

alter table t_root_partition_expand attach partition t1_partition_expand for values from (1) to (5);
alter table t_root_partition_expand attach partition t2_partition_expand for values from (5) to (10);

select localoid::regclass, policytype, numsegments, distkey, distclass
from gp_distribution_policy where localoid in (
  't_root_partition_expand'::regclass,
  't1_partition_expand'::regclass,
  't2_partition_expand'::regclass
);

alter table t_root_partition_expand expand partition prepare;
select localoid::regclass, policytype, numsegments, distkey, distclass
from gp_distribution_policy where localoid in ('t_root_partition_expand'::regclass, 't1_partition_expand'::regclass, 't2_partition_expand'::regclass);
alter table t1_partition_expand set distributed by (a);
alter table t2_partition_expand set distributed by (a);


--cleanup
select gp_debug_reset_create_table_default_numsegments();
drop extension gp_debug_numsegments;
