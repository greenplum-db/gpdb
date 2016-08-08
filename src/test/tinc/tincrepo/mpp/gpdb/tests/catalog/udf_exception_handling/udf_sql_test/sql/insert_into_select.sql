-- @Description Tests insert into select from function and function calling function success and failure scenarios with exceptions
-- 

DROP TABLE IF EXISTS output CASCADE;
DROP TABLE IF EXISTS input CASCADE;
DROP FUNCTION IF EXISTS inputArray() CASCADE;
DROP FUNCTION IF EXISTS outputArray() CASCADE;

CREATE TABLE input (x INT);
INSERT INTO input SELECT * from generate_series(1, 10);

CREATE OR REPLACE FUNCTION inputArray()
RETURNS INT[] AS $$
DECLARE
	theArray INT[];
BEGIN
	SELECT array(SELECT * FROM input ORDER BY x) INTO theArray;
	RETURN theArray;
EXCEPTION WHEN OTHERS THEN
        RAISE NOTICE 'Catching the exception ...%', SQLERRM;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION outputArray()
RETURNS VOID AS $$
BEGIN
	CREATE TABLE output AS
	SELECT inputArray()::INT[] AS x;
	INSERT INTO output SELECT inputArray()::INT[];
	vacuum;
EXCEPTION WHEN OTHERS THEN
	RAISE NOTICE 'Catching the exception ...%', SQLERRM;
END;
$$ LANGUAGE plpgsql;

SELECT outputArray();
SELECT * FROM output;


CREATE OR REPLACE FUNCTION outputArray()
RETURNS VOID AS $$
BEGIN
	CREATE TABLE output AS
	SELECT inputArray()::INT[] AS x;
	INSERT INTO output SELECT inputArray()::INT[];
EXCEPTION WHEN OTHERS THEN
	RAISE NOTICE 'Catching the exception ...%', SQLERRM;
END;
$$ LANGUAGE plpgsql;

SELECT outputArray();
SELECT * FROM output;

INSERT INTO output SELECT inputArray()::INT[];
SELECT * FROM output;

