
-- start_ignore
create schema functionProperty_1301_1400;
set search_path to functionProperty_1301_1400;

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
-- @description function_in_subqry_constant_withfunc2_104.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_int_sql_stb(func2_sql_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_105.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_int_sql_stb(func2_sql_int_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_106.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_int_sql_stb(func2_read_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_107.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_int_sql_stb(func2_read_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_108.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_int_sql_stb(func2_mod_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_109.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_int_sql_stb(func2_mod_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_11.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_nosql_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_110.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_vol(func2_nosql_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_111.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_vol(func2_nosql_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_112.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_vol(func2_nosql_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_113.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_vol(func2_sql_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_114.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_vol(func2_sql_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_115.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_vol(func2_sql_int_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_116.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_vol(func2_read_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_117.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_vol(func2_read_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_118.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_vol(func2_mod_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_119.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_vol(func2_mod_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_12.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_nosql_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_120.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_nosql_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_121.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_nosql_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_122.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_nosql_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_123.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_sql_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_124.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_sql_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_125.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_sql_int_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_126.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_read_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_127.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_read_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_128.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_mod_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_129.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_read_setint_sql_stb(func2_mod_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_13.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_sql_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_130.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_nosql_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_131.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_nosql_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_132.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_nosql_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_133.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_sql_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_134.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_sql_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_135.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_sql_int_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_136.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_read_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_137.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_read_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_138.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_mod_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_139.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_vol(func2_mod_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_14.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_sql_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_140.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_nosql_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_141.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_nosql_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_142.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_nosql_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_143.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_sql_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_144.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_sql_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_145.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_sql_int_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_146.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_read_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_147.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_read_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_148.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_mod_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_149.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_int_stb(func2_mod_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_15.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_sql_int_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_150.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_vol(func2_nosql_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_151.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_vol(func2_nosql_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_152.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_vol(func2_nosql_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_153.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_vol(func2_sql_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_154.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_vol(func2_sql_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_155.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_vol(func2_sql_int_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_156.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_vol(func2_read_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_157.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_vol(func2_read_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_158.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_vol(func2_mod_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_159.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_vol(func2_mod_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_16.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_read_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_160.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_nosql_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_161.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_nosql_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_162.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_nosql_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_163.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_sql_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_164.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_sql_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_165.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_sql_int_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_166.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_read_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @executemode ORCA_PLANNER_DIFF
-- @description function_in_subqry_constant_withfunc2_167.sql
-- @db_name functionproperty

-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_read_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_168.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_mod_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_169.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_mod_setint_stb(func2_mod_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_17.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_read_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_18.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_mod_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_19.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_stb(func2_mod_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_2.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_vol(func2_nosql_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_20.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_nosql_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_21.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_nosql_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_22.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_nosql_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_23.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_sql_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_24.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_sql_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_25.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_sql_int_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_26.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_read_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_27.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_read_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_28.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_mod_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_29.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_imm(func2_mod_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_3.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_vol(func2_sql_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_30.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_nosql_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_31.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_nosql_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_32.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_nosql_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_33.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_sql_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_34.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_sql_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_35.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_sql_int_imm(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_36.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_read_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_37.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_read_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_38.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_mod_int_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_39.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_int_vol(func2_mod_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_4.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_nosql_vol(func2_sql_int_stb(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_40.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_int_stb(func2_nosql_vol(5)) from foo) r order by 1,2,3; 
-- start_ignore
DROP TABLE foo;
DROP TABLE bar;
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
-- end_ignore
-- @description function_in_subqry_constant_withfunc2_41.sql
-- @db_name functionproperty
-- @author tungs1
-- @modified 2013-04-03 12:00:00
-- @created 2013-04-03 12:00:00
-- @tags functionProperties 
SELECT * FROM foo, (SELECT func1_sql_int_stb(func2_nosql_stb(5)) from foo) r order by 1,2,3; 

-- start_ignore
drop schema functionProperty_1301_1400 cascade;
-- end_ignore
