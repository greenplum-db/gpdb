--@db_name heap_tables_db
-- heap table distributed by a single column
DROP TABLE IF EXISTS heap_table_1;
CREATE TABLE heap_table_1(
    a int,
    b int,
    c text,
    d timestamp
) distributed by (a);

INSERT INTO heap_table_1
SELECT i, i * 10, i::text, '2013-09-01 00:01:02'
FROM generate_series(1, 1000)i;


-- heap table distributed by multiple columns
DROP TABLE IF EXISTS heap_table_2;
CREATE TABLE heap_table_2(
    a int,
    b int,
    c text,
    d timestamp
) distributed by (a, b);

INSERT INTO heap_table_2
SELECT i, i * 10, i::text, '2013-09-01 00:01:02'
FROM generate_series(1, 1000)i;

DROP TABLE IF EXISTS heap_table_3;
CREATE TABLE heap_table_3(
    a int,
    b int,
    c text,
    d timestamp
) distributed by (a, b, c);

INSERT INTO heap_table_3
SELECT i, i * 10, i::text, '2013-09-01 00:01:02'
FROM generate_series(1, 1000)i;

DROP TABLE IF EXISTS heap_table_4;
CREATE TABLE heap_table_4(
    a int,
    b int,
    c text,
    d timestamp
) distributed by (a, d);

INSERT INTO heap_table_4
SELECT i, i * 10, i::text, date '2013-09-01 00:01:02' + i * '1 day'::interval
FROM generate_series(1, 1000)i;

-- heap table distributed randomly
DROP TABLE IF EXISTS heap_table_5;
CREATE TABLE heap_table_5(
    a int,
    b int,
    c text,
    d timestamp
) distributed randomly;

INSERT INTO heap_table_5
SELECT i, i * 10, i::text, date '2013-09-01 00:01:02' + i * '1 day'::interval
FROM generate_series(1, 1000)i;

-- drop column distribution key
DROP TABLE IF EXISTS heap_table_6;
CREATE TABLE heap_table_6(
    a int,
    b int,
    c text,
    d timestamp
) distributed by (a);

INSERT INTO heap_table_6
SELECT i, i * 10, i::text, date '2013-09-01 00:01:02' + i * '1 day'::interval
FROM generate_series(1, 1000)i;

ALTER table heap_table_6 drop column a;

INSERT INTO heap_table_6
SELECT i * 10, i::text, date '2013-09-01 00:01:02' + i * '1 day'::interval
FROM generate_series(1, 1000)i;

-- drop column some other column
DROP TABLE IF EXISTS heap_table_7;
CREATE TABLE heap_table_7(
    a int,
    b int,
    c text,
    d timestamp
) distributed by (a);

INSERT INTO heap_table_7
SELECT i, i * 10, i::text, date '2013-09-01 00:01:02' + i * '1 day'::interval
FROM generate_series(1, 1000)i;

ALTER table heap_table_7 drop column b;

INSERT INTO heap_table_7
SELECT i, i::text, date '2013-09-01 00:01:02' + i * '1 day'::interval
FROM generate_series(1, 1000)i;


-- PARTITION TABLES
DROP TABLE IF EXISTS heap_part_table_1;
CREATE TABLE heap_part_table_1(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a)
PARTITION BY RANGE(b) (START (0) END (100) INCLUSIVE EVERY(20));

INSERT INTO heap_part_table_1
SELECT i, (i * 10) % 100, i::text, '2013-09-01 03:04:05'
FROM generate_series(1, 1000)i;

DROP TABLE IF EXISTS heap_part_table_2;
CREATE TABLE heap_part_table_2(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a, b)
PARTITION BY RANGE(b) (START (0) END (100) INCLUSIVE EVERY(20));

INSERT INTO heap_part_table_2
SELECT i, (i * 10) % 100, i::text, '2013-09-01 03:04:05'
FROM generate_series(1, 1000)i;


DROP TABLE IF EXISTS heap_part_table_3;
CREATE TABLE heap_part_table_3(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a, b, c)
PARTITION BY RANGE(b) (START (0) END (100) INCLUSIVE EVERY(20));

INSERT INTO heap_part_table_2
SELECT i, (i * 10) % 100, i::text, '2013-09-01 03:04:05'
FROM generate_series(1, 1000)i;

DROP TABLE IF EXISTS heap_part_table_4;
CREATE TABLE heap_part_table_4(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a, d)
PARTITION BY RANGE(b) (START (0) END (100) INCLUSIVE EVERY(20));

INSERT INTO heap_part_table_4
SELECT i, (i * 10) % 100, i::text, date '2013-09-01 03:04:05' + i * '1 day'::interval
FROM generate_series(1, 1000)i;

-- partition table distributed randomly
DROP TABLE IF EXISTS heap_part_table_5;
CREATE TABLE heap_part_table_5(
    a text,
    b int,
    c text,
    d timestamp
)
distributed randomly
PARTITION BY RANGE(b) (START (0) END (100) INCLUSIVE EVERY(20));

INSERT INTO heap_part_table_5
SELECT i, (i * 10) % 100, i::text, date '2013-09-01 03:04:05' + i * '1 day'::interval
FROM generate_series(1, 1000)i;

-- multi-level partitions
DROP TABLE IF EXISTS heap_part_table_6;
CREATE TABLE heap_part_table_6(
    a int,
    b int,
    c text,
    d timestamp
)
distributed randomly
PARTITION BY RANGE(b) 
subpartition by range(a) subpartition template (start (0) end (100) every (50), default subpartition other)
(
START (0) END (100) INCLUSIVE EVERY(20), default partition other
);

