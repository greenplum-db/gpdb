-- Test GPDB specific partitioning parser for subpartition template

-- Subpartition by RANGE
CREATE TABLE subpart_range_templ (id int, year date, letter char(1))
    DISTRIBUTED BY (id, letter, year)
    PARTITION BY list (letter)
        subpartition by range (year)
            SUBPARTITION TEMPLATE (
            subpartition r1 START (date '2001-01-01') END (date '2003-01-01'),
            subpartition r2 START (date '2003-01-01') END (date '2005-01-01') EVERY (interval '1 year'),
            DEFAULT SUBPARTITION other_year )
        (
        PARTITION a VALUES ('A'),
        PARTITION b VALUES ('B'),
        DEFAULT PARTITION other_letter
        );

-- Subpartition template should be stored in catalog
SELECT level, pg_get_expr(template, relid) from gp_partition_template t WHERE t.relid = 'subpart_range_templ'::regclass;
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('subpart_range_templ') As t, pg_class As c WHERE relid = oid ORDER BY 1,5;

-- ADD PARTITION should create subpartitions
ALTER TABLE subpart_range_templ ADD PARTITION c VALUES ('C');
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('subpart_range_templ') As t, pg_class As c WHERE relid = oid ORDER BY 1,5;
-- ADD PARTITION with subpartition definition spec should fail
ALTER TABLE subpart_range_templ ADD PARTITION d VALUES ('D') (SUBPARTITION r3 START (date '2003-01-01') END (date '2005-01-01'));
-- Remove subpartition template
ALTER TABLE subpart_range_templ SET SUBPARTITION TEMPLATE();
SELECT level, pg_get_expr(template, relid) from gp_partition_template t WHERE t.relid = 'subpart_range_templ'::regclass;
-- ADD PARTITION with subpartition definition spec now should work
ALTER TABLE subpart_range_templ ADD PARTITION d VALUES ('D') (SUBPARTITION r3 START (date '2003-01-01') END (date '2005-01-01'));
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('subpart_range_templ') As t, pg_class As c WHERE relid = oid ORDER BY 1,5;
-- Set a new invalid subpartition template should fail
ALTER TABLE subpart_range_templ SET SUBPARTITION TEMPLATE
(
    subpartition r4 START (date '2020-01-01'), END (date '2021-01-01'),
    subpartition r5 START (date '2021-01-01'), END (date '2023-01-01'),
    DEFAULT SUBPARTITION yet_another_year
);
-- Set a new valid subpartition template
ALTER TABLE subpart_range_templ SET SUBPARTITION TEMPLATE
(
    subpartition r4 START (date '2020-01-01') EXCLUSIVE END (date '2021-01-01') INCLUSIVE,
    subpartition r5 START (date '2021-01-01') EXCLUSIVE END (date '2023-01-01') INCLUSIVE EVERY (interval '1 year'),
    DEFAULT SUBPARTITION yet_another_year
);
SELECT level, pg_get_expr(template, relid) from gp_partition_template t WHERE t.relid = 'subpart_range_templ'::regclass;
-- ADD PARTITION should create subpartitions according to the new template
ALTER TABLE subpart_range_templ ADD PARTITION e VALUES ('E');
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('subpart_range_templ') As t, pg_class As c WHERE relid = oid ORDER BY 1,5;

-- Set a new subpartition template w/ WITH clause
ALTER TABLE subpart_range_templ SET SUBPARTITION TEMPLATE
    (
    subpartition r4 END (date '2020-01-01') WITH (appendonly=true, compresslevel=5),
    DEFAULT SUBPARTITION yet_another_year
    );
SELECT level, pg_get_expr(template, relid) from gp_partition_template t WHERE t.relid = 'subpart_range_templ'::regclass;
ALTER TABLE subpart_range_templ ADD PARTITION f VALUES ('F');
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('subpart_range_templ') As t, pg_class As c WHERE relid = oid ORDER BY 1,5;

ALTER TABLE subpart_range_templ SET SUBPARTITION TEMPLATE
    (
    subpartition r4 START (date '2020-01-01') EXCLUSIVE WITH (appendonly=true, orientation=column, compresslevel=5),
    DEFAULT SUBPARTITION yet_another_year
    );
