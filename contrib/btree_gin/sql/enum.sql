set enable_seqscan=off;
CREATE TYPE rainbow AS ENUM ('r','o','y','g','b','i','v');

CREATE TABLE test_enum (
   h int,
   i rainbow
);

INSERT INTO test_enum VALUES (1, 'v'),(2, 'y'),(3, 'r'),(4, 'g'),(5, 'o'),(6, 'i'),(7, 'b');

CREATE INDEX idx_enum ON test_enum USING gin (i);

SELECT i FROM test_enum WHERE i<'g'::rainbow ORDER BY i;
SELECT i FROM test_enum WHERE i<='g'::rainbow ORDER BY i;
SELECT i FROM test_enum WHERE i='g'::rainbow ORDER BY i;
SELECT i FROM test_enum WHERE i>='g'::rainbow ORDER BY i;
SELECT i FROM test_enum WHERE i>'g'::rainbow ORDER BY i;

explain (costs off) SELECT i FROM test_enum WHERE i>='g'::rainbow ORDER BY i;

-- make sure we handle the non-evenly-numbered oid case for enums
create type e as enum ('0', '2', '3');
alter type e add value '1' after '0';

CREATE TABLE t (
   h int,
   i e
);
insert into t select j, (j % 4)::text::e from generate_series(0, 100000) as j;
create index on t using gin (i);
