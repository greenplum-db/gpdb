DROP TABLE IF EXISTS dml_trigger_table_1;
CREATE TABLE dml_trigger_table_1
(
 name varchar(10),
 age  numeric(10)
)
distributed by (age);

INSERT INTO dml_trigger_table_1 VALUES('TEST',10);

CREATE OR REPLACE FUNCTION dml_function_1() RETURNS trigger AS
$$
BEGIN
   RETURN OLD;
END
$$ LANGUAGE 'plpgsql';

CREATE TRIGGER dml_trigger_1
BEFORE UPDATE or INSERT
ON dml_trigger_table_1
FOR EACH ROW
EXECUTE PROCEDURE dml_function_1();