INSERT INTO heap_part_table_6
SELECT i, (i * 10), i::text, date '2013-09-01 03:04:05' + i * '1 day'::interval
FROM generate_series(1, 1000)i;


-- mixed distribution policies
DROP TABLE IF EXISTS heap_part_table_7;
CREATE TABLE heap_part_table_7(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a)
PARTITION BY RANGE(b) (
PARTITION p1 START (0) END (20) INCLUSIVE,
PARTITION p2 START (21) END (40) INCLUSIVE,
PARTITION p3 START (41) END (60) INCLUSIVE,
PARTITION p4 START (61) END (80) INCLUSIVE,
PARTITION p5 START (81) END (100) INCLUSIVE);

INSERT INTO heap_part_table_7
SELECT i, (i * 10) % 100, i::text, date '2013-09-01 03:04:05' + i * '1 day'::interval
FROM generate_series(1, 1000)i;

ALTER TABLE only heap_part_table_7_1_prt_p1 set distributed randomly;
ALTER TABLE only heap_part_table_7_1_prt_p2 set distributed randomly;
ALTER TABLE only heap_part_table_7_1_prt_p3 set distributed randomly;

INSERT INTO heap_part_table_7
SELECT i, (i * 10) % 100, i::text, date '2013-09-01 03:04:05' + i * '1 day'::interval
FROM generate_series(1, 1000)i;


-- drop distribution key column in partition table
DROP TABLE IF EXISTS heap_part_table_8;
CREATE TABLE heap_part_table_8(
    a int,
    b int,
    c text,
    d timestamp
)
distributed by (a)
PARTITION BY RANGE(b) 
(
START (0) END (100) INCLUSIVE EVERY(20), default partition other
);

INSERT INTO heap_part_table_8
SELECT i, (i * 10), i::text, date '2013-09-01 03:04:05' + i * '1 day'::interval
FROM generate_series(1, 1000)i;

ALTER TABLE heap_part_table_8 drop column a;

INSERT INTO heap_part_table_8
SELECT (i * 10), i::text, date '2013-09-01 03:04:05' + i * '1 day'::interval
FROM generate_series(1, 1000)i;

-- drop column in partition table
DROP TABLE IF EXISTS heap_part_table_9;
CREATE TABLE heap_part_table_9(
    a int,
    b int,
    c text,
    d timestamp
)
distributed by (a)
PARTITION BY RANGE(b) 
(
START (0) END (100) INCLUSIVE EVERY(20), default partition other
);

INSERT INTO heap_part_table_9
SELECT i, (i * 10), i::text, date '2013-09-01 03:04:05' + i * '1 day'::interval
FROM generate_series(1, 1000)i;

ALTER TABLE heap_part_table_9 drop column c;

INSERT INTO heap_part_table_9
SELECT i, (i * 10), date '2013-09-01 03:04:05' + i * '1 day'::interval
FROM generate_series(1, 1000)i;



DROP TABLE IF EXISTS users_rank_third_to_last;
CREATE TABLE users_rank_third_to_last (
    id    INT4,
    name  TEXT,
    age   INT4
);
INSERT INTO users_rank_third_to_last select i, 'name'||i, i%50+1 from generate_series(1,1000) i;


DROP TABLE IF EXISTS users_rank_second_to_last;
CREATE TABLE users_rank_second_to_last (
    id    SERIAL,
    name  TEXT,
    age   INT4
) DISTRIBUTED BY (age);
INSERT INTO users_rank_second_to_last select i, 'name'||i, i%50+1 from generate_series(1,1000) i;

DROP TABLE IF EXISTS users_rank_very_last;
CREATE TABLE users_rank_very_last (
    id    SERIAL,
    name  TEXT,
    age   INT4
) DISTRIBUTED RANDOMLY;
INSERT INTO users_rank_very_last select i, 'name'||i, i%50+1 from generate_series(1,1000) i;

DROP TABLE IF EXISTS users_with_primary_key;
CREATE TABLE users_with_primary_key (
    id    SERIAL PRIMARY KEY,
    name  TEXT,
    age   INT4
);
INSERT INTO users_with_primary_key select i, 'name'||i, i%50+1 from generate_series(1,1000) i;


DROP TABLE IF EXISTS users_with_index;
CREATE TABLE users_with_index (
    id    SERIAL,
    name  TEXT,
   age   INT4
);
INSERT INTO users_with_index select i, 'name'||i, i%50+1 from generate_series(1,1000) i;
CREATE UNIQUE INDEX users_idx ON users_with_index (id);

--unalterable table test ref
drop type if exists custom_type cascade;
drop table if exists unalterable_table cascade;
create type custom_type;
create or replace function custom_typein (cstring) returns custom_type as 'textin' language internal;
create or replace function custom_typeout (custom_type) returns cstring as 'textout' language internal;
create type custom_type (input = custom_typein, output = custom_typeout, internallength = 42);
create table unalterable_table (i int, j custom_type);
alter table unalterable_table drop column j;
alter table unalterable_table set with(reorganize = true) distributed randomly;

-- MPP-24478: External table with error table configured
DROP EXTERNAL TABLE IF EXISTS exttab_with_error_table;

CREATE EXTERNAL WEB TABLE exttab_with_error_table (i int, j text) 
EXECUTE 'python $GPHOME/bin/datagen.py 100 2' on all format 'text' (delimiter '|')
LOG ERRORS SEGMENT REJECT LIMIT 1000;

-- Might return different number of rows in different configurations
SELECT COUNT(*) > 0 FROM exttab_with_error_table;
