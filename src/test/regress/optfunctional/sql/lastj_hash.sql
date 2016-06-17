
-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------

-- start_ignore
create schema lastj_hash;
set search_path to lastj_hash;
drop table if exists foo;
drop table if exists bar;

create table foo (a text, b int);
create table bar (x int, y int);

insert into foo values (1, 2);
insert into foo values (12, 20);
insert into foo values (NULL, 2);
insert into foo values (15, 2);
insert into foo values (NULL, NULL);
insert into foo values (1, 12);
insert into foo values (1, 102);

insert into bar select i/10, i from generate_series(1, 100)i;
insert into bar values (NULL, 101);
insert into bar values (NULL, 102);
insert into bar values (NULL, NULL);
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: sql/lasj_hash_all_1.sql
-- ----------------------------------------------------------------------

-- @author ramans2
-- @created 2013-07-05 12:00:00 
-- @modified 2013-07-05 12:00:00
-- @tags lasj HAWQ
-- @db_name lasjdb
-- @product_version gpdb: [4.3.0.0-9.9.99.99] , hawq: [1.1.4.0-9.9.99.99]
-- @description OPT-3351: Add test cases for LASJ

-- enable hash join
SELECT enable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- empty outer, non-empty inner
SELECT * FROM foo WHERE b = -1 AND a = ALL (SELECT x FROM bar WHERE y <= 100);

-- ----------------------------------------------------------------------
-- Test: sql/lasj_hash_all_2.sql
-- ----------------------------------------------------------------------

-- @author ramans2
-- @created 2013-07-05 12:00:00 
-- @modified 2013-07-05 12:00:00
-- @tags lasj HAWQ
-- @db_name lasjdb
-- @product_version gpdb: [4.3.0.0-9.9.99.99] , hawq: [1.1.4.0-9.9.99.99]
-- @description OPT-3351: Add test cases for LASJ

-- enable hash join
SELECT enable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- outer with nulls, non-empty inner
SELECT * FROM foo WHERE b = 2 AND a = ALL (SELECT x FROM bar WHERE y >=10 AND y < 20);

-- ----------------------------------------------------------------------
-- Test: sql/lasj_hash_all_3.sql
-- ----------------------------------------------------------------------

-- @author ramans2
-- @created 2013-07-05 12:00:00 
-- @modified 2013-07-05 12:00:00
-- @tags lasj HAWQ
-- @db_name lasjdb
-- @product_version gpdb: [4.3.0.0-9.9.99.99] , hawq: [1.1.4.0-9.9.99.99]
-- @description OPT-3351: Add test cases for LASJ

-- enable hash join
SELECT enable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- outer with nulls, empty inner
SELECT * FROM foo WHERE b = 2 AND a = ALL (SELECT x FROM bar WHERE y = -1) order by 1, 2;

-- ----------------------------------------------------------------------
-- Test: sql/lasj_hash_all_4.sql
-- ----------------------------------------------------------------------

-- @author ramans2
-- @created 2013-07-05 12:00:00 
-- @modified 2013-07-05 12:00:00
-- @tags lasj HAWQ
-- @db_name lasjdb
-- @product_version gpdb: [4.3.0.0-9.9.99.99] , hawq: [1.1.4.0-9.9.99.99]
-- @description OPT-3351: Add test cases for LASJ

-- enable hash join
SELECT enable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- outer with nulls, inner with nulls
SELECT * FROM foo WHERE a = ALL (SELECT x FROM bar WHERE x = 1 OR x IS NULL);

-- ----------------------------------------------------------------------
-- Test: sql/lasj_hash_full_1.sql
-- ----------------------------------------------------------------------

-- @author ramans2
-- @created 2013-07-05 12:00:00 
-- @modified 2013-07-05 12:00:00
-- @tags lasj HAWQ
-- @db_name lasjdb
-- @product_version gpdb: [4.3.0.0-9.9.99.99] , hawq: [1.1.4.0-9.9.99.99]
-- @description OPT-3351: Add test cases for LASJ

