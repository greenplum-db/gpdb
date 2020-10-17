--
-- testing that constraint types that are backed by indices(UNIQUE, PRIMARY KEY,
-- EXCLUSION) have constraint names that match the name of their corresponding
-- indices.  This file covers partition tables only, and covers all mechanisms
-- of DDL use of partition tables(EXCHANGE, ADD PARTITION, CREATE, etc).  We do
-- so for a TWO LEVEL(root and leaf) hierarchy and a THREE LEVEL hierarchy
--
-- We also add test here for partitioned indices.  Logically, these are more
-- or less tested via index-backed constraints, but it is a different DDL so
-- we have to assume as testers that the server code paths are different.
--
-- we have our test cases here, but the expected file contains the desired
-- outcomes.  In other words, we do not validate the results here but do so
-- in the expected file.

DROP schema IF EXISTS index_constraint_naming_partition cascade;
CREATE schema index_constraint_naming_partition;
SET search_path='index_constraint_naming_partition';

DROP FUNCTION IF EXISTS partition_tables(text);
CREATE FUNCTION partition_tables(tab text) RETURNS TABLE(partition_name regclass, parent_name regclass, root_name regclass)
  LANGUAGE SQL STABLE STRICT AS $fn$
SELECT relid, parentrelid, pg_partition_root(relid) FROM pg_partition_tree(tab);
$fn$;

DROP FUNCTION IF EXISTS constraints_and_indices();
CREATE FUNCTION constraints_and_indices() RETURNS TABLE(table_name regclass, constraint_name name, index_name regclass, constraint_type char, connspoid OID)
  LANGUAGE SQL STABLE STRICT AS $fn$
    SELECT
      con.conrelid::regclass,
      con.conname,
      con.conindid::regclass,
      con.contype::char,
      con.connamespace
    FROM
        pg_constraint con
    WHERE
        con.contype != 'c'
    ORDER BY conrelid
  ;
$fn$;

--displays all dependencies and their types
DROP FUNCTION IF EXISTS dependencies();
CREATE FUNCTION dependencies() RETURNS TABLE( depname NAME, classtype "char", depnsoid OID,
                                              refname NAME, refclasstype "char", refnsoid OID,
                                              classid REGCLASS,  objid OID, objsubid INTEGER,
                                              refclassid REGCLASS, refobjid OID, refobjsubid OID, deptype "char" )
  LANGUAGE SQL STABLE STRICT AS $fn$
WITH RECURSIVE
  w AS (
    SELECT classid::regclass,
           objid,
           objsubid,
           refclassid::regclass,
           refobjid,
           refobjsubid,
           deptype
    FROM pg_depend d
    WHERE classid IN ('pg_constraint'::regclass, 'pg_class'::regclass)
      AND (objid > 16384 OR refobjid > 16384)

    UNION

    SELECT d2.*
    FROM w
           INNER JOIN pg_depend d2
                      ON (w.refclassid, w.refobjid, w.refobjsubid) =
                         (d2.classid, d2.objid, d2.objsubid)
  )
SELECT COALESCE(con.conname, c.relname, t.typname, nsp.nspname)     AS depname,
       COALESCE(con.contype, c.relkind, '-') as classtype,
       COALESCE(con.connamespace, c.relnamespace, t.typnamespace, nsp.oid) as depnsoid,
       COALESCE(con2.conname, c2.relname, t2.typname, nsp2.nspname) AS refname,
       COALESCE(con2.contype, c2.relkind, '-') as refclasstype,
       COALESCE(con2.connamespace, c2.relnamespace, t2.typnamespace, nsp2.oid) as refnsoid,
       w.*
