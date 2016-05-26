
-- start_ignore
create schema functionProperty_1601_1700;
set search_path to functionProperty_1601_1700;

CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;

CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;

-- func1 IMMUTABLE
CREATE FUNCTION func1_nosql_imm(x int) RETURNS int AS $$
BEGIN
RETURN $1 +1;
END
$$ LANGUAGE plpgsql NO SQL IMMUTABLE;

CREATE FUNCTION func1_sql_int_imm(x int) RETURNS int AS $$
DECLARE
    r int;
BEGIN
    SELECT $1 + 1 INTO r;
    return r;
END
$$ LANGUAGE plpgsql CONTAINS SQL IMMUTABLE;

CREATE FUNCTION func1_sql_setint_imm(x int) RETURNS setof int AS $$
DECLARE
    r int;
BEGIN
    FOR r in SELECT generate_series($1, $1+5)
    LOOP
        RETURN NEXT r;
    END LOOP;
    RETURN;
END
$$ LANGUAGE plpgsql CONTAINS SQL IMMUTABLE;

--CREATE FUNCTION func1_read_int_sql_imm(x int) RETURNS int AS $$
--DECLARE
--    r int;
--BEGIN
--    SELECT d FROM bar WHERE c = $1 LIMIT 1 INTO r;
--    return r;
--END
--$$ LANGUAGE plpgsql IMMUTABLE READS SQL DATA;
--
--CREATE FUNCTION func1_read_setint_sql_imm(x int) RETURNS setof int AS $$
--DECLARE
--    r int;
--BEGIN
--    FOR r in SELECT d FROM bar WHERE c <> $1
--    LOOP
--        RETURN NEXT r;
--    END LOOP;
--    RETURN;
--END
--$$ LANGUAGE plpgsql IMMUTABLE READS SQL DATA;
--
--CREATE FUNCTION func1_mod_int_imm(x int) RETURNS int AS $$
--BEGIN
--UPDATE bar SET d = d+1 WHERE c = $1;
--RETURN $1 + 1;
--END
--$$ LANGUAGE plpgsql IMMUTABLE MODIFIES SQL DATA;
--
--CREATE FUNCTION func1_mod_setint_imm(x int) RETURNS setof int AS $$
--DECLARE
--    r int;
--BEGIN
--    UPDATE bar SET d = d+1 WHERE c > $1;
--    FOR r in SELECT d FROM bar WHERE c > $1 
--    LOOP
--        RETURN NEXT r;
--    END LOOP;
--    RETURN;
--END
--$$ LANGUAGE plpgsql MODIFIES SQL DATA IMMUTABLE;
--
----func2 IMMUTABLE

CREATE FUNCTION func2_nosql_imm(x int) RETURNS int AS $$
BEGIN 
RETURN $1 + 1; 
END
$$ LANGUAGE plpgsql NO SQL IMMUTABLE;

CREATE FUNCTION func2_sql_int_imm(x int) RETURNS int AS $$
DECLARE
    r int;
BEGIN
    SELECT $1 + 1 INTO r;
    RETURN r;
END
$$ LANGUAGE plpgsql CONTAINS SQL IMMUTABLE;
--
--CREATE FUNCTION func2_read_int_imm(x int) RETURNS int AS $$
--DECLARE
--    r int;
--BEGIN
--    SELECT d FROM bar WHERE c = $1 LIMIT 1 INTO r;
--    RETURN r;
--END
--$$ LANGUAGE plpgsql IMMUTABLE READS SQL DATA;
--
--CREATE FUNCTION func2_mod_int_imm(x int) RETURNS int AS $$
--BEGIN
--UPDATE bar SET d = d+1 WHERE c = $1;
--RETURN $1 + 1;
--END
--$$ LANGUAGE plpgsql IMMUTABLE MODIFIES SQL DATA;



-- func1 STABLE
CREATE FUNCTION func1_nosql_stb(x int) RETURNS int AS $$
BEGIN
RETURN $1 +1;
END
$$ LANGUAGE plpgsql STABLE NO SQL;

CREATE FUNCTION func1_sql_int_stb(x int) RETURNS int AS $$
DECLARE
    r int;
BEGIN
    SELECT $1 + 1 INTO r;
    RETURN r;
END
$$ LANGUAGE plpgsql STABLE CONTAINS SQL;

CREATE FUNCTION func1_sql_setint_stb(x int) RETURNS setof int AS $$
DECLARE
    r int;
BEGIN
    FOR r in SELECT generate_series($1, $1+5)
    LOOP
        RETURN NEXT r;
    END LOOP;
    RETURN;
END
$$ LANGUAGE plpgsql STABLE CONTAINS SQL;

CREATE FUNCTION func1_read_int_sql_stb(x int) RETURNS int AS $$
DECLARE
    r int;
BEGIN
    SELECT d FROM bar WHERE c = $1 LIMIT 1 INTO r;
    RETURN r;
