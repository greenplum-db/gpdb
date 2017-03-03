DROP TABLE IF EXISTS dml_trigger_table;

CREATE TABLE dml_trigger_table
(
 name varchar(10),
 age  numeric(10),
 updated_by varchar
)
distributed by (age)
partition by range (age) (
	partition p1 start (1) end (25),
	partition p2 start (26) end (50),
	partition p3 start (51) end (75)
	);

CREATE OR REPLACE FUNCTION dml_function_1() RETURNS trigger AS
$$
BEGIN
   NEW.updated_by = 'a';
   RETURN NEW;
END
$$ LANGUAGE 'plpgsql';

CREATE TRIGGER dml_trigger_1
BEFORE INSERT or UPDATE
ON dml_trigger_table_1_prt_p1
FOR EACH ROW
EXECUTE PROCEDURE dml_function_1();