FROM w
       LEFT JOIN pg_constraint con
                 ON classid = 'pg_constraint'::regclass AND objid = con.oid
       LEFT JOIN pg_class c ON classid = 'pg_class'::regclass AND objid = c.oid
       LEFT JOIN pg_type t ON classid = 'pg_type'::regclass AND objid = t.oid
       LEFT JOIN pg_namespace nsp
                 ON classid = 'pg_namespace'::regclass AND objid = nsp.oid

       LEFT JOIN pg_constraint con2
                 ON refclassid = 'pg_constraint'::regclass AND
                    refobjid = con2.oid
       LEFT JOIN pg_class c2
                 ON refclassid = 'pg_class'::regclass AND refobjid = c2.oid AND c2.relkind NOT IN ('p', 'I')
       LEFT JOIN pg_type t2
                 ON refclassid = 'pg_type'::regclass AND refobjid = t2.oid
       LEFT JOIN pg_namespace nsp2 ON refclassid = 'pg_namespace'::regclass AND
                                      refobjid = nsp2.oid
  ;
$fn$;

--dependencies() table but with namespace name instead of namespace oid
DROP FUNCTION IF EXISTS dependencies_nsname();
CREATE FUNCTION dependencies_nsname() RETURNS TABLE(depname NAME, classtype "char", depnsname NAME,
                                                                  refname NAME, refclasstype "char", refnsname NAME,
                                                                  classid REGCLASS, objsubid INTEGER,
                                                                  refclassid REGCLASS, refobjsubid OID, deptype "char")
  LANGUAGE SQL STABLE STRICT AS $fn$
SELECT t.depname, t.classtype, nsp.nspname as depnsame, t.refname, t.refclasstype, 
       nsp2.nspname as refnsname, t.classid, t.objsubid, t.refclassid, t.refobjsubid, t.deptype
FROM
  (SELECT depname, classtype, depnsoid, refname, refclasstype, refnsoid, 
          classid, objsubid, refclassid, refobjsubid, deptype
   FROM dependencies()
   WHERE
      (refclassid='pg_class'::REGCLASS OR refclassid='pg_constraint'::REGCLASS) AND (classtype!='c')
      AND (NOT (classtype='r' AND classid='pg_class'::REGCLASS AND refclasstype='r' AND refclassid='pg_class'::REGCLASS))
  ) as t
  JOIN pg_namespace nsp ON t.depnsoid=nsp.oid
  JOIN pg_namespace nsp2 ON t.refnsoid=nsp2.oid;
$fn$;

DROP FUNCTION IF EXISTS partition_tables_show_all(text);
CREATE FUNCTION partition_tables_show_all(tab text) RETURNS TABLE(partition_name regclass, parent_name regclass, root_name regclass, constraint_name name, index_name name, constraint_type char)
--connspname NAME) --, deptype "char")
  LANGUAGE SQL STABLE STRICT AS $fn$
  SELECT p.partition_name, p.parent_name, p.root_name, c.constraint_name, c.index_name, c.constraint_type --nsp.nspname --, d.deptype
    FROM 
         (SELECT partition_name, parent_name, root_name FROM partition_tables(tab)) as p
    LEFT JOIN 
         (SELECT table_name, constraint_name, CAST(index_name as name), constraint_type, connspoid from constraints_and_indices()) as c
    ON p.partition_name::name = CAST(c.table_name as name)
    JOIN pg_class as pgc ON p.root_name::name=pgc.relname     --find root_name's namespace name
    JOIN pg_namespace as nsp
    ON  pgc.relnamespace=nsp.oid WHERE nsp.nspname='index_constraint_naming_partition'
    ORDER BY p.partition_name::name, p.parent_name::name, p.root_name::name, c.constraint_name, c.index_name, c.constraint_type;  --make sure output is stabely ordered for pg_regress

$fn$;

DROP FUNCTION IF EXISTS dependencies_show_for_idx();
CREATE FUNCTION dependencies_show_for_idx() RETURNS TABLE(depname name, refname name, depType "char")
  LANGUAGE SQL STABLE STRICT AS $fn$
  SELECT t.depname, t.refname, t.deptype FROM
        (SELECT depname, depnsname, refname, refnsname, deptype 
          FROM dependencies_nsname() 
            WHERE 
                ((depname LIKE '%idx%') OR (refname LIKE '%idx%')) 
                AND (depnsname='index_constraint_naming_partition' AND refnsname='index_constraint_naming_partition')) as t
       ORDER BY t.depname, t.refname, t.deptype;   --make sure output is stabely ordered for pg_regress