END
$$ LANGUAGE plpgsql STABLE READS SQL DATA;

CREATE FUNCTION func1_read_setint_sql_stb(x int) RETURNS setof int AS $$
DECLARE
    r int;
BEGIN
    FOR r in SELECT d FROM bar WHERE c <> $1
    LOOP
        RETURN NEXT r;
    END LOOP;
    RETURN;
END
$$ LANGUAGE plpgsql STABLE READS SQL DATA;

CREATE FUNCTION func1_mod_int_stb(x int) RETURNS int AS $$
BEGIN
UPDATE bar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql STABLE MODIFIES SQL DATA;

CREATE FUNCTION func1_mod_setint_stb(x int) RETURNS setof int AS $$
DECLARE
    r int;
BEGIN
UPDATE bar SET d = d+1 WHERE c > $1;
    FOR r in SELECT d FROM bar WHERE c > $1
    LOOP
        RETURN NEXT r;
    END LOOP;
    RETURN;
END
$$ LANGUAGE plpgsql STABLE MODIFIES SQL DATA;

--func2 STABLE

CREATE FUNCTION func2_nosql_stb(x int) RETURNS int AS $$
BEGIN
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql STABLE NO SQL;

CREATE FUNCTION func2_sql_int_stb(x int) RETURNS int AS $$
DECLARE
    r int;
BEGIN
    SELECT $1 + 1 INTO r;
    RETURN r;
END
$$ LANGUAGE plpgsql STABLE CONTAINS SQL;

CREATE FUNCTION func2_read_int_stb(x int) RETURNS int AS $$
DECLARE
    r int;
BEGIN
    SELECT d FROM bar WHERE c = $1 LIMIT 1 INTO r;
    RETURN r;
END
$$ LANGUAGE plpgsql STABLE READS SQL DATA;

CREATE FUNCTION func2_mod_int_stb(x int) RETURNS int AS $$
BEGIN
UPDATE bar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql STABLE MODIFIES SQL DATA;






-- func1  VOLATILE
CREATE FUNCTION func1_nosql_vol(x int) RETURNS int AS $$
BEGIN
RETURN $1 +1;
END
$$ LANGUAGE plpgsql VOLATILE NO SQL;

CREATE FUNCTION func1_sql_int_vol(x int) RETURNS int AS $$
DECLARE
    r int;
BEGIN
    SELECT $1 + 1 INTO r;
    RETURN r;
END
$$ LANGUAGE plpgsql VOLATILE CONTAINS SQL;

CREATE FUNCTION func1_sql_setint_vol(x int) RETURNS setof int AS $$
DECLARE
    r int;
BEGIN
    FOR r in SELECT generate_series($1, $1+5)
    LOOP
        RETURN NEXT r;
    END LOOP;
    RETURN;
END
$$ LANGUAGE plpgsql VOLATILE CONTAINS SQL;

CREATE FUNCTION func1_read_int_sql_vol(x int) RETURNS int AS $$
DECLARE
    r int;
BEGIN
    SELECT d FROM bar WHERE c = $1 LIMIT 1 INTO r;
    RETURN r;
END
$$ LANGUAGE plpgsql VOLATILE READS SQL DATA;

CREATE FUNCTION func1_read_setint_sql_vol(x int) RETURNS setof int AS $$
DECLARE
    r int;
BEGIN
    FOR r in SELECT d FROM bar WHERE c <> $1
    LOOP
        RETURN NEXT r;
    END LOOP;
    RETURN;
END
$$ LANGUAGE plpgsql VOLATILE READS SQL DATA;

CREATE FUNCTION func1_mod_int_vol(x int) RETURNS int AS $$
BEGIN
UPDATE bar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

CREATE FUNCTION func1_mod_setint_vol(x int) RETURNS setof int AS $$
DECLARE
    r int;
BEGIN
    UPDATE bar SET d = d+1 WHERE c > $1;
    FOR r in SELECT d FROM bar WHERE c > $1
    LOOP
        RETURN NEXT r;
    END LOOP;
    RETURN;
END
$$ LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

--func2   VOLATILE

CREATE FUNCTION func2_nosql_vol(x int) RETURNS int AS $$
BEGIN
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql VOLATILE NO SQL;

CREATE FUNCTION func2_sql_int_vol(x int) RETURNS int AS $$
DECLARE
    r int;
BEGIN
    SELECT $1 + 1 INTO r;
    RETURN r;
END
$$ LANGUAGE plpgsql VOLATILE CONTAINS SQL;

CREATE FUNCTION func2_read_int_vol(x int) RETURNS int AS $$
DECLARE
    r int;
BEGIN
    SELECT d FROM bar WHERE c = $1 LIMIT 1 INTO r;
    RETURN r;
END
$$ LANGUAGE plpgsql VOLATILE READS SQL DATA;

