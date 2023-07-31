-- This sql file is used by mpp_gp2pg_postgres_fdw test, and it runs in
-- postgres server.

-- ===================================================================
-- create objects used through FDW pgserver server
-- ===================================================================
SET timezone = 'PST8PDT';

CREATE SCHEMA "MPP_S 1";
CREATE TABLE "MPP_S 1"."T 1" (
	c1 int,
	c2 int
);

CREATE TABLE "MPP_S 1"."T 2" (
	c1 int,
	c2 int,
	c3 smallint,
	c4 bigint,
	c5 real,
	c6 double precision,
	c7 numeric
);

-- Disable autovacuum for these tables to avoid unexpected effects of that
ALTER TABLE "MPP_S 1"."T 1" SET (autovacuum_enabled = 'false');
ALTER TABLE "MPP_S 1"."T 2" SET (autovacuum_enabled = 'false');

INSERT INTO "MPP_S 1"."T 1"
	SELECT id,
	       id % 2
	FROM generate_series(6, 10) id;

INSERT INTO "MPP_S 1"."T 2" (c1, c2, c3, c4)
	SELECT id,
	       id % 10,
	       id,
	       id
	FROM generate_series(501, 1000) id;

UPDATE "MPP_S 1"."T 2"
  SET c5 = c1 * 0.001,
      c6 = c1 * 0.001,
      c7 = c1 * 0.001;

ANALYZE "MPP_S 1"."T 1";
ANALYZE "MPP_S 1"."T 2";