--
-- test that index-backed constraints have matching index and constraint names
--   that work with pg_upgrade.  We are either testing results here or simply
--   creating tables that will be placed in ICW so that pg_upgrade will see them
--   in testing.

DROP FUNCTION IF EXISTS constraints_and_indices();
CREATE FUNCTION constraints_and_indices() RETURNS TABLE(table_name regclass, constraint_name name, index_name regclass, constraint_type char)
  LANGUAGE SQL STABLE STRICT AS $fn$
    SELECT
      con.conrelid::regclass,
      con.conname,
      con.conindid::regclass,
      con.contype::char
    FROM
        pg_constraint con
    WHERE
        con.contype != 'c'
    ORDER BY conrelid
  ;
$fn$;

-- *************************************************************
-- Renamed Table With Constraint Scenario...create it in ICW so it exists for
-- pg_upgrade

CREATE TABLE rename_table_o (a INT, b INT, UNIQUE (a,b));
SELECT table_name,* FROM constraints_and_indices() WHERE table_name::text='rename_table_o';

ALTER TABLE rename_table_o RENAME TO rename_table_r;
SELECT table_name,* FROM constraints_and_indices() WHERE table_name::text='rename_table_r';


-- *************************************************************
-- Name Collision Scenario...create it in ICW

-- hoard index name in pg_class
CREATE TYPE table_collision_a_b_key AS (my_constraint int);

-- create table with constraint called same as the type above, such that an index name collision occurs
-- Due to the name collision from the type, the table constraint name gets named to 'mytype_for_upgrade1' instead of the expected 'mytype_for_upgrade'.
-- The index name gets named to 'test_a_b_pkey1' instead of the expected 'test_a_b_pkey'
-- Note: until we implement ALTER TABLE i1 ATTACH INDEX i2, this needs to
-- throw an error relatior "table_collision_a_b_key1" exists for upgrade
-- to work correctly
CREATE TABLE table_collision (a INT, b INT, UNIQUE (a,b)) DISTRIBUTED BY (a);

-- show constraint and index names
SELECT table_name,* FROM constraints_and_indices() WHERE table_name::text='table_collision';

-- drop the type since it is no longer needed
DROP TYPE table_collision_a_b_key;


-- *************************************************************
-- Exchange Partition Scenario

-- Create a partition table with a primary key constraint
CREATE TABLE part_table_for_upgrade (a INT, b INT) DISTRIBUTED BY (a) PARTITION BY RANGE(b) (PARTITION alpha  END (3), PARTITION beta START (3));
ALTER TABLE part_table_for_upgrade ADD PRIMARY KEY(a, b);
-- Create a table to be used as a partition exchange.
CREATE TABLE like_table (like part_table_for_upgrade INCLUDING CONSTRAINTS INCLUDING INDEXES) DISTRIBUTED BY (a) ;

-- show constraint and index names
SELECT table_name,* FROM constraints_and_indices() WHERE table_name::text='part_table_for_upgrade';
SELECT table_name,* FROM constraints_and_indices() WHERE table_name::text='like_table';

-- Exchange the beta partition with like_table.
-- Everything gets swapped, but the constraint index name of like_table does not match with part_table_for_upgrade_1_prt_beta.
ALTER TABLE part_table_for_upgrade EXCHANGE PARTITION beta WITH TABLE like_table;

-- show constraint and index names for each table
SELECT table_name,* FROM constraints_and_indices() WHERE table_name::text='part_table_for_upgrade';
SELECT table_name,* FROM constraints_and_indices() WHERE table_name::text='like_table';

-- Drop part_table_for_upgrade before upgrade since that is not where the issue is.
DROP TABLE part_table_for_upgrade CASCADE;

-- Verify that the constroint in like_table was NOT dropped
SELECT table_name,* FROM constraints_and_indices() WHERE table_name::text='like_table';

-- *************************************************************
--Name Collision Scenario when Unifying Constraint & Index Names
-- NOTE: I think this is moot in our current fix but I include it here for
--  completeness.

-- create a table with constraint names that will collide with their index names
-- the check constraint name 'test_a_key' will potentially collide with the my_constraint2 index name in GPDB5
-- the check constraint name 'test_a_b_key' will potentially collide with the my_constraint2 index name in GPDB6
CREATE TABLE test_cc (a INT, b INT, CONSTRAINT test_cc_a_key CHECK (b > -42), CONSTRAINT test_cc_a_b_key CHECK (a+b > -1), CONSTRAINT my_constraint2 UNIQUE (a,b));

-- show unique constraint and index names (see prerequisites above to install the following function)
SELECT table_name,* FROM constraints_and_indices() WHERE table_name::text='test_cc';

-- show check constraint names
select conname as check_constraint_name from pg_constraint where conrelid = (select oid from pg_class where relname = 'test_cc') and contype = 'c';
