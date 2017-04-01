-- 
-- @created 2013-03-14 12:00:00
-- @modified 2012-03-14 12:00:00
-- @tags storage
-- @product_version gpdb: 4.2, 4.3
-- @description Transaction on AO table

CREATE OR REPLACE FUNCTION check_mirror_down() RETURNS boolean AS
$$
DECLARE
    counter integer :=0;
    num_loops integer :=0;
BEGIN

LOOP
    IF num_loops > 30 THEN
    RETURN false;
    END IF;

    SELECT count(*) FROM gp_segment_configuration WHERE role='m' AND status='d' INTO counter;

    IF counter > 0 THEN
    RETURN true;
    END IF;
    PERFORM pg_sleep(10);
    num_loops := num_loops + 1;
END LOOP;

END;
$$ LANGUAGE plpgsql;

BEGIN;

INSERT INTO ta 
VALUES(0),(1);

-- Sleep introduced so that all the mirror segments gets
-- killed before aborting
SELECT * FROM check_mirror_down();

ABORT;

INSERT INTO ta
VALUES(0),(1);