SELECT level, pg_get_expr(template, relid) from gp_partition_template t WHERE t.relid = 'subpart_range_templ'::regclass;
ALTER TABLE subpart_range_templ ADD PARTITION g VALUES ('G');
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('subpart_range_templ') As t, pg_class As c WHERE relid = oid ORDER BY 1,5;

-- Multi-level RANGE subpartitioning using templates
CREATE TABLE subpart_range_templ_multilevel (id int, year int, month int, day int,
                    region text)
    DISTRIBUTED BY (id)
    PARTITION BY RANGE (year)
        SUBPARTITION BY RANGE (month)
            SUBPARTITION TEMPLATE (
            START (11) END (12) EVERY (1),
            DEFAULT SUBPARTITION other_months )
        SUBPARTITION BY RANGE (day)
            SUBPARTITION TEMPLATE (
            START (7) END (15) EVERY (7),
            DEFAULT SUBPARTITION other_days )
        ( START (2022) END (2023) EVERY (1));

SELECT level, pg_get_expr(template, relid) from gp_partition_template t WHERE t.relid = 'subpart_range_templ_multilevel'::regclass;
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('subpart_range_templ_multilevel') As t, pg_class As c WHERE relid = oid ORDER BY 1,5;
-- ADD PARTITION should create subpartitions according to the templates
ALTER TABLE subpart_range_templ_multilevel ADD PARTITION new_part START (2023) END (2024);
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('subpart_range_templ_multilevel') As t, pg_class As c WHERE relid = oid AND t.relid::regclass::text like '%new_part%' ORDER BY 1,5;
-- Remove level 1 subpartition template
ALTER TABLE subpart_range_templ_multilevel SET SUBPARTITION TEMPLATE();
SELECT level, pg_get_expr(template, relid) from gp_partition_template t WHERE t.relid = 'subpart_range_templ_multilevel'::regclass;
-- ADD PARTITION should create subpartitions according to the level 1 partition definition spec and level 2 subpartition templates
ALTER TABLE subpart_range_templ_multilevel Add partition oct START (10) END (11) (SUBPARTITION new_days START (15) END (30));
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('subpart_range_templ_multilevel') As t, pg_class As c WHERE relid = oid AND t.relid::regclass::text LIKE '%oct%' ORDER BY 1,5;
-- Remove level 2 subpartition template, this works but no syntax will be available to add new partitions at any level
ALTER TABLE subpart_range_templ_multilevel ALTER PARTITION new_part SET SUBPARTITION TEMPLATE();
SELECT level, pg_get_expr(template, relid) from gp_partition_template t WHERE t.relid = 'subpart_range_templ_multilevel'::regclass;
-- Can't ADD PARTITION because subpartition template at the last level is missing
ALTER TABLE subpart_range_templ_multilevel Add partition oct START (10) END (11) (SUBPARTITION new_days START (15) END (30));

-- Multi-level subpartition using template at the last level
CREATE TABLE subpart_range_mixedtempl (a int, dropped int, b int, c int, d int)
    DISTRIBUTED RANDOMLY
    PARTITION BY RANGE (b)
        SUBPARTITION BY RANGE (c)
        SUBPARTITION BY RANGE (d)
        SUBPARTITION TEMPLATE (
              SUBPARTITION c_low start (1) end (5),
              SUBPARTITION c_hi start (5) end (10),
              DEFAULT SUBPARTITION def_d
            )
        (
        PARTITION b_low start (0) end (3)
            (
                SUBPARTITION c_low start (11) end (22) every (10),
                DEFAULT SUBPARTITION def_c
            ),
        PARTITION b_mid start (3) end (6)
            (
                SUBPARTITION c_mid start (22) end (32) every (10)
            )
        );
