-- Classic Syntax
-- Multi-level range-list partitioned table
DROP TABLE IF EXISTS partrl;
CREATE TABLE partrl (a int, b int, c int)
DISTRIBUTED BY (a)
PARTITION BY range(b)
SUBPARTITION BY list(c)
(
	PARTITION p1 START (10) END (20) EVERY (5)
	(
		SUBPARTITION sp1 VALUES (1, 2)
	),
	PARTITION p2 START (0) END (10)
	(
		SUBPARTITION sp2 VALUES (3, 4),
		SUBPARTITION sp1 VALUES (1, 2),
		DEFAULT SUBPARTITION others
	)
);

SELECT relid, level, gp_toolkit.pg_partition_rank(relid) as rank,
       pg_get_expr(relpartbound, c.oid) as relpartbound,
       gp_toolkit.pg_partition_range_from(c.oid) as range_from,
       gp_toolkit.pg_partition_range_to(c.oid) as range_to,
       gp_toolkit.pg_partition_lowest_child(c.oid),
       gp_toolkit.pg_partition_highest_child(c.oid)
    from pg_partition_tree('partrl') pt join pg_class c on pt.relid = c.oid;

SELECT * from gp_toolkit.gp_partitions where schemaname = 'public' and tablename = 'partrl' and partitionlevel = 1 order by partitionrank;
SELECT * from gp_toolkit.gp_partitions where schemaname = 'public' and tablename = 'partrl' and partitionlevel = 2;

-- Classic Syntax
-- Multi-level range-range partitioned table
CREATE TABLE partrr (a int, b int, c int)
DISTRIBUTED BY (a)
PARTITION BY range(b)
SUBPARTITION BY range(c)
(
	PARTITION p1 START (0) END (10)
	(
		DEFAULT SUBPARTITION other_c
	),
	PARTITION p2 START (10) END (20)
	(
		SUBPARTITION sp2 START (300) END (600),
		SUBPARTITION sp1 START (100) END (300),
		DEFAULT SUBPARTITION other_c
	)
);

SELECT relid, level, gp_toolkit.pg_partition_rank(relid) as rank,
       pg_get_expr(relpartbound, c.oid) as relpartbound,
       gp_toolkit.pg_partition_range_from(c.oid) as range_from,
       gp_toolkit.pg_partition_range_to(c.oid) as range_to,
       gp_toolkit.pg_partition_lowest_child(c.oid),
       gp_toolkit.pg_partition_highest_child(c.oid)
  from pg_partition_tree('partrr') pt join pg_class c on pt.relid = c.oid;
SELECT * from gp_toolkit.gp_partitions where schemaname = 'public' and tablename = 'partrr' and partitionlevel = 1 order by partitionrank;
SELECT * from gp_toolkit.gp_partitions where schemaname = 'public' and tablename = 'partrr' and partitionlevel = 2 order by partitionrank;

-- w/ minvalue and maxvalue
CREATE TABLE range_parted2 (
    a int
) PARTITION BY RANGE (a);
CREATE TABLE part1 PARTITION OF range_parted2 FOR VALUES FROM (1) TO (10);
CREATE TABLE part2 PARTITION OF range_parted2 FOR VALUES FROM (10) TO (20);
CREATE TABLE part3 PARTITION OF range_parted2 FOR VALUES FROM (20) TO (30);
CREATE TABLE part4 PARTITION OF range_parted2 FOR VALUES FROM (30) TO (40);
CREATE TABLE part5 PARTITION OF range_parted2 FOR VALUES FROM (40) TO (50);
CREATE TABLE part6 PARTITION OF range_parted2 FOR VALUES FROM (60) TO (maxvalue);
CREATE TABLE part7 PARTITION OF range_parted2 FOR VALUES FROM (minvalue) TO (1);

SELECT relid, level, gp_toolkit.pg_partition_rank(relid) as rank,
       pg_get_expr(relpartbound, c.oid) as relpartbound,
       gp_toolkit.pg_partition_range_from(c.oid) as range_from,
       gp_toolkit.pg_partition_range_to(c.oid) as range_to,
       gp_toolkit.pg_partition_lowest_child(c.oid),
       gp_toolkit.pg_partition_highest_child(c.oid)
from pg_partition_tree('range_parted2') pt join pg_class c on pt.relid = c.oid;
SELECT * from gp_toolkit.gp_partitions where schemaname = 'public' and tablename = 'range_parted2' order by partitionrank;

