--
-- test that index-backed constraints have matching index and constraint names
--    under all possibilities we can think of and that we cannot break this
--    invariant
--
-- We have type(UNIQUE, PRIMARY KEY, EXCLUSION) x 
--         create(CREATE TABLE usage, ALTER TABLE ADD CONSTRAINT usage) x
--         rename(as is, RENAME CONSTRAINT, RENAME INDEX)
--
-- We do try all rename() with both create() as we assume that once a
--         constraint has been created, it acts the same. 
--

DROP schema IF EXISTS index_constraint_naming;
CREATE schema index_constraint_naming;
SET search_path='index_constraint_naming';

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
    JOIN
        pg_namespace ns
    ON
        con.connamespace = ns.oid
    WHERE
        con.contype != 'c'
    AND
        ns.nspname = 'index_constraint_naming'
    ORDER BY conrelid
  ;
$fn$;


-- UNIQUE CONSTRAINT

-- UNIQUE CONSTRAINT: make sure  a simple table has constraint and index names matching
CREATE TABLE st_u (a int, b int, UNIQUE(a,b));
SELECT table_name,constraint_name,index_name,constraint_type FROM constraints_and_indices() WHERE (constraint_name::text=index_name::text AND constraint_type='u');

-- UNIQUE CONSTRAINT: create table LIKE st and check constraint propagates
CREATE TABLE st_u_like (LIKE st_u INCLUDING CONSTRAINTS INCLUDING INDEXES);
SELECT table_name,constraint_name,index_name,constraint_type FROM constraints_and_indices() WHERE (constraint_name::text=index_name::text AND constraint_type='u');

-- UNIQUE CONSTRAINT: create table INHERITS st and check constraint NOT inherited
CREATE TABLE st_u_inherits (c int) INHERITS (st_u);
SELECT table_name,constraint_name,index_name,constraint_type FROM constraints_and_indices() WHERE (constraint_name::text=index_name::text AND constraint_type='u');

-- UNIQUE CONSTRAINT 2: make sure  a simple table has constraint and index names matching
CREATE TABLE st_u2 (a int, b int);
ALTER TABLE st_u2 ADD UNIQUE(a,b);
SELECT table_name,constraint_name,index_name,constraint_type FROM constraints_and_indices() WHERE (constraint_name::text=index_name::text AND constraint_type='u');

-- UNIQUE CONSTRAINT: ALTER CONSTRAINT NAME
ALTER TABLE st_u2 RENAME CONSTRAINT st_u2_a_b_key TO st_u2_a_b_key_r;
SELECT table_name,constraint_name,index_name,constraint_type FROM constraints_and_indices() WHERE (constraint_name::text=index_name::text AND constraint_type='u');

-- UNIQUE CONSTRAINT: ALTER INDEX
ALTER INDEX st_u2_a_b_key_r RENAME TO st_u2_a_b_key_r_i;
SELECT table_name,constraint_name,index_name,constraint_type FROM constraints_and_indices() WHERE (constraint_name::text=index_name::text AND constraint_type='u');

-- UNIQUE CONSTRAINT: cannot rename constraint to existing name
ALTER TABLE st_u2 RENAME CONSTRAINT st_u2_a_b_key_r_i TO st_u_a_b_key;

-- UNIQUE CONSTRAINT: cannot rename index to existing name
ALTER INDEX st_u2_a_b_key_r_i RENAME TO st_u_a_b_key;


-- PRIMARY KEY CONSTRAINT

-- PRIMARY KEY CONSTRAINT: make sure  a simple table has constraint and index names matching
CREATE TABLE st_pk (a int, b int, PRIMARY KEY(a,b));
SELECT table_name,constraint_name,index_name,constraint_type FROM constraints_and_indices() WHERE (constraint_name::text=index_name::text AND constraint_type='p');

-- PRIMARY KEY CONSTRAINT: create table LIKE st and check constraint propagates
CREATE TABLE st_pk_like (LIKE st_pk INCLUDING CONSTRAINTS INCLUDING INDEXES);
SELECT table_name,constraint_name,index_name,constraint_type FROM constraints_and_indices() WHERE (constraint_name::text=index_name::text AND constraint_type='p');

-- PRIMARY KEY CONSTRAINT: create table INHERITS st and check constraint NOT inherited
CREATE TABLE st_pk_inherits (c int) INHERITS (st_pk);
SELECT table_name,constraint_name,index_name,constraint_type FROM constraints_and_indices() WHERE (constraint_name::text=index_name::text AND constraint_type='p');

-- PRIMARY KEY CONSTRAINT 2: make sure  a simple table has constraint and index names matching
CREATE TABLE st_pk2 (a int, b int);
ALTER TABLE st_pk2 ADD PRIMARY KEY(a,b);
SELECT table_name,constraint_name,index_name,constraint_type FROM constraints_and_indices() WHERE (constraint_name::text=index_name::text AND constraint_type='p');

-- PRIMARY KEY CONSTRAINT: ALTER CONSTRAINT NAME
ALTER TABLE st_pk2 RENAME CONSTRAINT st_pk2_pkey TO st_pk2_pkey_r;
SELECT table_name,constraint_name,index_name,constraint_type FROM constraints_and_indices() WHERE (constraint_name::text=index_name::text AND constraint_type='p');

-- PRIMARY KEY CONSTRAINT: ALTER INDEX
ALTER INDEX st_pk2_pkey_r RENAME TO st_pk2_pkey_r_i;
SELECT table_name,constraint_name,index_name,constraint_type FROM constraints_and_indices() WHERE (constraint_name::text=index_name::text AND constraint_type='p');

-- PRIMARY KEY CONSTRAINT: cannot rename constraint to existing name
ALTER TABLE st_pk2 RENAME CONSTRAINT st_pk2_pkey_r_i  TO st_pk_pkey;

-- PRIMARY KEY CONSTRAINT: cannot rename index to existing name
ALTER INDEX st_pk2_pkey_r_i RENAME TO st_pk_pkey;

-- EXCLUSION CONSTRAINT

-- EXCLUSION CONSTRAINT: GPDB does not support exclusion constraints
CREATE TABLE st_x (a int, b int, EXCLUDE (a with =, b with =));
-- Should fail
SELECT table_name,constraint_name,index_name,constraint_type FROM constraints_and_indices() WHERE (constraint_name::text=index_name::text AND constraint_type='x');

-- U_P
ALTER TABLE st_u2 RENAME CONSTRAINT st_u2_a_b_key_r_i TO st_pk_pkey;
ALTER INDEX st_u2_a_b_key_r_i RENAME TO st_pk_pkey;

-- P_U
ALTER TABLE st_pk2 RENAME CONSTRAINT st_pk2_pkey_r_i TO st_u2_a_b_key_r_i;
ALTER INDEX st_pk2_pkey_r_i RENAME TO st_u2_a_b_key_r_i;
