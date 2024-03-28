-- We added special flags to the commit log of one-phase transactions 
-- so we can detect such transactions during logical decoding.
-- First execute an one-phase transaction, 
-- then get the decoded content of the transaction log on the segment, 
-- and check the flag of the one-phase.

-- table and function defination
CREATE TABLE test_table(id int PRIMARY KEY);

-- Create slot on segment
CREATE OR REPLACE FUNCTION create_slot() RETURNS void AS $$
DECLARE
  -- buf is only used to receive the return value of SELECT. 
  -- Otherwise, a syntax error will occur. The same usage is used in other functions.
  buf text;
BEGIN
  SELECT pg_create_logical_replication_slot('regression_slot_p', 'test_decoding') INTO buf;
END;
$$ language plpgsql;

-- Create slot on coordinator and segments
CREATE OR REPLACE FUNCTION create_slot_on_all_segments() RETURNS void AS $$
DECLARE
  buf text;
BEGIN
  SELECT pg_create_logical_replication_slot('regression_slot_p', 'test_decoding') INTO buf; -- create slot on coordinator
  SELECT create_slot() FROM gp_dist_random('gp_id') INTO buf;
END;
$$ language plpgsql;

-- Drop slot on segment
CREATE OR REPLACE FUNCTION drop_slot() RETURNS void AS $$
DECLARE
  buf text;
BEGIN
  SELECT * FROM pg_drop_replication_slot('regression_slot_p') INTO buf;
END;
$$ language plpgsql;

-- Drop slot on coordinator and segments
CREATE OR REPLACE FUNCTION drop_slot_on_all_segments() RETURNS void AS $$
DECLARE
  buf text;
BEGIN
  SELECT * FROM pg_drop_replication_slot('regression_slot_p') INTO buf;
  SELECT drop_slot() FROM gp_dist_random('gp_id') INTO buf;
END;
$$ language plpgsql;

-- Only one piece of data is inserted, which is a one-phase transaction.
CREATE OR REPLACE FUNCTION execute_one_phase_transaction() RETURNS void AS $$
DECLARE
BEGIN
    INSERT INTO test_table VALUES(700);
END;
$$ language plpgsql;

-- Use the 'pg_logical_slot_get_changes' command to get the decoded log content on one segment.
-- Normally pg_logical_slot_get_changes will return 'BEGIN xid'.
-- For one-phase transactions, a flag is added, returning 'ONE-PHASE,BEGIN xid'.
-- The specific implementation code is in the 'pg_output_begin' function of test_decoding.c.
CREATE OR REPLACE FUNCTION get_change() RETURNS void AS $$
DECLARE
  buf text;
  get_change_result text;
BEGIN
  SELECT data FROM pg_logical_slot_get_changes('regression_slot_p', NULL, NULL) INTO buf;
  IF buf <> '' THEN -- Only one segment will generate logs.
    SELECT * FROM SPLIT_PART(buf, ',', 1) INTO get_change_result;

    IF get_change_result = 'ONE-PHASE' THEN
      raise notice 'result match';
    ELSE
      raise notice 'result not match';
    END IF;
  END IF;
END;
$$ language plpgsql;

-- Check the logs on each segment.
CREATE OR REPLACE FUNCTION test_one_phase() RETURNS void AS $$
DECLARE
  buf text;
BEGIN
  -- All segments will execute this function, 
  -- but only one segment can get the decoded log content and check it.
  SELECT get_change() FROM gp_dist_random('gp_id') INTO buf;
END;
$$ language plpgsql;

-- Start test
SELECT * FROM create_slot_on_all_segments();
SELECT * FROM execute_one_phase_transaction();
SELECT * FROM test_one_phase();

-- Clean
SELECT * FROM drop_slot_on_all_segments();
DROP TABLE test_table;
DROP FUNCTION execute_one_phase_transaction;
DROP FUNCTION test_one_phase;