$fn$;

--return all dependencies of interest here...
DROP FUNCTION IF EXISTS dependencies_show_for_cons_idx();
CREATE FUNCTION dependencies_show_for_cons_idx() RETURNS TABLE(depname NAME, classtype "char", 
                                                               refname NAME, refclasstype "char", classid REGCLASS, objsubid INTEGER,
                                                               refclassid REGCLASS, refobjsubid OID, deptype "char")
  LANGUAGE SQL STABLE STRICT AS $fn$
  SELECT t.depname, t.classtype, t.refname, t.refclasstype, t.classid, t.objsubid, t.refclassid, t.refobjsubid, t.deptype FROM
      (SELECT depname, classtype, depnsname, refname, refclasstype, refnsname, classid, objsubid, refclassid, refobjsubid, deptype FROM dependencies_nsname() WHERE
    (refclassid='pg_class'::REGCLASS OR refclassid='pg_constraint'::REGCLASS) AND (classtype!='c') 
    AND (NOT (classtype='r' AND classid='pg_class'::REGCLASS AND refclasstype='r' AND refclassid='pg_class'::REGCLASS)) 
    AND (depnsname='index_constraint_naming_partition' AND refnsname='index_constraint_naming_partition')) as t
  ORDER BY t.classtype, t.refclasstype, t.deptype, t.depname, t.refname, t.classid, t.objsubid, t.refclassid, t.refobjsubid;   --make sure output is stabely ordered for pg_regress
$fn$;

--return all dependencies of interest here...

--############################ TWO LEVEL ##################################

DROP FUNCTION IF EXISTS recreate_two_level_table();
CREATE FUNCTION recreate_two_level_table() RETURNS VOID
  LANGUAGE SQL VOLATILE STRICT AS $fn$
    DROP TABLE IF EXISTS r;
    CREATE TABLE r (
        r_key INTEGER NOT NULL,
        r_name CHAR(25)
    )
    DISTRIBUTED BY (r_key)
    PARTITION BY range (r_key)
    (
        PARTITION r1 START (0),
        PARTITION r2 START (5) END (8)
    );
$fn$;

-- validate that there are no constraints when we start
SELECT * FROM partition_tables_show_all('r');

-- UNIQUE constraint: validate we correctly add it and can only drop from root
SELECT recreate_two_level_table();
ALTER TABLE r ADD UNIQUE(r_key,r_name);
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
INSERT INTO r VALUES (1,'xxxx'),(1,'xxxx'); --should be prevented by constraint
INSERT INTO r VALUES (6,'xxxx'),(6,'xxxx'); --should be prevented by constraint

ALTER TABLE r ADD PARTITION added_part START(8) INCLUSIVE END (10) EXCLUSIVE;
SELECT * FROM dependencies_show_for_cons_idx();
INSERT INTO r VALUES (9,'xxxx'),(9,'xxxx'); --should be prevented by constraint

SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r_1_prt_r1 DROP CONSTRAINT r_1_prt_r1_r_key_r_name_key; --should fail
INSERT INTO r VALUES (1,'xxxx'),(1,'xxxx'); --should be prevented by constraint
INSERT INTO r VALUES (6,'xxxx'),(6,'xxxx'); --should be prevented by constraint

SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r ADD DEFAULT PARTITION d;
SELECT * FROM dependencies_show_for_cons_idx();
INSERT INTO r VALUES (22,'xxxx'),(22,'xxxx'); --should be prevented by constraint

SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r SPLIT PARTITION r2 AT (6) INTO (PARTITION r2_split_l, PARTITION r2_split_r);
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
INSERT INTO r VALUES (5,'xxxx'),(5,'xxxx'); --should be prevented by constraint
INSERT INTO r VALUES (7,'xxxx'),(7,'xxxx'); --should be prevented by constraint


