--
-- Test data type behavior
--

--
-- Base/common types
--

CREATE FUNCTION test_type_conversion_bool(x bool) RETURNS bool AS $$
plpy.info(x, type(x))
return x
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_bool(true);
SELECT * FROM test_type_conversion_bool(false);
SELECT * FROM test_type_conversion_bool(null);


-- test various other ways to express Booleans in Python
CREATE FUNCTION test_type_conversion_bool_other(n int) RETURNS bool AS $$
# numbers
if n == 0:
   ret = 0
elif n == 1:
   ret = 5
# strings
elif n == 2:
   ret = ''
elif n == 3:
   ret = 'fa' # true in Python, false in PostgreSQL
# containers
elif n == 4:
   ret = []
elif n == 5:
   ret = [0]
plpy.info(ret, not not ret)
return ret
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_bool_other(0);
SELECT * FROM test_type_conversion_bool_other(1);
SELECT * FROM test_type_conversion_bool_other(2);
SELECT * FROM test_type_conversion_bool_other(3);
SELECT * FROM test_type_conversion_bool_other(4);
SELECT * FROM test_type_conversion_bool_other(5);


CREATE FUNCTION test_type_conversion_char(x char) RETURNS char AS $$
plpy.info(x, type(x))
return x
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_char('a');
SELECT * FROM test_type_conversion_char(null);


CREATE FUNCTION test_type_conversion_int2(x int2) RETURNS int2 AS $$
plpy.info(x, type(x))
return x
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_int2(100::int2);
SELECT * FROM test_type_conversion_int2(-100::int2);
SELECT * FROM test_type_conversion_int2(null);


CREATE FUNCTION test_type_conversion_int4(x int4) RETURNS int4 AS $$
plpy.info(x, type(x))
return x
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_int4(100);
SELECT * FROM test_type_conversion_int4(-100);
SELECT * FROM test_type_conversion_int4(null);


CREATE FUNCTION test_type_conversion_int8(x int8) RETURNS int8 AS $$
plpy.info(x, type(x))
return x
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_int8(100);
SELECT * FROM test_type_conversion_int8(-100);
SELECT * FROM test_type_conversion_int8(5000000000);
SELECT * FROM test_type_conversion_int8(null);


CREATE FUNCTION test_type_conversion_numeric(x numeric) RETURNS numeric AS $$
plpy.info(x, type(x))
return x
$$ LANGUAGE plpythonu;

/* The current implementation converts numeric to float. */
SELECT * FROM test_type_conversion_numeric(100);
SELECT * FROM test_type_conversion_numeric(-100);
SELECT * FROM test_type_conversion_numeric(5000000000.5);
SELECT * FROM test_type_conversion_numeric(null);


CREATE FUNCTION test_type_conversion_float4(x float4) RETURNS float4 AS $$
plpy.info(x, type(x))
return x
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_float4(100);
SELECT * FROM test_type_conversion_float4(-100);
SELECT * FROM test_type_conversion_float4(5000.5);
SELECT * FROM test_type_conversion_float4(null);


CREATE FUNCTION test_type_conversion_float8(x float8) RETURNS float8 AS $$
plpy.info(x, type(x))
return x
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_float8(100);
SELECT * FROM test_type_conversion_float8(-100);
SELECT * FROM test_type_conversion_float8(5000000000.5);
SELECT * FROM test_type_conversion_float8(null);


CREATE FUNCTION test_type_conversion_text(x text) RETURNS text AS $$
plpy.info(x, type(x))
return x
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_text('hello world');
SELECT * FROM test_type_conversion_text(null);


CREATE FUNCTION test_type_conversion_bytea(x bytea) RETURNS bytea AS $$
plpy.info(x, type(x))
return x
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_bytea('hello world');
SELECT * FROM test_type_conversion_bytea(E'null\\000byte');
SELECT * FROM test_type_conversion_bytea(null);


CREATE FUNCTION test_type_marshal() RETURNS bytea AS $$
import marshal
return marshal.dumps('hello world')
$$ LANGUAGE plpythonu;

