-- CVE-2022-2625
-- https://github.com/postgres/postgres/commit/5919bb5a5989cda232ac3d1f8b9d90f337be2077
--
-- It's generally bad style to use CREATE OR REPLACE unnecessarily.
-- Test what happens if an extension does it anyway.
-- Replacing a shell type or operator is sort of like CREATE OR REPLACE;
-- check that too.

CREATE FUNCTION ext_cor_func() RETURNS text
  AS $$ SELECT 'ext_cor_func: original'::text $$ LANGUAGE sql;

CREATE EXTENSION test_ext_cor;  -- fail

SELECT ext_cor_func();

DROP FUNCTION ext_cor_func();

CREATE VIEW ext_cor_view AS
  SELECT 'ext_cor_view: original'::text AS col;

CREATE EXTENSION test_ext_cor;  -- fail

SELECT ext_cor_func();

SELECT * FROM ext_cor_view;

DROP VIEW ext_cor_view;

CREATE TYPE test_ext_type;

CREATE EXTENSION test_ext_cor;  -- fail

DROP TYPE test_ext_type;

-- this makes a shell "point <<@@ polygon" operator too
CREATE OPERATOR @@>> ( PROCEDURE = poly_contain_pt,
  LEFTARG = polygon, RIGHTARG = point,
  COMMUTATOR = <<@@ );

CREATE EXTENSION test_ext_cor;  -- fail

DROP OPERATOR <<@@ (point, polygon);

CREATE EXTENSION test_ext_cor;  -- now it should work

SELECT ext_cor_func();

SELECT * FROM ext_cor_view;

SELECT 'x'::test_ext_type;

SELECT point(0,0) <<@@ polygon(circle(point(0,0),1));

\dx+ test_ext_cor

--
-- CREATE IF NOT EXISTS is an entirely unsound thing for an extension
-- to be doing, but let's at least plug the major security hole in it.
--

CREATE SCHEMA ext_cine_schema;
CREATE EXTENSION test_ext_cine;  -- fail
DROP SCHEMA ext_cine_schema;

CREATE TABLE ext_cine_tab1 (x int);
CREATE EXTENSION test_ext_cine;  -- fail
DROP TABLE ext_cine_tab1;

CREATE EXTENSION test_ext_cine;
\dx+ test_ext_cine
ALTER EXTENSION test_ext_cine UPDATE TO '1.1';
\dx+ test_ext_cine