-- enable hash join
SELECT enable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- empty outer, non-empty inner
SELECT * FROM (SELECT * FROM foo WHERE b = -1) foo2 FULL OUTER JOIN bar ON (a = x);

-- ----------------------------------------------------------------------
-- Test: sql/lasj_hash_full_2.sql
-- ----------------------------------------------------------------------

-- @author ramans2
-- @created 2013-07-05 12:00:00 
-- @modified 2013-07-05 12:00:00
-- @tags lasj HAWQ
-- @db_name lasjdb
-- @product_version gpdb: [4.3.0.0-9.9.99.99] , hawq: [1.1.4.0-9.9.99.99]
-- @description OPT-3351: Add test cases for LASJ

-- enable hash join
SELECT enable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- non-empty outer, empty inner
SELECT * FROM foo FULL OUTER JOIN (SELECT * FROM bar WHERE y = -1) bar2 ON (a = x);

-- ----------------------------------------------------------------------
-- Test: sql/lasj_hash_full_3.sql
-- ----------------------------------------------------------------------

-- @author ramans2
-- @created 2013-07-05 12:00:00 
-- @modified 2013-07-05 12:00:00
-- @tags lasj HAWQ
-- @db_name lasjdb
-- @product_version gpdb: [4.3.0.0-9.9.99.99] , hawq: [1.1.4.0-9.9.99.99]
-- @description OPT-3351: Add test cases for LASJ

-- enable hash join
SELECT enable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- non-empty outer, non-empty inner
SELECT * FROM (SELECT * FROM foo WHERE b = 2) foo2 FULL OUTER JOIN (SELECT * FROM bar WHERE y BETWEEN 16 AND 22 OR x IS NULL) bar2 ON (a = x);

-- ----------------------------------------------------------------------
-- Test: sql/lasj_hash_notin_multiple_1.sql
-- ----------------------------------------------------------------------

-- @author ramans2
-- @created 2013-07-05 12:00:00 
-- @modified 2013-07-05 12:00:00
-- @tags lasj HAWQ
-- @db_name lasjdb
-- @product_version gpdb: [4.3.0.0-9.9.99.99] , hawq: [1.1.4.0-9.9.99.99]
-- @description OPT-3351: Add test cases for LASJ

-- enable hash join
SELECT enable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- empty outer, non-empty inner
SELECT * FROM foo WHERE b = -1 AND (a, b) NOT IN (SELECT x, y FROM bar WHERE y <= 100);

-- ----------------------------------------------------------------------
-- Test: sql/lasj_hash_notin_multiple_2.sql
-- ----------------------------------------------------------------------

-- @author ramans2
-- @created 2013-07-05 12:00:00 
-- @modified 2013-07-05 12:00:00
-- @tags lasj HAWQ
-- @db_name lasjdb
-- @product_version gpdb: [4.3.0.0-9.9.99.99] , hawq: [1.1.4.0-9.9.99.99]
-- @description OPT-3351: Add test cases for LASJ

-- enable hash join
SELECT enable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- outer with nulls, non-empty inner
SELECT * FROM foo WHERE (a, b) NOT IN (SELECT x, y FROM bar WHERE y <= 100);

-- ----------------------------------------------------------------------
-- Test: sql/lasj_hash_notin_multiple_3.sql
-- ----------------------------------------------------------------------

-- @author ramans2
-- @created 2013-07-05 12:00:00 
-- @modified 2013-07-05 12:00:00
-- @tags lasj HAWQ
-- @db_name lasjdb
-- @product_version gpdb: [4.3.0.0-9.9.99.99] , hawq: [1.1.4.0-9.9.99.99]
-- @description OPT-3351: Add test cases for LASJ

-- enable hash join
SELECT enable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- outer with nulls, empty inner
SELECT * FROM foo WHERE (a, b) NOT IN (SELECT x, y FROM bar WHERE y = -1);

-- ----------------------------------------------------------------------
-- Test: sql/lasj_hash_notin_multiple_4.sql
-- ----------------------------------------------------------------------

