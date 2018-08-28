-- The following is a cut-down version of PostgreSQL 9.3's "alter_generic"
-- test, containing the opclass and opfamily parts of that test. Backported
-- to GPDB, because we have no other tests for the opclass and opfamily DDL
-- commands. These can be removed once we merge with 9.3 (the DROP+CREATE
-- with same name tests are not in the upstream version though!)

-- Clean up in case a prior regression run failed
SET client_min_messages TO 'warning';

DROP ROLE IF EXISTS regtest_alter_user1;
DROP ROLE IF EXISTS regtest_alter_user2;
DROP ROLE IF EXISTS regtest_alter_user3;

RESET client_min_messages;

CREATE USER regtest_alter_user3;
CREATE USER regtest_alter_user2;
CREATE USER regtest_alter_user1 IN ROLE regtest_alter_user3;

CREATE SCHEMA alt_nsp1;

GRANT ALL ON SCHEMA alt_nsp1 TO public;

SET search_path = alt_nsp1, public;

--
-- OpFamily and OpClass
--
CREATE OPERATOR FAMILY alt_opf1 USING hash;
CREATE OPERATOR FAMILY alt_opf2 USING hash;

-- Drop, and create another opfamily with same name. Should work, unless
-- we forgot to dispatch the DROP to a segment.
DROP OPERATOR FAMILY alt_opf1 USING hash;
CREATE OPERATOR FAMILY alt_opf1 USING hash;

ALTER OPERATOR FAMILY alt_opf1 USING hash OWNER TO regtest_alter_user1;
ALTER OPERATOR FAMILY alt_opf2 USING hash OWNER TO regtest_alter_user1;

CREATE OPERATOR CLASS alt_opc1 FOR TYPE uuid USING hash AS STORAGE uuid;
CREATE OPERATOR CLASS alt_opc2 FOR TYPE uuid USING hash AS STORAGE uuid;

-- Also test DROP+CREATE for opclasses
DROP OPERATOR CLASS alt_opc1 USING hash;
CREATE OPERATOR CLASS alt_opc1 FOR TYPE uuid USING hash AS STORAGE uuid;

ALTER OPERATOR CLASS alt_opc1 USING hash OWNER TO regtest_alter_user1;
ALTER OPERATOR CLASS alt_opc2 USING hash OWNER TO regtest_alter_user1;

SET SESSION AUTHORIZATION regtest_alter_user1;

ALTER OPERATOR FAMILY alt_opf1 USING hash RENAME TO alt_opf2;  -- failed (name conflict)
ALTER OPERATOR FAMILY alt_opf1 USING hash RENAME TO alt_opf3;  -- OK
ALTER OPERATOR FAMILY alt_opf2 USING hash OWNER TO regtest_alter_user2;  -- failed (no role membership)
ALTER OPERATOR FAMILY alt_opf2 USING hash OWNER TO regtest_alter_user3;  -- OK

ALTER OPERATOR CLASS alt_opc1 USING hash RENAME TO alt_opc2;  -- failed (name conflict)
ALTER OPERATOR CLASS alt_opc1 USING hash RENAME TO alt_opc3;  -- OK
ALTER OPERATOR CLASS alt_opc2 USING hash OWNER TO regtest_alter_user2;  -- failed (no role membership)
ALTER OPERATOR CLASS alt_opc2 USING hash OWNER TO regtest_alter_user3;  -- OK

RESET SESSION AUTHORIZATION;

-- Test adding operators to an existing opfamily as that requires oid
-- dispatching to the segments

-- Should fail. duplicate operator number / function number in ALTER OPERATOR FAMILY ... ADD FUNCTION
CREATE OPERATOR FAMILY alt_opf17 USING btree;
ALTER OPERATOR FAMILY alt_opf17 USING btree ADD OPERATOR 1 < (int4, int4), OPERATOR 1 < (int4, int4); -- operator # appears twice in same statement
ALTER OPERATOR FAMILY alt_opf17 USING btree ADD OPERATOR 1 < (int4, int4); -- operator 1 requested first-time
ALTER OPERATOR FAMILY alt_opf17 USING btree ADD OPERATOR 1 < (int4, int4); -- operator 1 requested again in separate statement
ALTER OPERATOR FAMILY alt_opf17 USING btree ADD
  OPERATOR 1 < (int4, int2) ,
  OPERATOR 2 <= (int4, int2) ,
  OPERATOR 3 = (int4, int2) ,
  OPERATOR 4 >= (int4, int2) ,
  OPERATOR 5 > (int4, int2) ,
  FUNCTION 1 btint42cmp(int4, int2) ,
  FUNCTION 1 btint42cmp(int4, int2);    -- procedure 1 appears twice in same statement
ALTER OPERATOR FAMILY alt_opf17 USING btree ADD
  OPERATOR 1 < (int4, int2) ,
  OPERATOR 2 <= (int4, int2) ,
  OPERATOR 3 = (int4, int2) ,
  OPERATOR 4 >= (int4, int2) ,
  OPERATOR 5 > (int4, int2) ,
  FUNCTION 1 btint42cmp(int4, int2);    -- procedure 1 appears first time
ALTER OPERATOR FAMILY alt_opf17 USING btree ADD
  OPERATOR 1 < (int4, int2) ,
  OPERATOR 2 <= (int4, int2) ,
  OPERATOR 3 = (int4, int2) ,
  OPERATOR 4 >= (int4, int2) ,
  OPERATOR 5 > (int4, int2) ,
  FUNCTION 1 btint42cmp(int4, int2);    -- procedure 1 requested again in separate statement
DROP OPERATOR FAMILY alt_opf17 USING btree;


-- GPDB: Test a GiST ordering operator.

CREATE TYPE my_gisttest_type AS (f1 int, f2 text);

CREATE FUNCTION my_gisttest_function(geom1 my_gisttest_type, geom2 my_gisttest_type)
    RETURNS float8
    AS $$
        select 1.0::float8
    $$ language SQL;

CREATE OPERATOR <-> (
    LEFTARG = my_gisttest_type, RIGHTARG = my_gisttest_type, PROCEDURE = my_gisttest_function,
    COMMUTATOR = '<->'
);

CREATE OPERATOR CLASS my_gisttest_op_class
DEFAULT FOR TYPE my_gisttest_type USING GIST AS
OPERATOR        13       <-> FOR ORDER BY pg_catalog.float_ops;
