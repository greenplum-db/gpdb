ALTER TABLE texttable ADD column n9 text;

UPDATE texttable SET n9='data' WHERE s1 LIKE '___';