-- @author ramans2
-- @created 2013-07-05 12:00:00 
-- @modified 2013-07-05 12:00:00
-- @tags lasj HAWQ
-- @db_name lasjdb
-- @product_version gpdb: [4.3.0.0-9.9.99.99] , hawq: [1.1.4.0-9.9.99.99]
-- @description OPT-3351: Add test cases for LASJ

-- enable hash join
SELECT enable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- outer with nulls, inner with partial nulls
SELECT * FROM foo WHERE (a, b) NOT IN (SELECT x, y FROM bar WHERE y IS NOT NULL);

-- ----------------------------------------------------------------------
-- Test: sql/lasj_hash_notin_multiple_5.sql
-- ----------------------------------------------------------------------

-- @author ramans2
-- @created 2013-07-05 12:00:00 
-- @modified 2013-07-05 12:00:00
-- @tags lasj HAWQ
-- @db_name lasjdb
-- @product_version gpdb: [4.3.0.0-9.9.99.99] , hawq: [1.1.4.0-9.9.99.99]
-- @description OPT-3351: Add test cases for LASJ

-- enable hash join
SELECT enable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- outer with nulls, inner with null tuples
SELECT * FROM foo WHERE (a, b) NOT IN (SELECT x, y FROM bar);

-- ----------------------------------------------------------------------
-- Test: sql/lasj_hash_notin_single_1.sql
-- ----------------------------------------------------------------------

-- @author ramans2
-- @created 2013-07-05 12:00:00 
-- @modified 2013-07-05 12:00:00
-- @tags lasj HAWQ
-- @db_name lasjdb
-- @product_version gpdb: [4.3.0.0-9.9.99.99] , hawq: [1.1.4.0-9.9.99.99]
-- @description OPT-3351: Add test cases for LASJ

-- enable hash join
SELECT enable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- empty outer, non-empty inner
SELECT * FROM foo WHERE b = -1 AND a NOT IN (SELECT x FROM bar WHERE y <= 100);

-- ----------------------------------------------------------------------
-- Test: sql/lasj_hash_notin_single_2.sql
-- ----------------------------------------------------------------------

-- @author ramans2
-- @created 2013-07-05 12:00:00 
-- @modified 2013-07-05 12:00:00
-- @tags lasj HAWQ
-- @db_name lasjdb
-- @product_version gpdb: [4.3.0.0-9.9.99.99] , hawq: [1.1.4.0-9.9.99.99]
-- @description OPT-3351: Add test cases for LASJ

-- enable hash join
SELECT enable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- outer with nulls, non-empty inner
SELECT * FROM foo WHERE b = 2 AND a NOT IN (SELECT x FROM bar WHERE y <= 100);

-- ----------------------------------------------------------------------
-- Test: sql/lasj_hash_notin_single_3.sql
-- ----------------------------------------------------------------------

-- @author ramans2
-- @created 2013-07-05 12:00:00 
-- @modified 2013-07-05 12:00:00
-- @tags lasj HAWQ
-- @db_name lasjdb
-- @product_version gpdb: [4.3.0.0-9.9.99.99] , hawq: [1.1.4.0-9.9.99.99]
-- @description OPT-3351: Add test cases for LASJ

-- enable hash join
SELECT enable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- outer with nulls, empty inner
SELECT * FROM foo WHERE b = 2 AND a NOT IN (SELECT x FROM bar WHERE y = -1) order by 1, 2;

-- ----------------------------------------------------------------------
-- Test: sql/lasj_hash_notin_single_4.sql
-- ----------------------------------------------------------------------

-- @author ramans2
-- @created 2013-07-05 12:00:00 
-- @modified 2013-07-05 12:00:00
-- @tags lasj HAWQ
-- @db_name lasjdb
-- @product_version gpdb: [4.3.0.0-9.9.99.99] , hawq: [1.1.4.0-9.9.99.99]
-- @description OPT-3351: Add test cases for LASJ

-- enable hash join
SELECT enable_xform('CXformLeftAntiSemiJoin2HashJoin');

-- outer with nulls, inner with nulls
SELECT * FROM foo WHERE a NOT IN (SELECT x FROM bar);

-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------

-- start_ignore
drop schema lastj_hash cascade;
-- end_ignore