CREATE FUNCTION func2_mod_int_vol(x int) RETURNS int AS $$
BEGIN
UPDATE bar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;
-- end_ignore
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_79.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_setint_stb(func2_mod_int_stb(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_8.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_vol(func2_mod_int_vol(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_80.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_nosql_vol(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_81.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_nosql_stb(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_82.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_nosql_imm(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_83.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_sql_int_vol(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_84.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_sql_int_stb(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_85.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_sql_int_imm(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_86.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_read_int_vol(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_87.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_read_int_stb(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_88.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_mod_int_vol(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_89.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_setint_imm(func2_mod_int_stb(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_9.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_vol(func2_mod_int_stb(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_90.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_nosql_vol(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_91.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_nosql_stb(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_92.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_nosql_imm(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_93.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_sql_int_vol(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_94.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_sql_int_stb(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_95.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_sql_int_imm(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_96.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_read_int_vol(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_97.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_read_int_stb(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_98.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_mod_int_vol(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_withfunc2_99.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_int_sql_vol(func2_mod_int_stb(5))) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_0.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_vol(a), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_1.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_stb(a), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_11.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_read_setint_sql_vol(a), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_12.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_read_setint_sql_stb(a), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_15.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_mod_setint_vol(a), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_16.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_mod_setint_stb(a), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_2.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_imm(a), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_3.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_vol(a), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_4.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_stb(a), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_5.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_imm(a), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_6.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_setint_vol(a), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_7.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_setint_stb(a), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_8.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_setint_imm(a), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_0.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_vol(func2_nosql_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_1.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_vol(func2_nosql_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_10.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_stb(func2_nosql_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_11.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_stb(func2_nosql_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_110.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_read_setint_sql_vol(func2_nosql_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_111.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_read_setint_sql_vol(func2_nosql_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_112.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_read_setint_sql_vol(func2_nosql_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_113.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_read_setint_sql_vol(func2_sql_int_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_114.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_read_setint_sql_vol(func2_sql_int_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_115.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_read_setint_sql_vol(func2_sql_int_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_12.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_stb(func2_nosql_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_120.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_read_setint_sql_stb(func2_nosql_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_121.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_read_setint_sql_stb(func2_nosql_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_122.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_read_setint_sql_stb(func2_nosql_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_123.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_read_setint_sql_stb(func2_sql_int_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_124.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_read_setint_sql_stb(func2_sql_int_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_125.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_read_setint_sql_stb(func2_sql_int_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_13.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_stb(func2_sql_int_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_14.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_stb(func2_sql_int_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_15.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_stb(func2_sql_int_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_150.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_mod_setint_vol(func2_nosql_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_151.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_mod_setint_vol(func2_nosql_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_152.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_mod_setint_vol(func2_nosql_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_153.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_mod_setint_vol(func2_sql_int_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_154.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_mod_setint_vol(func2_sql_int_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_155.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_mod_setint_vol(func2_sql_int_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_160.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_mod_setint_stb(func2_nosql_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_161.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_mod_setint_stb(func2_nosql_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_162.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_mod_setint_stb(func2_nosql_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_163.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_mod_setint_stb(func2_sql_int_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_164.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_mod_setint_stb(func2_sql_int_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_with_withfunc2_165.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_mod_setint_stb(func2_sql_int_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_2.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_vol(func2_nosql_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_20.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_imm(func2_nosql_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_21.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_imm(func2_nosql_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_22.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_imm(func2_nosql_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_23.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_imm(func2_sql_int_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_24.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_imm(func2_sql_int_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_25.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_imm(func2_sql_int_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_3.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_vol(func2_sql_int_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_30.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_vol(func2_nosql_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_31.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_vol(func2_nosql_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_32.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_vol(func2_nosql_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_33.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_vol(func2_sql_int_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_34.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_vol(func2_sql_int_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_35.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_vol(func2_sql_int_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_4.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_vol(func2_sql_int_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_40.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_stb(func2_nosql_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_41.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_stb(func2_nosql_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_42.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_stb(func2_nosql_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_43.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_stb(func2_sql_int_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_44.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_stb(func2_sql_int_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_45.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_stb(func2_sql_int_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_5.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_nosql_vol(func2_sql_int_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_50.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_imm(func2_nosql_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_51.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_imm(func2_nosql_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_52.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_imm(func2_nosql_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_53.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_imm(func2_sql_int_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_54.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_imm(func2_sql_int_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_55.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_int_imm(func2_sql_int_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_60.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_setint_vol(func2_nosql_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_61.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_setint_vol(func2_nosql_stb(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_62.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_setint_vol(func2_nosql_imm(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_with_withfunc2_63.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
WITH v(a, b) AS (SELECT func1_sql_setint_vol(func2_sql_int_vol(a)), b FROM foo WHERE b < 5) SELECT v1.a, v2.b FROM v AS v1, v AS v2 WHERE v1.a < v2.a order by v1.a, v2.b;  

-- start_ignore
drop schema functionProperty_1601_1700 cascade;
-- end_ignore