-- Subpartition template should be stored in catalog
SELECT level, pg_get_expr(template, relid) from gp_partition_template t WHERE t.relid = 'subpart_range_mixedtempl'::regclass;
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('subpart_range_mixedtempl') As t, pg_class As c WHERE relid = oid ORDER BY 1,5;
-- ADD PARTITION without subpartition definition spec for level 2 should fail
ALTER TABLE subpart_range_mixedtempl ADD PARTITION b_high START (6) END (9);
-- ADD PARTITION with subpartition definition spec for level 2 should fail if EVERY is specified in level 1 partition definition spec
ALTER TABLE subpart_range_mixedtempl ADD PARTITION b_high START (6) END (9) every (3) (subpartition c_high start(32) end (50));
-- ADD PARTITION with subpartition definition spec for level 2 should fail if there is more than one subpartition at level 2.
ALTER TABLE subpart_range_mixedtempl ADD PARTITION b_high START (6) END (9) (subpartition c_high start(32) end (50) every (15));
-- ADD PARTITION with subpartition definition spec for level 2 should create level 3 subpartitions according to the template
ALTER TABLE subpart_range_mixedtempl ADD PARTITION b_high START (6) END (9) (subpartition c_high start(32) end (50) every (50));
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('subpart_range_mixedtempl') As t, pg_class As c WHERE relid = oid AND t.relid::regclass::text LIKE '%b_high%' ORDER BY 1,5;

-- Subpartitioned by LIST
CREATE TABLE subpart_list_templ (trans_id int, date date, amount
                             decimal(9,2), region text)
    DISTRIBUTED BY (trans_id)
    PARTITION BY RANGE (date)
        SUBPARTITION BY LIST (region)
            SUBPARTITION TEMPLATE
            ( SUBPARTITION usa VALUES ('usa'),
            SUBPARTITION asia VALUES ('asia'),
            SUBPARTITION europe VALUES ('europe'),
            DEFAULT SUBPARTITION other_regions)
        (START (date '2020-01-01') INCLUSIVE
        END (date '2022-01-01') EXCLUSIVE
        EVERY (INTERVAL '1 year'),
        DEFAULT PARTITION outlying_dates );

SELECT level, pg_get_expr(template, relid) from gp_partition_template t WHERE t.relid = 'subpart_list_templ'::regclass;
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('subpart_list_templ') As t, pg_class As c WHERE relid = oid ORDER BY 1,5;
ALTER TABLE subpart_list_templ ADD PARTITION year_2023 START (date '2022-01-01') EXCLUSIVE END (date '2023-01-01') INCLUSIVE;
-- ADD PARTITION should create subpartitions according to the template
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('subpart_list_templ') As t, pg_class As c WHERE relid = oid AND t.relid::regclass::text LIKE '%year_2023%' ORDER BY 1,5;

-- Subpartitioning without any template should work as usual
CREATE TABLE notemplate (a int, dropped int, b int, c int, d int)
    DISTRIBUTED RANDOMLY
    PARTITION BY RANGE (b)
        SUBPARTITION BY RANGE (c)
        (
        PARTITION b_low start (0) end (3)
            (
              SUBPARTITION c_low start (1) end (5),
              SUBPARTITION c_hi start (5) end (10),
              DEFAULT SUBPARTITION def
            ),
        PARTITION b_mid start (3) end (6)
            (
              SUBPARTITION c_low start (1) end (5),
              SUBPARTITION c_hi start (5) end (10)
            )
        );

SELECT level, pg_get_expr(template, relid) from gp_partition_template t WHERE t.relid = 'notemplate'::regclass;
-- This should fail due to missing subpartition definition spec
ALTER TABLE notemplate ADD partition b_hi START (6) END (9);
-- This should work
ALTER TABLE notemplate ADD partition b_hi START (6) END (9) (subpartition c_low START (1) END (7));
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('notemplate') As t, pg_class As c WHERE relid = oid ORDER BY 1,5;

-- Subpartition templates with encoding clauses

prepare encoding_check as
select attrelid::regclass as relname,
        attnum, filenum,attoptions from pg_class c, pg_attribute_encoding e
where c.relname like 'subpart_templ_encoding%' and c.oid=e.attrelid
order by relname, attnum;

-- Range partition with enclause
DROP TABLE IF EXISTS subpart_templ_encoding;
create table subpart_templ_encoding (i int, j int, k int, l int)
    with (appendonly = true, orientation=column)
    partition by range(j)
    subpartition by range (k)
    subpartition template(
        subpartition sp1 start (100) end (200) every (50),
        column i encoding(compresstype=zlib),
        column j encoding(compresstype=RLE_TYPE),
        column k encoding(compresstype=zlib),
        column l encoding(compresstype=zlib)
    )