CREATE FUNCTION test_type_unmarshal(x bytea) RETURNS text AS $$
import marshal
try:
    return marshal.loads(x)
except ValueError, e:
    return 'FAILED: ' + str(e)
$$ LANGUAGE plpythonu;

SELECT test_type_unmarshal(x) FROM test_type_marshal() x;


--
-- Domains
--

CREATE DOMAIN booltrue AS bool CHECK (VALUE IS TRUE OR VALUE IS NULL);

CREATE FUNCTION test_type_conversion_booltrue(x booltrue, y bool) RETURNS booltrue AS $$
return y
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_booltrue(true, true);
SELECT * FROM test_type_conversion_booltrue(false, true);
SELECT * FROM test_type_conversion_booltrue(true, false);


CREATE DOMAIN uint2 AS int2 CHECK (VALUE >= 0);

CREATE FUNCTION test_type_conversion_uint2(x uint2, y int) RETURNS uint2 AS $$
plpy.info(x, type(x))
return y
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_uint2(100::uint2, 50);
SELECT * FROM test_type_conversion_uint2(100::uint2, -50);
SELECT * FROM test_type_conversion_uint2(null, 1);


CREATE DOMAIN nnint AS int CHECK (VALUE IS NOT NULL);

CREATE FUNCTION test_type_conversion_nnint(x nnint, y int) RETURNS nnint AS $$
return y
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_nnint(10, 20);
SELECT * FROM test_type_conversion_nnint(null, 20);
SELECT * FROM test_type_conversion_nnint(10, null);


CREATE DOMAIN bytea10 AS bytea CHECK (octet_length(VALUE) = 10 AND VALUE IS NOT NULL);

CREATE FUNCTION test_type_conversion_bytea10(x bytea10, y bytea) RETURNS bytea10 AS $$
plpy.info(x, type(x))
return y
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_bytea10('hello wold', 'hello wold');
SELECT * FROM test_type_conversion_bytea10('hello world', 'hello wold');
SELECT * FROM test_type_conversion_bytea10('hello word', 'hello world');
SELECT * FROM test_type_conversion_bytea10(null, 'hello word');
SELECT * FROM test_type_conversion_bytea10('hello word', null);


--
-- Arrays
--

CREATE FUNCTION test_type_conversion_array_int4(x int4[]) RETURNS int4[] AS $$
plpy.info(x, type(x))
return x
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_array_int4(ARRAY[0, 100]);
SELECT * FROM test_type_conversion_array_int4(ARRAY[0,-100,55]);
SELECT * FROM test_type_conversion_array_int4(ARRAY[NULL,1]);
SELECT * FROM test_type_conversion_array_int4(ARRAY[]::integer[]);
SELECT * FROM test_type_conversion_array_int4(NULL);
SELECT * FROM test_type_conversion_array_int4(ARRAY[[1,2,3],[4,5,6]]);
SELECT * FROM test_type_conversion_array_int4(ARRAY[[[1,2,NULL],[NULL,5,6]],[[NULL,8,9],[10,11,12]]]);
SELECT * FROM test_type_conversion_array_int4('[2:4]={1,2,3}');

CREATE FUNCTION test_type_conversion_array_int8(x int8[]) RETURNS int8[] AS $$
plpy.info(x, type(x))
return x
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_array_int8(ARRAY[[[1,2,NULL],[NULL,5,6]],[[NULL,8,9],[10,11,12]]]::int8[]);

CREATE FUNCTION test_type_conversion_array_date(x date[]) RETURNS date[] AS $$
plpy.info(x, type(x))
return x
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_array_date(ARRAY[[['2016-09-21','2016-09-22',NULL],[NULL,'2016-10-21','2016-10-22']],
            [[NULL,'2016-11-21','2016-10-21'],['2015-09-21','2015-09-22','2014-09-21']]]::date[]);

