-- Create some tables
CREATE TABLE aoco_table (a int, b int) with (appendonly=true, orientation=column);
CREATE TABLE heap_table (a int NOT NULL, b int);

-- Create indexes
CREATE UNIQUE INDEX idx_a ON heap_table USING btree (a);
CREATE INDEX idx_b ON aoco_table USING btree (b);

-- Create a function that will be used as the trigger
CREATE FUNCTION trigger_func() RETURNS trigger LANGUAGE plpgsql NO SQL AS '
BEGIN
RAISE NOTICE ''trigger_func() called: action = %, when = %, level = %'', TG_OP, TG_WHEN, TG_LEVEL;
RETURN NULL;
END;';

-- Create a trigger
CREATE TRIGGER before_heap_ins_trig BEFORE INSERT ON heap_table
FOR EACH ROW EXECUTE PROCEDURE trigger_func();

-- Create a user-defined type
CREATE TYPE user_defined_type AS (r real, i real);

-- Create aggregate functions
CREATE AGGREGATE newcnt ("any") (
   sfunc = int8inc_any, stype = int8,
   initcond = '0'
);

-- Create user-defined operator
CREATE OPERATOR ## (
   leftarg = path,
   rightarg = path,
   procedure = path_inter,
   commutator = ##
);

-- Create user-defined conversion
CREATE CONVERSION myconv FOR 'LATIN1' TO 'UTF8' FROM iso8859_1_to_utf8;