alter table r rename partition r1 to r1_renamed;
SELECT * FROM partition_tables_show_all('r');
INSERT INTO r VALUES (1,'xxxx'),(1,'xxxx'); --should be prevented by constraint

ALTER TABLE r DROP PARTITION r1_renamed;
SELECT * FROM dependencies_show_for_cons_idx();
INSERT INTO r VALUES (1,'xxxx'),(1,'xxxx'); --should be prevented by constraint

SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r DROP CONSTRAINT r_r_key_r_name_key;                   --should work
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
INSERT INTO r VALUES (1,'xxxx'),(1,'xxxx');     --should be allowed: no unique constraint exists
INSERT INTO r VALUES (6,'xxxx'),(6,'xxxx');     --should be allowed: no unique constraint exists
INSERT INTO r VALUES (9,'xxxx'),(9,'xxxx');     --should be allowed: no unique constraint exists
INSERT INTO r VALUES (22,'xxxx'),(22,'xxxx');   --should be allowed: no unique constraint exists


-- EXCHANGE... prepare table e and swap with partition
SELECT recreate_two_level_table();
ALTER TABLE r ADD UNIQUE(r_key,r_name);
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
CREATE TABLE e (LIKE r INCLUDING CONSTRAINTS INCLUDING INDEXES);
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r EXCHANGE PARTITION r1 WITH TABLE e;
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
--MAKE SURE PARTITION CONSTRAINT EXISTS...

-- UNIQUE create constraint inside CREATE TABLE DDL...this is different
DROP TABLE IF EXISTS r;
CREATE TABLE r (r_key INTEGER NOT NULL,UNIQUE(r_key)) DISTRIBUTED BY (r_key)
PARTITION BY range (r_key) (PARTITION r1 START (0), PARTITION r2 START (5) END (8));
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
INSERT INTO r VALUES (1),(1);
INSERT INTO r VALUES (6),(6);

SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
INSERT INTO r VALUES (1),(1);
INSERT INTO r VALUES (6),(6);

--PRIMARY KEY constraint: validate we correctly add it and can only drop from root
SELECT recreate_two_level_table();
ALTER TABLE r ADD PRIMARY KEY(r_key,r_name);
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r_1_prt_r1 DROP CONSTRAINT r_1_prt_r1_pkey; --should fail
SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r DROP CONSTRAINT r_pkey;                   --should work
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');

--EXCLUSION constraint: validate we cannot create an exclusion constraint on a
--partition table. These are disallowed in GPDB.
SELECT recreate_two_level_table();
ALTER TABLE r ADD EXCLUDE (r_key WITH =,r_name WITH =);
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r_1_prt_r1 DROP CONSTRAINT r_1_prt_r1_r_key_r_name_excl;   --should fail
SELECT * FROM partition_tables_show_all('r');

--new partition table (4) should be created with constraints.
DROP TABLE IF EXISTS r;
CREATE TABLE r (
    r_key INTEGER NOT NULL,
    r_name CHAR(25)
)
DISTRIBUTED BY (r_key)
PARTITION BY list (r_key)
(
    PARTITION r1 VALUES (0),
    PARTITION r2 VALUES (1)
);
ALTER TABLE r ADD UNIQUE(r_key,r_name);
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r ADD PARTITION r3 VALUES(2);
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');

-- PRIMARY create constraint inside CREATE TABLE DDL...this is different
DROP TABLE IF EXISTS r;
CREATE TABLE r (r_key INTEGER NOT NULL,PRIMARY KEY (r_key)) DISTRIBUTED BY (r_key)
  PARTITION BY range (r_key) (PARTITION r1 START (0), PARTITION r2 START (5) END (8));
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r DROP CONSTRAINT r_pkey;                   --should work
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');