CREATE FUNCTION test_type_conversion_array_timestamp(x timestamp[]) RETURNS timestamp[] AS $$
plpy.info(x, type(x))
return x
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_array_timestamp(ARRAY[[['2016-09-21 15:34:24.078792-04','2016-10-22 11:34:24.078795-04',NULL],
            [NULL,'2016-10-21 11:34:25.078792-04','2016-10-21 11:34:24.098792-04']],
            [[NULL,'2016-01-21 11:34:24.078792-04','2016-11-21 11:34:24.108792-04'],
            ['2015-09-21 11:34:24.079792-04','2014-09-21 11:34:24.078792-04','2013-09-21 11:34:24.078792-04']]]::timestamp[]);


CREATE OR REPLACE FUNCTION pyreturnmultidemint4(h int4, i int4, j int4, k int4 ) RETURNS int4[] AS $BODY$
m = [[[[x for x in range(h)] for y in range(i)] for z in range(j)] for w in range(k)]
plpy.info(m, type(m))
return m
$BODY$ LANGUAGE plpythonu;

select pyreturnmultidemint4(8,5,3,2);

CREATE OR REPLACE FUNCTION pyreturnmultidemint8(h int4, i int4, j int4, k int4 ) RETURNS int8[] AS $BODY$
m = [[[[x for x in range(h)] for y in range(i)] for z in range(j)] for w in range(k)]
plpy.info(m, type(m))
return m
$BODY$ LANGUAGE plpythonu;

select pyreturnmultidemint8(5,5,3,2);

CREATE OR REPLACE FUNCTION pyreturnmultidemfloat4(h int4, i int4, j int4, k int4 ) RETURNS float4[] AS $BODY$
m = [[[[x for x in range(h)] for y in range(i)] for z in range(j)] for w in range(k)]
plpy.info(m, type(m))
return m
$BODY$ LANGUAGE plpythonu;

select pyreturnmultidemfloat4(6,5,3,2);

CREATE OR REPLACE FUNCTION pyreturnmultidemfloat8(h int4, i int4, j int4, k int4 ) RETURNS float8[] AS $BODY$
m = [[[[x for x in range(h)] for y in range(i)] for z in range(j)] for w in range(k)]
plpy.info(m, type(m))
return m
$BODY$ LANGUAGE plpythonu;

select pyreturnmultidemfloat8(7,5,3,2);

CREATE FUNCTION test_type_conversion_array_text(x text[]) RETURNS text[] AS $$
plpy.info(x, type(x))
return x
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_array_text(ARRAY['foo', 'bar']);
SELECT * FROM test_type_conversion_array_text(ARRAY[['foo', 'bar'],['foo2', 'bar2']]);


CREATE FUNCTION test_type_conversion_array_bytea(x bytea[]) RETURNS bytea[] AS $$
plpy.info(x, type(x))
return x
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_array_bytea(ARRAY[E'\\xdeadbeef'::bytea, NULL]);


CREATE FUNCTION test_type_conversion_array_mixed1() RETURNS text[] AS $$
return [123, 'abc']
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_array_mixed1();


CREATE FUNCTION test_type_conversion_array_mixed2() RETURNS int[] AS $$
return [123, 'abc']
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_array_mixed2();

CREATE FUNCTION test_type_conversion_mdarray_malformed() RETURNS int[] AS $$
return [[1,2,3],[4,5]]
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_mdarray_malformed();

CREATE FUNCTION test_type_conversion_mdarray_toodeep() RETURNS int[] AS $$
return [[[[[[[1]]]]]]]
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_mdarray_toodeep();


CREATE FUNCTION test_type_conversion_array_record() RETURNS type_record[] AS $$
return [{'first': 'one', 'second': 42}, {'first': 'two', 'second': 11}]
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_array_record();


CREATE FUNCTION test_type_conversion_array_string() RETURNS text[] AS $$
return 'abc'
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_array_string();

CREATE FUNCTION test_type_conversion_array_tuple() RETURNS text[] AS $$
return ('abc', 'def')
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_array_tuple();

CREATE FUNCTION test_type_conversion_array_error() RETURNS int[] AS $$
return 5
$$ LANGUAGE plpythonu;

SELECT * FROM test_type_conversion_array_error();
