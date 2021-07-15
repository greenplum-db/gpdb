
SET test_copy_qd_qe_split = on;

-- Distributed randomly. QD doesn't need any of the cols.
CREATE TABLE disttest (a int, b int, c int) DISTRIBUTED RANDOMLY;
COPY disttest FROM stdin;
1	2	3
\.
DROP TABLE disttest;

CREATE TABLE disttest (a int, b int, c int) DISTRIBUTED BY (b);
COPY disttest FROM stdin;
1	2	3
\.
DROP TABLE disttest;

CREATE TABLE disttest (a int, b int, c int) DISTRIBUTED BY (c);
COPY disttest FROM stdin;
1	2	3
\.
DROP TABLE disttest;

CREATE TABLE disttest (a int, b int, c int) DISTRIBUTED BY (c, a);
COPY disttest FROM stdin;
1	2	3
\.
DROP TABLE disttest;

-- With column list
CREATE TABLE disttest (a int, b int, c int) DISTRIBUTED BY (c, b);
COPY disttest (c, b, a) FROM stdin;
3	2	1
\.
DROP TABLE disttest;


--
-- Partitioned scenarios.
--

-- Distributed randomly, but QD needs the partitioning key.
CREATE TABLE partdisttest (a int, b int, c int) DISTRIBUTED RANDOMLY PARTITION BY RANGE (b) (START (1) END (10) EVERY (5));
COPY partdisttest FROM stdin;
1	2	3
\.
DROP TABLE partdisttest;

-- With a dropped column
CREATE TABLE partdisttest (a int, dropped int, b int, c int) DISTRIBUTED RANDOMLY PARTITION BY RANGE (b) (START (1) END (10) EVERY (5));
ALTER TABLE partdisttest DROP COLUMN dropped;
COPY partdisttest FROM stdin;
1	2	3
\.
DROP TABLE partdisttest;

-- Hash distributed, with a dropped column

-- We used to have a bug where QD would pick the wrong partition and/or the
-- wrong segment due to difference between the base table and a partition: the
-- partitioing attribute(s) for the root table is column 3, but it is column 1
-- in the leaf partition "neg". The QD would then mistakenly pick a partition
-- for the NULL value, and error out that no such a partition exists.

-- Note if the dropped columns are in a different position, a different (but
-- really similar) symptom will appear: the QD will pick another partition,
-- which potentially results in the wrong segment receiving the line / tuple.

CREATE TABLE partdisttest (dropped1 int, dropped2 int, a int, b int, c int)
  DISTRIBUTED BY (a)
  PARTITION BY RANGE (b) (START (0) END (100) EVERY (50));
ALTER TABLE partdisttest DROP COLUMN dropped1, DROP COLUMN dropped2;

ALTER TABLE partdisttest ADD PARTITION neg start (-10) end (0);

COPY partdisttest FROM stdin;
2	-1	3
\.

SELECT tableoid::regclass, * FROM partdisttest;
DROP TABLE partdisttest;


-- Similar case, where the columns are in different order
CREATE TABLE partdisttest (a int, b int, c int)
  DISTRIBUTED BY (a)
  PARTITION BY RANGE (b);

CREATE TABLE partdisttest_part0 (a int, b int, c int) DISTRIBUTED BY (a);
ALTER TABLE partdisttest ATTACH PARTITION partdisttest_part0 FOR VALUES FROM (0) TO (50);

CREATE TABLE partdisttest_part50 (c int, a int, b int) DISTRIBUTED BY (a);
ALTER TABLE partdisttest ATTACH PARTITION partdisttest_part50 FOR VALUES FROM (50) TO (100);

COPY partdisttest FROM stdin;
1	0	3
2	0	3
3	0	3
4	0	3
1	50	3
2	50	3
3	50	3
4	50	3
\.

SELECT gp_segment_id, tableoid::regclass, * from partdisttest;

DROP TABLE partdisttest;

-- Subpartitions
CREATE TABLE partdisttest (a int, dropped int, b int, c int, d int)
  DISTRIBUTED RANDOMLY
  PARTITION BY RANGE (b)
  SUBPARTITION BY RANGE (c)
  (
    PARTITION b_low start (1)
    (
      SUBPARTITION c_low start (1),
      SUBPARTITION c_hi start (5)
    ),
    PARTITION b_hi start (5)
    (
      SUBPARTITION c_low start (1),
      SUBPARTITION c_hi start (5)
    )
  );
ALTER TABLE partdisttest DROP COLUMN dropped;
COPY partdisttest FROM stdin;
1	2	3	4
\.