DROP TABLE r;
-- --########### THREE LEVEL ###############################
DROP FUNCTION IF EXISTS recreate_three_level_table();
CREATE FUNCTION recreate_three_level_table() RETURNS VOID
  LANGUAGE SQL VOLATILE STRICT AS $fn$
    DROP TABLE IF EXISTS r;
    CREATE TABLE r (
        r_key INTEGER NOT NULL,
        r_name CHAR(25)
    )
    DISTRIBUTED BY (r_key)
    PARTITION BY range (r_key)
    SUBPARTITION BY list (r_name) SUBPARTITION TEMPLATE
    (
        SUBPARTITION a VALUES ('A'),
        SUBPARTITION b VALUES ('B')
    )
    (
        PARTITION r1 START (0),
        PARTITION r2 START (5) END (8)
    );
$fn$;

-- validate that there are no constraints when we start
SELECT * FROM partition_tables_show_all('r');

-- UNIQUE constraint: validate we correctly add it and can only drop from root
SELECT recreate_three_level_table();
ALTER TABLE r ADD UNIQUE(r_key,r_name);
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r_1_prt_r1 DROP CONSTRAINT r_1_prt_r1_r_key_r_name_key1;                --should fail
SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r_1_prt_r1_2_prt_a DROP CONSTRAINT r_1_prt_r1_2_prt_a_r_key_r_name_key; --should fail
SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r DROP CONSTRAINT r_r_key_r_name_key;                                   --should work
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');

--PRIMARY KEY constraint: validate we correctly add it and can only drop from root
SELECT recreate_three_level_table();
ALTER TABLE r ADD PRIMARY KEY(r_key,r_name);
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r_1_prt_r1 DROP CONSTRAINT r_1_prt_r1_pkey;                --should fail
SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r_1_prt_r1_2_prt_a DROP CONSTRAINT r_1_prt_r1_2_prt_a_pkey; --should fail
SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r DROP CONSTRAINT r_pkey;                     --should work
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');

--New partition table 'C' should be created with constraints.
SELECT recreate_three_level_table();
ALTER TABLE r ADD UNIQUE(r_key,r_name);
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r ALTER PARTITION r1 ADD PARTITION c VALUES ('C');
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');

-- EXCHANGE... prepare table e2 and swap with partition
SELECT recreate_three_level_table();
ALTER TABLE r ADD UNIQUE(r_key,r_name);
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
CREATE TABLE e2 (LIKE r INCLUDING CONSTRAINTS INCLUDING INDEXES);
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r ALTER PARTITION r1 EXCHANGE PARTITION a WITH TABLE e2;
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');

-- UNIQUE: create constraint inside CREATE TABLE DDL...this is different
DROP TABLE IF EXISTS r;
CREATE TABLE r (r_key INTEGER NOT NULL,r_name CHAR(25), UNIQUE(r_key, r_name))
  DISTRIBUTED BY (r_key) PARTITION BY range (r_key)
  SUBPARTITION BY list (r_name) SUBPARTITION TEMPLATE
  (SUBPARTITION a VALUES ('A'),SUBPARTITION b VALUES ('B'))
  (PARTITION r1 START (0),PARTITION r2 START (5) END (8));
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r DROP CONSTRAINT r_r_key_r_name_key;
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');

-- PRIMARY: create constraint inside CREATE TABLE DDL...this is different
DROP TABLE IF EXISTS r;
CREATE TABLE r (r_key INTEGER NOT NULL,r_name CHAR(25), PRIMARY KEY(r_key, r_name))
  DISTRIBUTED BY (r_key) PARTITION BY range (r_key)
  SUBPARTITION BY list (r_name) SUBPARTITION TEMPLATE
(SUBPARTITION a VALUES ('A'),SUBPARTITION b VALUES ('B'))
(PARTITION r1 START (0),PARTITION r2 START (5) END (8));
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');
ALTER TABLE r DROP CONSTRAINT r_pkey;
SELECT * FROM dependencies_show_for_cons_idx();
SELECT * FROM partition_tables_show_all('r');

