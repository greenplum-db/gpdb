CREATE EXTENSION gp_legacy_string_agg;

SELECT string_agg('a') FROM generate_series(1, 10);

SELECT string_agg(a) FROM (VALUES('aaaa'),('bbbb'),('cccc'),(NULL)) g(a);

CREATE TABLE foo(a int, b text);
INSERT INTO foo VALUES(1, 'aaaa'),(2, 'bbbb'),(3, 'cccc'), (4, NULL);
SELECT string_agg(b ORDER BY a) FROM foo;
