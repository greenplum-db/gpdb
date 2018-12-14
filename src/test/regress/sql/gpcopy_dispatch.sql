
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
CREATE TABLE partdisttest (a int, dropped int, b int, c int)
  DISTRIBUTED BY (b)
  PARTITION BY RANGE (a) (START (0) END (100) EVERY (50));
ALTER TABLE partdisttest DROP COLUMN dropped;

ALTER TABLE partdisttest ADD PARTITION neg start (-10) end (0);

COPY partdisttest FROM stdin;
-1	2	3
\.
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