-- PARTITIONED INDEX TESTS--------------
-- UNIQUE index: validate we can add a unique index to any node in the partition
--  hierarchy and that that index is added to all descendents.  Furthermore, no
--  descendent can remove that index; it can only be removed from the creating
--  node.
--create the index at the root; should only be able to drop it from root
SELECT recreate_three_level_table();
CREATE UNIQUE INDEX myidx_onroot ON r USING btree(r_key,r_name);
SELECT * FROM dependencies_show_for_idx();
DROP INDEX r_1_prt_r2_r_key_r_name_idx;         --should fail: cannot drop from non-creating node
SELECT * FROM dependencies_show_for_idx();
DROP INDEX r_1_prt_r2_2_prt_b_r_key_r_name_idx; --should fail: cannot drop from non-creating node
SELECT * FROM dependencies_show_for_idx();
DROP INDEX myidx_onroot;                 --works: is root of index hierarchy
SELECT * FROM dependencies_show_for_idx();

--create the index starting at mid-level; should only be able to drop it from mid-level
SELECT recreate_three_level_table();
CREATE UNIQUE INDEX myidx_midlevel ON r_1_prt_r1 USING btree(r_key, r_name);
SELECT * FROM dependencies_show_for_idx();
DROP INDEX r_1_prt_r1_2_prt_a_r_key_r_name_idx;  --should fail: cannot drop from non-creating node
SELECT * FROM dependencies_show_for_idx();
DROP INDEX myidx_midlevel;                --works: is root of index hierarchy
SELECT * FROM dependencies_show_for_idx();

--we can create more than one index on a table(same as upstream 11)
--yes, this is inconsistent with index subsumption but upstream does it this way
--in particular, there's no subsumption going on here.
SELECT recreate_three_level_table();
CREATE UNIQUE INDEX ON r USING btree(r_key,r_name);
SELECT * FROM dependencies_show_for_idx();
CREATE UNIQUE INDEX ON r USING btree(r_key,r_name);
SELECT * FROM dependencies_show_for_idx();

--test subsumption of lower-level index from upper-level one
SELECT recreate_three_level_table();
CREATE UNIQUE INDEX myidx_midlevel ON r_1_prt_r1 USING btree(r_key, r_name);
SELECT * FROM dependencies_show_for_idx();
CREATE UNIQUE INDEX myidx_onroot ON r USING btree(r_key,r_name);
SELECT * FROM dependencies_show_for_idx();

--show subsumption works across rename
SELECT recreate_three_level_table();
CREATE UNIQUE INDEX myidx_midlevel ON r_1_prt_r1 USING btree(r_key, r_name);
SELECT * FROM dependencies_show_for_idx();
ALTER INDEX myidx_midlevel RENAME TO myidx_midlevel_renamed;
SELECT * FROM dependencies_show_for_idx();
CREATE UNIQUE INDEX myidx_midlevel ON r USING btree(r_key,r_name);
SELECT * FROM dependencies_show_for_idx();

--show that renaming still does not allow a name conflict
SELECT recreate_three_level_table();
CREATE UNIQUE INDEX myidx_midlevel ON r_1_prt_r1 USING btree(r_key, r_name);
SELECT * FROM dependencies_show_for_idx();
ALTER INDEX myidx_midlevel RENAME TO myidx_midlevel_renamed;
SELECT * FROM dependencies_show_for_idx();
CREATE UNIQUE INDEX myidx_midlevel_renamed ON r USING btree(r_key,r_name);   --should fail: index already exists
SELECT * FROM dependencies_show_for_idx();

--show that renaming a mid-level index still prevents it from being dropped.
SELECT recreate_three_level_table();
CREATE UNIQUE INDEX myidx_onroot ON r USING btree(r_key,r_name);
SELECT * FROM dependencies_show_for_idx();
ALTER INDEX r_1_prt_r1_r_key_r_name_idx RENAME TO middle_index_renamed;
SELECT * FROM dependencies_show_for_idx();
DROP INDEX middle_index_renamed;  --should fail: this renamed index is still controlled by the root index that created it
SELECT * FROM dependencies_show_for_idx();


DROP TABLE IF EXISTS r;