ALTER TABLE partdisttest ADD PARTITION b_negative start (-10) end (0) (subpartition c_negative start (-10) end (0));

COPY partdisttest FROM stdin;
100	-1	-1	1
\.

DROP TABLE partdisttest;

CREATE TABLE partdisttest (dropped bool, a smallint, b smallint)
  DISTRIBUTED BY (a)
  PARTITION BY RANGE(a)
  (PARTITION segundo START(5));
ALTER TABLE partdisttest DROP dropped;
ALTER TABLE partdisttest ADD PARTITION primero START(0) END(5);

-- We used to have bug, when a partition has a different distribution
-- column number than the base table, we'd lazily construct a tuple
-- table slot for that partition, but in a (mistakenly) short-lived
-- per query memory context. COPY FROM uses batch size of
-- MAX_BUFFERED_TUPLES tuples, and after that it resets the per query
-- context. As a result while processing second batch, segment used to
-- crash. This test exposed the bug and now validates the fix.
COPY (
    SELECT 2,1
    FROM generate_series(1, 10001)
    ) TO '/tmp/ten-thousand-and-one-lines.txt';
COPY partdisttest FROM '/tmp/ten-thousand-and-one-lines.txt';

SELECT tableoid::regclass, count(*) FROM partdisttest GROUP BY 1;
DROP TABLE partdisttest;

-- Log errors on QEs for partitions
CREATE TABLE partdisttest(id INT, t TIMESTAMP, d VARCHAR(4))
  DISTRIBUTED BY (id)
  PARTITION BY RANGE (t)
  (
    PARTITION p2020 START ('2020-01-01'::TIMESTAMP) END ('2021-01-01'::TIMESTAMP),
    DEFAULT PARTITION extra
  );

\set QUIET off

COPY partdisttest FROM STDIN LOG ERRORS SEGMENT REJECT LIMIT 2;
1	'2020-04-15'	abcde
1	'2020-04-15'	abc
\.

\set QUIET on

DROP TABLE partdisttest;


-- Check completion tags when COPY FROM
CREATE TABLE copydisttest(id INT, d VARCHAR(4))
  DISTRIBUTED BY (id);

CREATE FUNCTION copydisttest_ignore_even() RETURNS trigger
LANGUAGE plpgsql AS $$ 
BEGIN
  IF new.id % 2 = 0 THEN
    RETURN NULL;
  END IF;
  RETURN NEW;
END;
$$;

CREATE TRIGGER copydisttest_before_ins BEFORE INSERT ON copydisttest
FOR EACH ROW EXECUTE PROCEDURE copydisttest_ignore_even();

\set QUIET off

-- COPY on QD
-- expect 'COPY 2'
COPY copydisttest(d, id) FROM STDIN LOG ERRORS SEGMENT REJECT LIMIT 3;
abc	1
abc	1
\.
-- expect 'COPY 1'
COPY copydisttest(d, id) FROM STDIN LOG ERRORS SEGMENT REJECT LIMIT 3;
abcde	1
abc	1
\.
-- expect 'COPY 0'
COPY copydisttest(d, id) FROM STDIN LOG ERRORS SEGMENT REJECT LIMIT 3;
abcde	1
abcde	1
\.
-- expect 'COPY 0'
COPY copydisttest(d, id) FROM STDIN LOG ERRORS SEGMENT REJECT LIMIT 3;
abcde	2
abc	2
\.
-- expect 'COPY 0'
COPY copydisttest(d, id) FROM STDIN LOG ERRORS SEGMENT REJECT LIMIT 3;
abcde	2
abcde	2
\.

-- COPY on QE
-- expect 'COPY 2'
COPY copydisttest(id, d) FROM STDIN LOG ERRORS SEGMENT REJECT LIMIT 3;
1	abc
1	abc
\.
-- expect 'COPY 1'
COPY copydisttest(id, d) FROM STDIN LOG ERRORS SEGMENT REJECT LIMIT 3;
1	abcde
1	abc
\.
-- expect 'COPY 0'
COPY copydisttest(id, d) FROM STDIN LOG ERRORS SEGMENT REJECT LIMIT 3;
1	abcde
1	abcde
\.
-- expect 'COPY 0'
COPY copydisttest(id, d) FROM STDIN LOG ERRORS SEGMENT REJECT LIMIT 3;
2	abcde
2	abc
\.
-- expect 'COPY 0'
COPY copydisttest(id, d) FROM STDIN LOG ERRORS SEGMENT REJECT LIMIT 3;
2	abcde
2	abcde
\.

\set QUIET on

DROP TABLE copydisttest;
DROP FUNCTION copydisttest_ignore_even();