-- w/ expressions
CREATE TABLE range_parted3 (
    a int,
    b int
) PARTITION BY RANGE (abs(a - b));
CREATE TABLE range_parted3_part1 PARTITION OF range_parted3 FOR VALUES FROM (1) TO (10);
CREATE TABLE range_parted3_part2 PARTITION OF range_parted3 FOR VALUES FROM (10) TO (20);
CREATE TABLE range_parted3_part3 PARTITION OF range_parted3 FOR VALUES FROM (20) TO (30);
CREATE TABLE range_parted3_part4 PARTITION OF range_parted3 FOR VALUES FROM (30) TO (40);
CREATE TABLE range_parted3_part5 PARTITION OF range_parted3 FOR VALUES FROM (40) TO (50);
CREATE TABLE range_parted3_part6 PARTITION OF range_parted3 FOR VALUES FROM (60) TO (maxvalue);
CREATE TABLE range_parted3_part7 PARTITION OF range_parted3 FOR VALUES FROM (minvalue) TO (1);

SELECT relid, level, gp_toolkit.pg_partition_rank(relid) as rank,
       pg_get_expr(relpartbound, c.oid) as relpartbound,
       gp_toolkit.pg_partition_range_from(c.oid) as range_from,
       gp_toolkit.pg_partition_range_to(c.oid) as range_to,
       gp_toolkit.pg_partition_lowest_child(c.oid),
       gp_toolkit.pg_partition_highest_child(c.oid)
from pg_partition_tree('range_parted3') pt join pg_class c on pt.relid = c.oid;
SELECT * from gp_toolkit.gp_partitions where schemaname = 'public' and tablename = 'range_parted3' order by partitionrank;

-- date partitioned table
CREATE TABLE sales (trans_id int, date date, amount
                             decimal(9,2), region text)
    DISTRIBUTED BY (trans_id)
    PARTITION BY RANGE (date)
        (START (date '2023-01-01')
        END (date '2023-05-01') EVERY (INTERVAL '7 day'),
        DEFAULT PARTITION other_date);
SELECT relid, level, gp_toolkit.pg_partition_rank(relid) as rank,
       pg_get_expr(relpartbound, c.oid) as relpartbound,
       gp_toolkit.pg_partition_range_from(c.oid) as range_from,
       gp_toolkit.pg_partition_range_to(c.oid) as range_to,
       gp_toolkit.pg_partition_lowest_child(c.oid),
       gp_toolkit.pg_partition_highest_child(c.oid)
from pg_partition_tree('sales') pt join pg_class c on pt.relid = c.oid;
SELECT * from gp_toolkit.gp_partitions where schemaname = 'public' and tablename = 'sales' order by partitionrank;

-- Multi-column range partitioned table
-- multi-column keys
create table mc3p (a int, b int, c int) partition by range (a, abs(b), c);
create table mc3p_default partition of mc3p default;
create table mc3p0 partition of mc3p for values from (minvalue, minvalue, minvalue) to (1, 1, 1);
create table mc3p1 partition of mc3p for values from (1, 1, 1) to (10, 5, 10);
create table mc3p2 partition of mc3p for values from (10, 5, 10) to (10, 10, 10);
create table mc3p3 partition of mc3p for values from (10, 10, 10) to (10, 10, 20);
create table mc3p4 partition of mc3p for values from (10, 10, 20) to (10, maxvalue, maxvalue);
create table mc3p5 partition of mc3p for values from (11, 1, 1) to (20, 10, 10);
create table mc3p6 partition of mc3p for values from (20, 10, 10) to (20, 20, 20);
create table mc3p7 partition of mc3p for values from (20, 20, 20) to (maxvalue, maxvalue, maxvalue);

SELECT relid, level, gp_toolkit.pg_partition_rank(relid) as rank,
       pg_get_expr(relpartbound, c.oid) as relpartbound,
       gp_toolkit.pg_partition_range_from(c.oid) as range_from,
       gp_toolkit.pg_partition_range_to(c.oid) as range_to,
       gp_toolkit.pg_partition_lowest_child(c.oid),
       gp_toolkit.pg_partition_highest_child(c.oid)
from pg_partition_tree('mc3p') pt join pg_class c on pt.relid = c.oid;
SELECT * from gp_toolkit.gp_partitions where schemaname = 'public' and tablename = 'mc3p' order by partitionrank;
