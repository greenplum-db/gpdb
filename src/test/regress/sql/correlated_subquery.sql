SET optimizer_enforce_subplans = 1;
SET optimizer_trace_fallback=on;

SELECT a = ALL (SELECT generate_series(1, 2)), a FROM (values (1),(2)) v(a);
SELECT a = ALL (SELECT generate_series(2, 2)), a FROM (values (1),(2)) v(a);
SELECT 1 = ALL (SELECT generate_series(1, 2)) FROM (values (1),(2)) v(a);
SELECT 2 = ALL (SELECT generate_series(2, 2)) FROM (values (1),(2)) v(a);
SELECT 2 = ALL (SELECT generate_series(2, 3)) FROM (values (1),(2)) v(a);
SELECT 2+1 = ALL (SELECT generate_series(2, 3)) FROM (values (1),(2)) v(a);
SELECT 2+1 = ALL (SELECT generate_series(3, 3)) FROM (values (1),(2)) v(a);
SELECT (SELECT a) = ALL (SELECT generate_series(1, 2)), a FROM (values (1),(2)) v(a);
SELECT (SELECT a) = ALL (SELECT generate_series(2, 2)), a FROM (values (1),(2)) v(a);
SELECT (SELECT a+1) = ALL (SELECT generate_series(2, 2)), a FROM (values (1),(2)) v(a);
SELECT (SELECT 1) = ALL (SELECT generate_series(1, 1)) FROM (values (1),(2)) v(a);
SELECT (SELECT 1) = ALL (SELECT generate_series(1, 2)) FROM  (values (1),(2)) v(a);
SELECT (SELECT 3) = ALL (SELECT generate_series(3, 3)) FROM  (values (1),(2)) v(a);

SELECT (SELECT 1) = ALL (SELECT generate_series(1, 1));
SELECT (SELECT 1) = ALL (SELECT generate_series(1, 2));
SELECT (SELECT 3) = ALL (SELECT generate_series(3, 3));

CREATE TABLE correlated_subquery_test(
   a varchar(100),
   b int
);
SELECT (SELECT a FROM correlated_subquery_test LIMIT 1)=ALL(SELECT a FROM correlated_subquery_test);
-- Use a transaction because following CREATE CAST doesn't necessarily play
-- nicely with other tests.
BEGIN;
CREATE CAST (integer AS text) WITH INOUT AS IMPLICIT;
SELECT (SELECT b FROM correlated_subquery_test LIMIT 1)=ALL(SELECT a FROM correlated_subquery_test);
ROLLBACK;
