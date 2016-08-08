-- Corrupt the free list and test drop table

-- Create a few tables
CREATE TABLE foo1(a int);
CREATE TABLE foo2(a int);
CREATE TABLE foo3(a int);
CREATE TABLE foo4(a int);

-- Drop a few tables to create a free list
DROP TABLE foo1;
DROP TABLE foo2;
DROP TABLE foo3;

-- Corrupt the free list
SET allow_system_table_mods = 'DML';
SET gp_permit_persistent_metadata_update = true;
UPDATE gp_persistent_relation_node 
    SET previous_free_tid = '(0, 0)'
    WHERE persistent_serial_num = (SELECT max(persistent_serial_num)
                                   FROM gp_persistent_relation_node
                                   WHERE previous_free_tid > '(0, 0)')
    AND previous_free_tid > '(0, 0)';

-- This should reset the free list
DROP TABLE foo4;

SELECT count(*) = 2 FROM gp_persistent_relation_node WHERE persistent_serial_num = 1 AND previous_free_tid > '(0, 0)';

-- Recreate the persistent tables
SELECT gp_persistent_reset_all();
SELECT gp_persistent_build_all(true);
