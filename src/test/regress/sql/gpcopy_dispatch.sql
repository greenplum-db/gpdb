
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
