-- @Description Tests loop and update in function with exception
-- 

DROP TABLE IF EXISTS db CASCADE;
CREATE TABLE db (a INT NOT NULL, b TEXT);
DROP FUNCTION IF EXISTS insert_db(key INT, data TEXT) CASCADE;

CREATE FUNCTION insert_db(key INT, data TEXT) RETURNS VOID AS
$$
BEGIN
	LOOP
   	-- first try to update the key
	UPDATE db SET b = data WHERE a = key;
	IF found THEN
		RETURN;
	END IF;
	-- not there, so try to insert the key
	BEGIN
		INSERT INTO db(a,b) VALUES (NULL, data);
	RETURN;
	EXCEPTION WHEN OTHERS THEN
			INSERT INTO db(a,b) VALUES (2, 'dummy');
			INSERT INTO db(a,b) VALUES (key, 'dummy');
	END;
	END LOOP;
END;
$$
LANGUAGE plpgsql;

SELECT insert_db(1, 'david');
SELECT * from db;

INSERT INTO db select *,'abcdefghijklmnopqrstuvwxyz' from generate_series(1, 100);
DROP FUNCTION IF EXISTS insert_db1(flag INT) CASCADE;

CREATE FUNCTION insert_db1(flag INT) RETURNS INT AS
$$
DECLARE
	rec   record;
	x INT;
BEGIN
	FOR rec IN
	SELECT *
	FROM   db
	WHERE  a % 2 = 0
	LOOP
		x = rec.a * flag;
		IF x = 400 THEN
			INSERT INTO db VALUES (NULL);
		ELSE
			INSERT INTO db VALUES (rec.a);
		END IF;
	END LOOP;
	RETURN 1;
	EXCEPTION WHEN OTHERS THEN
	BEGIN
		INSERT INTO db VALUES (999), (9999), (99999);
		RAISE NOTICE 'Exception Hit !!!';
		RETURN -1;
	END;
END;
$$
LANGUAGE plpgsql;

SELECT insert_db1(1);
SELECT count(*) FROM db;

SELECT insert_db1(200);
SELECT count(*) FROM db;
