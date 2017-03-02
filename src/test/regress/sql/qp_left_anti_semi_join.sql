-- Tests for Left Anti Semi Joins
create schema lasj;
set search_path='lasj';

create table foo (a int, b int);
create table bar (x int, y int);

insert into foo values (1, 2);
insert into foo values (12, 20);
insert into foo values (NULL, 2);
insert into foo values (15, 2);
insert into foo values (NULL, NULL);
insert into foo values (1, 12);
insert into foo values (1, 102);

insert into bar select i/10, i from generate_series(1, 100)i;
insert into bar values (NULL, 101);
insert into bar values (NULL, 102);
insert into bar values (NULL, NULL);


-- We will run the same queries with hash joins enabled, and disabled. First,
-- disabled.
set enable_hashjoin=off;
SELECT disable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- empty outer, non-empty inner
SELECT * FROM foo WHERE b = -1 AND a = ALL (SELECT x FROM bar WHERE y <= 100);

-- outer with nulls, non-empty inner
SELECT * FROM foo WHERE b = 2 AND a = ALL (SELECT x FROM bar WHERE y >=10 AND y < 20);

-- outer with nulls, empty inner
SELECT * FROM foo WHERE b = 2 AND a = ALL (SELECT x FROM bar WHERE y = -1) order by 1, 2;

-- outer with nulls, inner with nulls
SELECT * FROM foo WHERE a = ALL (SELECT x FROM bar WHERE x = 1 OR x IS NULL);

-- empty outer, non-empty inner
SELECT * FROM (SELECT * FROM foo WHERE b = -1) foo2 FULL OUTER JOIN bar ON (a = x);

-- non-empty outer, empty inner
SELECT * FROM foo FULL OUTER JOIN (SELECT * FROM bar WHERE y = -1) bar2 ON (a = x);

-- non-empty outer, non-empty inner
SELECT * FROM (SELECT * FROM foo WHERE b = 2) foo2 FULL OUTER JOIN (SELECT * FROM bar WHERE y BETWEEN 16 AND 22 OR x IS NULL) bar2 ON (a = x);

-- empty outer, non-empty inner
SELECT * FROM foo WHERE b = -1 AND (a, b) NOT IN (SELECT x, y FROM bar WHERE y <= 100);

-- outer with nulls, non-empty inner
SELECT * FROM foo WHERE (a, b) NOT IN (SELECT x, y FROM bar WHERE y <= 100);

-- outer with nulls, empty inner
SELECT * FROM foo WHERE (a, b) NOT IN (SELECT x, y FROM bar WHERE y = -1);

-- outer with nulls, inner with partial nulls
SELECT * FROM foo WHERE (a, b) NOT IN (SELECT x, y FROM bar WHERE y IS NOT NULL);

-- outer with nulls, inner with null tuples
SELECT * FROM foo WHERE (a, b) NOT IN (SELECT x, y FROM bar);

-- empty outer, non-empty inner
SELECT * FROM foo WHERE b = -1 AND a NOT IN (SELECT x FROM bar WHERE y <= 100);

-- outer with nulls, non-empty inner
SELECT * FROM foo WHERE b = 2 AND a NOT IN (SELECT x FROM bar WHERE y <= 100);

-- outer with nulls, empty inner
SELECT * FROM foo WHERE b = 2 AND a NOT IN (SELECT x FROM bar WHERE y = -1) order by 1, 2;

-- outer with nulls, inner with nulls
SELECT * FROM foo WHERE a NOT IN (SELECT x FROM bar);


-- Now run the same tests again, with hashjoins enabled.
set enable_hashjoin=on;
SELECT enable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- empty outer, non-empty inner
SELECT * FROM foo WHERE b = -1 AND a = ALL (SELECT x FROM bar WHERE y <= 100);

-- outer with nulls, non-empty inner
SELECT * FROM foo WHERE b = 2 AND a = ALL (SELECT x FROM bar WHERE y >=10 AND y < 20);

-- outer with nulls, empty inner
SELECT * FROM foo WHERE b = 2 AND a = ALL (SELECT x FROM bar WHERE y = -1) order by 1, 2;

-- outer with nulls, inner with nulls
SELECT * FROM foo WHERE a = ALL (SELECT x FROM bar WHERE x = 1 OR x IS NULL);

-- empty outer, non-empty inner
SELECT * FROM (SELECT * FROM foo WHERE b = -1) foo2 FULL OUTER JOIN bar ON (a = x);

-- non-empty outer, empty inner
SELECT * FROM foo FULL OUTER JOIN (SELECT * FROM bar WHERE y = -1) bar2 ON (a = x);

-- non-empty outer, non-empty inner
SELECT * FROM (SELECT * FROM foo WHERE b = 2) foo2 FULL OUTER JOIN (SELECT * FROM bar WHERE y BETWEEN 16 AND 22 OR x IS NULL) bar2 ON (a = x);

-- empty outer, non-empty inner
SELECT * FROM foo WHERE b = -1 AND (a, b) NOT IN (SELECT x, y FROM bar WHERE y <= 100);

-- outer with nulls, non-empty inner
SELECT * FROM foo WHERE (a, b) NOT IN (SELECT x, y FROM bar WHERE y <= 100);

-- outer with nulls, empty inner
SELECT * FROM foo WHERE (a, b) NOT IN (SELECT x, y FROM bar WHERE y = -1);

-- outer with nulls, inner with partial nulls
SELECT * FROM foo WHERE (a, b) NOT IN (SELECT x, y FROM bar WHERE y IS NOT NULL);

-- outer with nulls, inner with null tuples
SELECT * FROM foo WHERE (a, b) NOT IN (SELECT x, y FROM bar);

-- empty outer, non-empty inner
SELECT * FROM foo WHERE b = -1 AND a NOT IN (SELECT x FROM bar WHERE y <= 100);

-- outer with nulls, non-empty inner
SELECT * FROM foo WHERE b = 2 AND a NOT IN (SELECT x FROM bar WHERE y <= 100);

-- outer with nulls, empty inner
SELECT * FROM foo WHERE b = 2 AND a NOT IN (SELECT x FROM bar WHERE y = -1) order by 1, 2;

-- outer with nulls, inner with nulls
SELECT * FROM foo WHERE a NOT IN (SELECT x FROM bar);
