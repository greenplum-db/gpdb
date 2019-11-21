ALTER TABLE tsquery_tbl ALTER COLUMN b TYPE TSQUERY USING b::tsquery;
SELECT * FROM tsquery_tbl;
SELECT * FROM tsquery_tbl WHERE b @> 'New';
