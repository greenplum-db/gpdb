SET optimizer_enforce_subplans = 1;
SET optimizer_trace_fallback=on;
SET client_min_messages=log;

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
