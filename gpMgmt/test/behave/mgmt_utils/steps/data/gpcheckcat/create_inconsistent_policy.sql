set allow_system_table_mods=true;
CREATE TABLE sales (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id)
PARTITION BY RANGE (date)
SUBPARTITION BY LIST (region)
SUBPARTITION TEMPLATE
( SUBPARTITION usa VALUES ('usa'), 
  SUBPARTITION asia VALUES ('asia'), 
  SUBPARTITION europe VALUES ('europe'), 
  DEFAULT SUBPARTITION other_regions)
  (START (date '2011-01-01') INCLUSIVE
   END (date '2012-01-01') EXCLUSIVE
   EVERY (INTERVAL '1 month'), 
   DEFAULT PARTITION outlying_dates );
update gp_distribution_policy set distkey = '2' where localoid in (select localoid from gp_distribution_policy order by random() limit 3);

-- Corrupt a partition's distribution opclass
CREATE TABLE test (a int, b int)
    DISTRIBUTED BY (a, b)
    PARTITION BY RANGE(a) (START(1) END(2) EVERY(1));
UPDATE gp_distribution_policy SET distkey=1::int2vector
    WHERE localoid='test_1_prt_1'::regclass;