(   partition p1 start(1) end(10),
    partition p2 start(10) end(20)
);
SELECT level, pg_get_expr(template, relid) from gp_partition_template t WHERE t.relid = 'subpart_templ_encoding'::regclass;
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('subpart_templ_encoding') AS t, pg_class AS c WHERE relid = oid ORDER BY 1,5;
EXECUTE encoding_check;
-- FIXME: the new partitions should have the encodings specified in the template
ALTER TABLE subpart_templ_encoding ADD PARTITION p3 START (30) END (40);
EXECUTE encoding_check;

-- List partition with enclause
DROP TABLE IF EXISTS subpart_templ_encoding;
create table subpart_templ_encoding (i int, j int, k int, l int)
    with (appendonly = true, orientation=column)
    partition by range(j)
    subpartition by list (k)
    subpartition template(
        subpartition sp1 values(1, 2, 3, 4, 5) ,
        column i encoding(compresstype=zlib),
        column j encoding(compresstype=RLE_TYPE),
        column k encoding(compresstype=zlib),
        column l encoding(compresstype=zlib)
    )
(   partition p1 start(1) end(10),
    partition p2 start(10) end(20)
);
SELECT level, pg_get_expr(template, relid) from gp_partition_template t WHERE t.relid = 'subpart_templ_encoding'::regclass;
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('subpart_templ_encoding') AS t, pg_class AS c WHERE relid = oid ORDER BY 1,5;
EXECUTE encoding_check;
-- FIXME: the new partitions should have the encodings specified in the template
ALTER TABLE subpart_templ_encoding ADD PARTITION p3 START (30) END (40);
EXECUTE encoding_check;

-- Partition specific ENCODING clause is not supported in SUBPARTITION TEMPLATE
create table template_partelem_enc (i int, j int, k int, l int)
    with (appendonly = true, orientation=column)
    partition by range(j)
    subpartition by range (k)
        subpartition template(
        subpartition sp1 start (100) end (200) every (50) column i encoding(compresstype=RLE_TYPE),
        column i encoding(compresstype=zlib),
        column j encoding(compresstype=RLE_TYPE),
        column k encoding(compresstype=zlib),
        column l encoding(compresstype=zlib)
    )
(   partition p1 start(1) end(10),
    partition p2 start(10) end(20)
);

-- More tests on SET SUBPARTITION TEMPLATE

CREATE TABLE set_templ_test (id int, year date, letter char(1))
    DISTRIBUTED BY (id, letter, year)
    PARTITION BY list (letter);
-- Can't SET SUBPARTITION TEMPLATE when level 1 partition is not present
ALTER TABLE set_templ_test SET SUBPARTITION TEMPLATE(
    subpartition r1 START (date '2001-01-01') END (date '2003-01-01'),
    subpartition r2 START (date '2003-01-01') END (date '2005-01-01') EVERY (interval '1 year')
    );
-- Add a level 1 partition but the partition itself is not a partitioned table
CREATE TABLE set_templ_test_c PARTITION OF set_templ_test FOR VALUES IN ('C');
-- Can't SET SUBPARTITION TEMPLATE when level 1 partition is not partitioned
ALTER TABLE set_templ_test SET SUBPARTITION TEMPLATE(
    subpartition r1 START (date '2001-01-01') END (date '2003-01-01'),
    subpartition r2 START (date '2003-01-01') END (date '2005-01-01') EVERY (interval '1 year')
    );
-- Add another level 1 partition that IS a partitioned table
CREATE TABLE set_templ_test_d PARTITION OF set_templ_test FOR VALUES IN ('D')
    PARTITION BY range (year);
-- Can't SET SUBPARTITION TEMPLATE when the FIRST level 1 child partition is not partitioned
ALTER TABLE set_templ_test SET SUBPARTITION TEMPLATE(
    subpartition r1 START (date '2001-01-01') END (date '2003-01-01'),
    subpartition r2 START (date '2003-01-01') END (date '2005-01-01') EVERY (interval '1 year')
    );
