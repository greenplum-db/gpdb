--
-- This test file sets up test tables and functions for subsequent
-- qp_functions_in_* tests, which live in different files. They have been
-- split into several files, to make the size more manageable, and so
-- that they can be run efficiently in parallel.
--

create schema qp_funcs_in_contexts;
set search_path='qp_funcs_in_contexts';

-- The test functions rely on these tables, but we don't create them here.
-- Instead, each test that uses the below functions, have to create their
-- own copies of these, so that they don't interfere with each other.
/*
CREATE TABLE foo (a int, b int);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;
CREATE TABLE bar (c int, d int);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;
*/

-- Create test functions

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

CREATE FUNCTION func_nosql_record_imm(x int) RETURNS record as $$
BEGIN
    return (x, x + 1);
END
$$ LANGUAGE plpgsql NO SQL IMMUTABLE;
