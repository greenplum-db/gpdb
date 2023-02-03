-- This sql file is used by gp2pg_postgres_fdw test, and it runs in
-- postgres dataserver.

-- ===================================================================
-- create objects used through FDW pgserver server
-- ===================================================================
SET timezone = 'PST8PDT';

CREATE TYPE user_enum AS ENUM ('foo', 'bar', 'buz');
CREATE SCHEMA "S 1";
CREATE TABLE "S 1"."T 1" (
	"C 1" int NOT NULL,
	c2 int NOT NULL,
	c3 text,
	c4 timestamptz,
	c5 timestamp,
	c6 varchar(10),
	c7 char(10),
	c8 user_enum,
	CONSTRAINT t1_pkey PRIMARY KEY ("C 1")
);
CREATE TABLE "S 1"."T 2" (
	c1 int NOT NULL,
	c2 text,
	CONSTRAINT t2_pkey PRIMARY KEY (c1)
);
CREATE TABLE "S 1"."T 3" (
	c1 int NOT NULL,
	c2 int NOT NULL,
	c3 text,
	CONSTRAINT t3_pkey PRIMARY KEY (c1)
);
CREATE TABLE "S 1"."T 4" (
	c1 int NOT NULL,
	c2 int NOT NULL,
	c3 text,
	CONSTRAINT t4_pkey PRIMARY KEY (c1)
);

-- Disable autovacuum for these tables to avoid unexpected effects of that
ALTER TABLE "S 1"."T 1" SET (autovacuum_enabled = 'false');
ALTER TABLE "S 1"."T 2" SET (autovacuum_enabled = 'false');
ALTER TABLE "S 1"."T 3" SET (autovacuum_enabled = 'false');
ALTER TABLE "S 1"."T 4" SET (autovacuum_enabled = 'false');

INSERT INTO "S 1"."T 1"
	SELECT id,
	       id % 10,
	       to_char(id, 'FM00000'),
	       '1970-01-01'::timestamptz + ((id % 100) || ' days')::interval,
	       '1970-01-01'::timestamp + ((id % 100) || ' days')::interval,
	       id % 10,
	       id % 10,
	       'foo'::user_enum
	FROM generate_series(1, 1000) id;
INSERT INTO "S 1"."T 2"
	SELECT id,
	       'AAA' || to_char(id, 'FM000')
	FROM generate_series(1, 100) id;
INSERT INTO "S 1"."T 3"
	SELECT id,
	       id + 1,
	       'AAA' || to_char(id, 'FM000')
	FROM generate_series(1, 100) id;
DELETE FROM "S 1"."T 3" WHERE c1 % 2 != 0;	-- delete for outer join tests
INSERT INTO "S 1"."T 4"
	SELECT id,
	       id + 1,
	       'AAA' || to_char(id, 'FM000')
	FROM generate_series(1, 100) id;
DELETE FROM "S 1"."T 4" WHERE c1 % 3 != 0;	-- delete for outer join tests

ANALYZE "S 1"."T 1";
ANALYZE "S 1"."T 2";
ANALYZE "S 1"."T 3";
ANALYZE "S 1"."T 4";

-- ===================================================================
-- WHERE with remotely-executable conditions
-- ===================================================================
-- user-defined operator/function
CREATE FUNCTION postgres_fdw_abs(int) RETURNS int AS $$
BEGIN
RETURN abs($1);
END
$$ LANGUAGE plpgsql IMMUTABLE;

CREATE OPERATOR === (
    LEFTARG = int,
    RIGHTARG = int,
    PROCEDURE = int4eq,
    COMMUTATOR = ===
);

-- ===================================================================
-- Aggregate and grouping queries
-- ===================================================================
-- User defined function for user defined aggregate, VARIADIC
create function least_accum(anyelement, variadic anyarray)
returns anyelement language sql as
  'select least($1, min($2[i])) from generate_subscripts($2,1) g(i)';
create aggregate least_agg(variadic items anyarray) (
  stype = anyelement, sfunc = least_accum
);

-- Testing USING OPERATOR() in ORDER BY within aggregate.
-- For this, we need user defined operators along with operator family and
-- operator class.  Create those and then add them in extension.  Note that
-- user defined objects are considered unshippable unless they are part of
-- the extension.
create operator public.<^ (
 leftarg = int4,
 rightarg = int4,
 procedure = int4eq
);

create operator public.=^ (
 leftarg = int4,
 rightarg = int4,
 procedure = int4lt
);

create operator public.>^ (
 leftarg = int4,
 rightarg = int4,
 procedure = int4gt
);

create operator family my_op_family using btree;

create function my_op_cmp(a int, b int) returns int as
  $$begin return btint4cmp(a, b); end $$ language plpgsql;

create operator class my_op_class for type int using btree family my_op_family as
 operator 1 public.<^,
 operator 3 public.=^,
 operator 5 public.>^,
 function 1 my_op_cmp(int, int);


 -- Test that trigger on remote table works as expected
CREATE OR REPLACE FUNCTION "S 1".F_BRTRIG() RETURNS trigger AS $$
BEGIN
    NEW.c3 = NEW.c3 || '_trig_update';
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Test various RETURN statements in BEFORE triggers.
CREATE FUNCTION trig_row_before_insupdate() RETURNS TRIGGER AS $$
  BEGIN
    NEW.f2 := NEW.f2 || ' triggered !';
    RETURN NEW;
  END
$$ language plpgsql;

-- Test with a trigger returning NULL
CREATE FUNCTION trig_null() RETURNS TRIGGER AS $$
  BEGIN
    RETURN NULL;
  END
$$ language plpgsql;

-- Test that remote triggers work with insert tuple routing
create function br_insert_trigfunc() returns trigger as $$
begin
	new.b := new.b || ' triggered !';
	return new;
end
$$ language plpgsql;