-- Add another level 1 partition that IS a partitioned table (but has zero child)
-- and will be treated as the FIRST child on this level
CREATE TABLE set_templ_test_b PARTITION OF set_templ_test FOR VALUES IN ('B')
    PARTITION BY range (year);
-- Can't SET SUBPARTITION TEMPLATE when sibling level 2 partition is not present
ALTER TABLE set_templ_test SET SUBPARTITION TEMPLATE(
    subpartition r1 START (date '2001-01-01') END (date '2003-01-01'),
    subpartition r2 START (date '2003-01-01') END (date '2005-01-01') EVERY (interval '1 year')
    );
-- Add a level 2 partition as the child of the first level 1 partition
CREATE TABLE set_templ_test_b_sp1 PARTITION OF set_templ_test_b FOR VALUES FROM (date '2021-01-01') TO (date '2022-01-01');
-- SET SUBPARTITION TEMPLATE should work
ALTER TABLE set_templ_test SET SUBPARTITION TEMPLATE(
    subpartition r1 START (date '2001-01-01') END (date '2003-01-01'),
    subpartition r2 START (date '2003-01-01') END (date '2005-01-01') EVERY (interval '1 year')
    );
SELECT level, pg_get_expr(template, relid) from gp_partition_template t WHERE t.relid = 'set_templ_test'::regclass;
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('set_templ_test') AS t, pg_class AS c WHERE relid = oid ORDER BY 1,5;
-- ADD PARTITION should created subpartitions according to the template
ALTER TABLE set_templ_test ADD PARTITION e VALUES ('E');
SELECT t.*, pg_get_expr(relpartbound, oid) FROM pg_partition_tree('set_templ_test') AS t, pg_class AS c WHERE relid = oid ORDER BY 1,5;
-- Add a level 1 partition that is partitioned BY LIST rather than by range,
-- and it will be treated as the FIRST child on this level
CREATE TABLE set_templ_test_a PARTITION OF set_templ_test FOR VALUES IN ('A')
    PARTITION BY list (year);
CREATE TABLE set_templ_test_a_sp1 PARTITION OF set_templ_test_a FOR VALUES IN (date '2022-01-01');
-- Can't ADD PARTITION because the RANGE boundary spec in the stored partition template
-- doesn't match the LIST partition strategy of the FIRST level 1 partition
ALTER TABLE set_templ_test ADD PARTITION f VALUES ('F');
-- Drop the FIRST level 1 partition
DROP TABLE set_templ_test_a;
-- Add a level 1 partition that is partitioned by multi-column range partition key,
-- and it will be treated as the FIRST child on this level
CREATE TABLE set_templ_test_a PARTITION OF set_templ_test FOR VALUES IN ('A')
    PARTITION BY range (year, letter);
CREATE TABLE set_templ_test_a_sp2 PARTITION OF set_templ_test_a
    FOR VALUES FROM (date '2022-01-01', 'A') TO (date '2022-01-01', 'K');
-- Can't ADD PARTITION because the boundary spec in the stored partition template
-- doesn't match the first level 1 partition's partition key
ALTER TABLE set_templ_test ADD PARTITION f VALUES ('F');
-- Drop the FIRST level 1 partition
DROP TABLE set_templ_test_a;
-- Add a level 1 partition who's partition key is an expression
CREATE TABLE set_templ_test_a PARTITION OF set_templ_test FOR VALUES IN ('A')
    PARTITION BY range (abs(id+1));
CREATE TABLE set_templ_test_a_sp3 PARTITION OF set_templ_test_a
    FOR VALUES FROM (1) TO (10);
-- Can't ADD PARTITION because level 1 first partition's partition key has expression
ALTER TABLE set_templ_test ADD PARTITION f VALUES ('F');
-- Reset SUBPARTITION TEMPLATE
ALTER TABLE set_templ_test SET SUBPARTITION TEMPLATE();
ALTER TABLE set_templ_test SET SUBPARTITION TEMPLATE(
    subpartition r1 START (date '2001-01-01') END (date '2003-01-01'),
    subpartition r2 START (date '2003-01-01') END (date '2005-01-01') EVERY (interval '1 year')
    );
