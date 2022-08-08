-- The failure case with AOCS partition
-- Create partitioned table with one range and default partitions
create table p_ao(a int) with (appendonly=true)
PARTITION BY range(a) (start(1) end(2) every(1), default partition extra);
insert into p_ao_1_prt_2 values(1);
analyze p_ao;

-- create table for exchange with one value suitable to range partition with one row
create table t_ao_exchanged(a int) with (appendonly=true);
insert into t_ao_exchanged values (1);
analyze t_ao_exchanged;
-- exchange default partition with `t_exchanged` table
set gp_enable_exchange_default_partition to on;
alter table p_ao exchange default partition with table t_ao_exchanged without validation;
select tableoid::regclass, ctid, a from p_ao;

-- remove row from range partition - mark it as deleted
delete from p_ao_1_prt_2;

-- check whether partitioned table has a single row in default partition
-- whose partitioning key value corresponds to neighbor range partition
-- and tupleid is the same as previously deleted row had
select tableoid::regclass, ctid, a from p_ao;

-- perform deletion from default partition
-- before fix in case of ORCA it generated SIGSEGV
explain (costs off, verbose)
delete from p_ao_1_prt_extra where a = 1;
delete from p_ao_1_prt_extra where a = 1;
