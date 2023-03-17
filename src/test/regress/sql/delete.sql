CREATE TABLE delete_test (
    id SERIAL PRIMARY KEY,
    a INT,
    b text
) DISTRIBUTED BY (id);

INSERT INTO delete_test (a) VALUES (10);
INSERT INTO delete_test (a, b) VALUES (50, repeat('x', 10000));
INSERT INTO delete_test (a) VALUES (100);

-- allow an alias to be specified for DELETE's target table
DELETE FROM delete_test AS dt WHERE dt.a > 75;

-- if an alias is specified, don't allow the original table name
-- to be referenced
DELETE FROM delete_test dt WHERE delete_test.a > 25;

SELECT id, a, char_length(b) FROM delete_test;

-- delete a row with a TOASTed value
DELETE FROM delete_test WHERE a > 25;

SELECT id, a, char_length(b) FROM delete_test;

-- issue 14417: Range Tables related relations opening's locking issue
CREATE EXTERNAL WEB TABLE dummy(x int) EXECUTE 'touch /tmp/dummy2.csv;cat /tmp/dummy2.csv' ON MASTER FORMAT 'csv';
CREATE TABLE issue_14417(x int);
DELETE FROM issue_14417 WHERE x IN (SELECT x FROM dummy);

DROP TABLE delete_test;
DROP TABLE issue_14417;
DROP EXTERNAL TABLE dummy;
