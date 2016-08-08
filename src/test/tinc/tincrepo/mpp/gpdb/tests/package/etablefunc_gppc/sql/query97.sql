-- Negative: Test system behavior and error message when sub query is non-SELECT query
-- such as INSERT, INSERT ... INTO, UPDATE, DELETE, 
-- or even DDL (CREATE/ALTER TABLE, CREATE/ALTER VIEW, CREATE/ALTER INDEX, VACUUM, and etc)

SELECT * FROM transform(
TABLE( INSERT INTO randtable values (11, 'value_11'))
);

SELECT * FROM transform(
TABLE( UPDATE randtable SET value='value_11' WHERE id=11)
);

SELECT * FROM transform(
TABLE( DELETE FROM randtable WHERE id=11;)
);

SELECT * FROM transform(
TABLE( select i, 'foo'||i into randtable2 from generate_series(11,15) i)
);

SELECT * FROM ud_project(
TABLE( CREATE TABLE neg_test (a int, b text)));

SELECT * FROM ud_project(
TABLE( ALTER TABLE randtable RENAME id TO id2));


SELECT * FROM ud_project(
TABLE( vacuum));

