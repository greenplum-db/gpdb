-- Tests Datatypes for Input Parameters and return values
--
-- PL/Python is written such that certain Postgres Types map to 
-- certain Python Types, and some return types likewise have 
-- special cast functions.
--
-- The purpose of this test is to verify that all of the datatype
-- mapping code is working correctly, as well as the code that
-- manages OUT parameters, RETURNS TABLE, and other variants
-- on how parameters are returned from PL/Python functions.
--
-- Every function (both single value and setof) is run both from
-- the SELECT clause and the FROM clause since these are different
-- execution paths.
--
-- Greenplum has another distinction regarding whether the function
-- is run on the master or the segment.  So we additionally run
-- every function as a SELECT clause function over a table with
-- a single row.
--  
-- Because Greenplum is slow at reporting errors from segments
-- we only execute against gp_single_row for the statements that
-- should succeed, or where we expect different results (executing SQL)
\set VERBOSITY terse
CREATE TABLE gp_single_row(a int) distributed by (a);
insert into gp_single_row values(1);

-- Set up test functions first.
CREATE FUNCTION test_return_void(s text) 
RETURNS void AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;
CREATE FUNCTION test_return_bool(s text) 
RETURNS bool AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;
CREATE FUNCTION test_return_text(s text) 
RETURNS text AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;
CREATE FUNCTION test_return_bytea(s text) 
RETURNS bytea AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;
CREATE FUNCTION test_return_circle(s text) 
RETURNS circle AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;
CREATE FUNCTION test_return_array_int(s text) 
RETURNS int[] AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;
CREATE FUNCTION test_return_array_text(s text) 
RETURNS text[] AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;
CREATE FUNCTION test_return_setof_void(s text) 
RETURNS setof void AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;
CREATE FUNCTION test_return_setof_bool(s text) 
RETURNS setof bool AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;
CREATE FUNCTION test_return_setof_text(s text) 
RETURNS setof text AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;
CREATE FUNCTION test_return_setof_bytea(s text) 
RETURNS setof bytea AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;
CREATE FUNCTION test_return_setof_circle(s text) 
RETURNS setof circle AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;

-- RETURNS record types
CREATE FUNCTION test_return_table_record(s text) 
RETURNS table_record AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;
CREATE FUNCTION test_return_type_record(s text) 
RETURNS type_record AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;
CREATE FUNCTION test_return_setof_table_record(s text) 
RETURNS table_record AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;
CREATE FUNCTION test_return_setof_type_record(s text) 
RETURNS type_record AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;

-- RETURNS with OUT Parameters
CREATE FUNCTION test_return_out_text(s text, OUT text) 
AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;
CREATE FUNCTION test_return_out_setof_text(s text, OUT text) 
RETURNS setof text AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;
CREATE FUNCTION test_return_out_record(s text, OUT first text, OUT second int4) 
AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;
CREATE FUNCTION test_return_out_setof_record(s text, OUT first text, OUT second int4)
RETURNS setof record AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;

-- RETURNS TABLE
CREATE FUNCTION test_return_table(s text) RETURNS TABLE(first text, second int4)
AS $$
  exec('y = ' + s, globals())
  return y
$$ language plpythonu;



-- =============================================
-- TEST 1:  RETURN VALUE TESTING - Scalar values
-- =============================================
--
-- There are 4 "special" return datatypes: Void, Bool, Text, Bytea
-- all other datatypes are handled by converting the python object to
-- a cstring and then calling the typinput function for the type.


--
-- Case 1: Return void
--
-- The only return that is valid for a void returns is Py_None

-- From Python None
SELECT test_return_void('None');
SELECT * FROM test_return_void('None');
SELECT test_return_void('None') 
FROM gp_single_row;

-- Error conditions

-- [ERROR] From Python empty string
SELECT test_return_void('""');
SELECT * FROM test_return_void('""');

-- [ERROR] From Python empty list
SELECT test_return_void('[]');
SELECT * FROM test_return_void('[]');

-- [ERROR] From Python empty dict
SELECT test_return_void('{}');
SELECT * FROM test_return_void('{}');


--
-- Case 2: Return Bool
--
-- Any Object in Python can be resolved as a boolean value.  
--   The conversion is based on the Python notion of True and False.
--   So the string 'f' is True, empty strings and containers are False
--

-- From Python None
SELECT test_return_bool('None');
SELECT * FROM test_return_bool('None');
SELECT test_return_bool('None') FROM gp_single_row;

-- From Python bool
SELECT test_return_bool('True');
SELECT * FROM test_return_bool('True');
SELECT test_return_bool('True') FROM gp_single_row;

SELECT test_return_bool('False');
SELECT * FROM test_return_bool('False');
SELECT test_return_bool('False') FROM gp_single_row;

-- From Python empty string
SELECT test_return_bool('""');
SELECT * FROM test_return_bool('""');
SELECT test_return_bool('""') FROM gp_single_row;

-- From Python string
SELECT test_return_bool('"value"');
SELECT * FROM test_return_bool('"value"');
SELECT test_return_bool('"value"') FROM gp_single_row;

-- From Python empty list
SELECT test_return_bool('[]');
SELECT * FROM test_return_bool('[]');
SELECT test_return_bool('[]') FROM gp_single_row;

-- From Python non-empty list
SELECT test_return_bool('[False]');
SELECT * FROM test_return_bool('[False]');
SELECT test_return_bool('[False]') FROM gp_single_row;

-- From Python empty dict
SELECT test_return_bool('{}');
SELECT * FROM test_return_bool('{}');
SELECT test_return_bool('{}') FROM gp_single_row;

-- From Python non-empty dict
SELECT test_return_bool('{None: None}');
SELECT * FROM test_return_bool('{None: None}');
SELECT test_return_bool('{None: None}') FROM gp_single_row;

-- From Python Object with bad str() function
SELECT test_return_bool(
    E'1; exec("class Foo:\\n  def __str__(self): return None", globals()); y = Foo()'
);
SELECT * FROM test_return_bool(
    E'1; exec("class Foo:\\n  def __str__(self): return None", globals()); y = Foo()'
);

-- Error conditions
--   None


--
-- Case 3: Return Text
--

-- From Python None
SELECT test_return_text('None');
SELECT * FROM test_return_text('None');
SELECT test_return_text('None') 
FROM gp_single_row;

-- From Python bool
SELECT test_return_text('True');
SELECT * FROM test_return_text('True');
SELECT test_return_text('True') 
FROM gp_single_row;

-- From Python empty string
SELECT test_return_text('""');
SELECT * FROM test_return_text('""');
SELECT test_return_text('""') 
FROM gp_single_row;

-- From Python string
SELECT test_return_text('"value"');
SELECT * FROM test_return_text('"value"');
SELECT test_return_text('"value"') 
FROM gp_single_row;

-- From Python empty list
SELECT test_return_text('[]');
SELECT * FROM test_return_text('[]');
SELECT test_return_text('[]') 
FROM gp_single_row;

-- From Python non-empty list
SELECT test_return_text('[False]');
SELECT * FROM test_return_text('[False]');
SELECT test_return_text('[False]') 
FROM gp_single_row;

-- From Python empty dict
SELECT test_return_text('{}');
SELECT * FROM test_return_text('{}');
SELECT test_return_text('{}') 
FROM gp_single_row;

-- From Python non-empty dict
SELECT test_return_text('{None: None}');
SELECT * FROM test_return_text('{None: None}');
SELECT test_return_text('{None: None}') 
FROM gp_single_row;

-- From Python Object with str() function
SELECT test_return_text(
    E'1; exec("class Foo:\\n  def __str__(self): return \\"test\\"", globals()); y = Foo()'
);
SELECT * FROM test_return_text(
    E'1; exec("class Foo:\\n  def __str__(self): return \\"test\\"", globals()); y = Foo()'
);
SELECT test_return_text(
    E'1; exec("class Foo:\\n  def __str__(self): return \\"test\\"", globals()); y = Foo()'
) FROM gp_single_row;


-- Error conditions:
--   There are two ways that a "text" return can fail:
--     a) There isn't a __str__ function for the returned object
--     b) The returned string contains null characters (valid in Python)

-- [ERROR] From Python String with null characters
SELECT test_return_text(E'"test\\0"');
SELECT * FROM test_return_text(E'"test\\0"');

-- [ERROR] From Python Object with bad str() function
SELECT test_return_text(
    E'1; exec("class Foo:\\n  def __str__(self): return None", globals()); y = Foo()'
);
SELECT * FROM test_return_text(
    E'1; exec("class Foo:\\n  def __str__(self): return None", globals()); y = Foo()'
);


--
-- Case 4: Return Bytea
--
-- Bytea is like text, except that it allows return strings to contain null 
-- characters
--

-- From Python None
SELECT test_return_bytea('None');
SELECT * FROM test_return_bytea('None');
SELECT test_return_bytea('None') 
FROM gp_single_row;

-- From Python bool
SELECT test_return_bytea('True');
SELECT * FROM test_return_bytea('True');
SELECT test_return_bytea('True') 
FROM gp_single_row;

-- From Python empty string
SELECT test_return_bytea('""');
SELECT * FROM test_return_bytea('""');
SELECT test_return_bytea('""') 
FROM gp_single_row;

-- From Python string
SELECT test_return_bytea('"value"');
SELECT * FROM test_return_bytea('"value"');
SELECT test_return_bytea('"value"') 
FROM gp_single_row;

-- From Python empty list
SELECT test_return_bytea('[]');
SELECT * FROM test_return_bytea('[]');
SELECT test_return_bytea('[]') 
FROM gp_single_row;

-- From Python non-empty list
SELECT test_return_bytea('[False]');
SELECT * FROM test_return_bytea('[False]');
SELECT test_return_bytea('[False]') 
FROM gp_single_row;

-- From Python empty dict
SELECT test_return_bytea('{}');
SELECT * FROM test_return_bytea('{}');
SELECT test_return_bytea('{}') 
FROM gp_single_row;

-- From Python non-empty dict
SELECT test_return_bytea('{None: None}');
SELECT * FROM test_return_bytea('{None: None}');
SELECT test_return_bytea('{None: None}') 
FROM gp_single_row;

-- From Python Object with str() function
SELECT test_return_bytea(
    E'1; exec("class Foo:\\n  def __str__(self): return \\"test\\"", globals()); y = Foo()'
);
SELECT * FROM test_return_bytea(
    E'1; exec("class Foo:\\n  def __str__(self): return \\"test\\"", globals()); y = Foo()'
);
SELECT test_return_bytea(
    E'1; exec("class Foo:\\n  def __str__(self): return \\"test\\"", globals()); y = Foo()'
) FROM gp_single_row;

-- From Python String with null characters
SELECT test_return_bytea(E'"test\\0"');
SELECT * FROM test_return_bytea(E'"test\\0"');
SELECT test_return_bytea(E'"test\\0"') 
FROM gp_single_row;

-- Error conditions

-- [ERROR] From Python Object with bad str() function
SELECT test_return_bytea(
    E'1; exec("class Foo:\\n  def __str__(self): return None", globals()); y = Foo()'
);
SELECT * FROM test_return_bytea(
    E'1; exec("class Foo:\\n  def __str__(self): return None", globals()); y = Foo()'
);
SELECT test_return_bytea(
    E'1; exec("class Foo:\\n  def __str__(self): return None", globals()); y = Foo()'
) FROM gp_single_row;


--
-- Case 5: Return Circle
--
-- We use Circle as a representative type that uses the typinput function
-- This equally applies to other types such as "int2", "int4", "money", etc.
--

-- From Python None
SELECT test_return_circle('None');
SELECT * FROM test_return_circle('None');
SELECT test_return_circle('None') 
FROM gp_single_row;

--  From Python String
SELECT test_return_circle('"((3,1),4)"');
SELECT * FROM test_return_circle('"((3,1),4)"');
SELECT test_return_circle('"((3,1),4)"') 
FROM gp_single_row;

-- From Python List
SELECT test_return_circle('((3,1),4)');
SELECT * FROM test_return_circle('((3,1),4)');
SELECT test_return_circle('((3,1),4)') 
FROM gp_single_row;

-- Questionable error conditions

-- [ERROR] From Python Tuple
SELECT test_return_circle('[[3,1],4]');
SELECT * FROM test_return_circle('[[3,1],4]');

-- Error conditions

-- [ERROR] From Python bool
SELECT test_return_circle('True');
SELECT * FROM test_return_circle('True');

-- [ERROR] From Python empty string
SELECT test_return_circle('""');
SELECT * FROM test_return_circle('""');

-- [ERROR] From Python string
SELECT test_return_circle('"value"');
SELECT * FROM test_return_circle('"value"');

-- [ERROR] From Python empty list
SELECT test_return_circle('[]');
SELECT * FROM test_return_circle('[]');

-- [ERROR] From Python non-empty list
SELECT test_return_circle('[False]');
SELECT * FROM test_return_circle('[False]');

-- [ERROR] From Python empty dict
SELECT test_return_circle('{}');
SELECT * FROM test_return_circle('{}');

-- [ERROR] From Python non-empty dict
SELECT test_return_circle('{None: None}');
SELECT * FROM test_return_circle('{None: None}');

-- [ERROR] From Python String with null characters
SELECT test_return_circle(E'"test\\0"');
SELECT * FROM test_return_circle(E'"test\\0"');

--
-- Case 5: Return array of integers
--
-- Test returning arrays of fixed-width elements from PL/Python functions
--

-- From Python None
SELECT test_return_array_int('None');
SELECT * FROM test_return_array_int('None');

-- From Python empty list
SELECT test_return_array_int('[]');
SELECT * FROM test_return_array_int('[]');

-- From Python non-empty list
SELECT test_return_array_int('[1,2]');
SELECT * FROM test_return_array_int('[1,2]');

-- From Python multiple lists
SELECT test_return_array_int('[[1,2,3],[4,5,6]]');
SELECT * FROM test_return_array_int('[[1,2,3],[4,5,6]]');

-- Error conditions

-- Multi-dimensional array with non-fixed dimension sizes
SELECT test_return_array_int('[[1,2,3],[1,2]]');
SELECT * FROM test_return_array_int('[[1,2,3],[1,2]]');

-- Multi-dimensional array with mix of arrays and atomic elements
SELECT test_return_array_int('[[1,2,3],[1,[2,3],[4,5]]]');
SELECT * FROM test_return_array_int('[[1,2,3],[1,[2,3],[4,5]]]');

-- Multi-dimensional array with missing dimensions
SELECT test_return_array_int('[[1,2,3],None,[4,5,6]]');
SELECT * FROM test_return_array_int('[[1,2,3],None,[4,5,6]]');

--
-- Case 5: Return array of texts
--
-- Test returning arrays of variable-width elements from PL/Python functions
--

-- From Python None
SELECT test_return_array_text('None');
SELECT * FROM test_return_array_text('None');

-- From Python empty list
SELECT test_return_array_text('[]');
SELECT * FROM test_return_array_text('[]');

-- From Python non-empty list
SELECT test_return_array_text('["abc","def"]');
SELECT * FROM test_return_array_text('["abc","def"]');

-- From Python multiple lists
SELECT test_return_array_text('[["a","bcd","ef"],[None,"gh","ijklm"]]');
SELECT * FROM test_return_array_text('[["a","bcd","ef"],[None,"gh","ijklm"]]');

-- Error conditions

-- Multi-dimensional array with non-fixed dimension sizes
SELECT test_return_array_text('[["a","bcd","ef"],[None,"gh","ijklm","ERROR"]]');
SELECT * FROM test_return_array_text('[["a","bcd","ef"],[None,"gh","ijklm","ERROR"]]');

-- Multi-dimensional array with mix of arrays and atomic elements
SELECT test_return_array_text('[[["a"],"b"],["c",["d","e"]]]');
SELECT * FROM test_return_array_text('[[["a"],"b"],["c",["d","e"]]]');

-- Multi-dimensional array with missing dimensions
SELECT test_return_array_text('[["abc","def"],None,["ghij","k"]]');
SELECT * FROM test_return_array_text('[["abc","def"],None,["ghij","k"]]');

-- ===================================================
-- TEST 2:  RETURN VALUE TESTING - SETOF scalar values
-- ===================================================
--

-- The test for the scalar values also need to be tested with SETOF since this
-- goes through a different codepath.
--
-- In this case the returned Python Object is converted into an iterator object
-- and each value of the resulting iterator is returned as a row matching the
-- return datatype.
--
-- This can create some peculiar results due to how some Python objects are
-- actually converted to iterators.  Most notably an iterator over a string
-- is the set of characters comprising the string, and an iterator over a dict
-- is the set of KEYS of the dictionary.


--
-- Case 1: Return SETOF void
--

-- Success conditions

-- From Python List of None
SELECT test_return_setof_void('[None, None]');
SELECT * FROM test_return_setof_void('[None, None]');
SELECT test_return_setof_void('[None, None]') 
FROM gp_single_row;

-- From Python Empty List
SELECT test_return_setof_void('[]');
SELECT * FROM test_return_setof_void('[]');
SELECT test_return_setof_void('[]') 
FROM gp_single_row;

-- Error conditions

-- [ERROR] From Python None
SELECT test_return_setof_void('None');
SELECT * FROM test_return_setof_void('None');

-- [ERROR] From Python string
SELECT test_return_setof_void('"value"');
SELECT * FROM test_return_setof_void('"value"');

-- [ERROR] From Python List of values
SELECT test_return_setof_void('["value"]');
SELECT * FROM test_return_setof_void('["value"]');

-- [ERROR] From Python Dict with keys other than "None"
SELECT test_return_setof_void('{"value": None}');
SELECT * FROM test_return_setof_void('{"value": None}');

-- Questionable Success conditions

-- From Python empty string
SELECT test_return_setof_void('""');
SELECT * FROM test_return_setof_void('""');

-- From Python empty dictionary
SELECT test_return_setof_void('{}');
SELECT * FROM test_return_setof_void('{}');

-- From Python dictionary with a "None" key
SELECT test_return_setof_void('{None: "ignored"}');
SELECT * FROM test_return_setof_void('{None: "ignored"}');


--
-- Case 2: Return SETOF Bool
--

-- Success conditions

-- From Python Empty List
SELECT test_return_setof_bool('[]');
SELECT * FROM test_return_setof_bool('[]');
SELECT test_return_setof_bool('[]') 
FROM gp_single_row;

-- From Python List
SELECT test_return_setof_bool('[None, True, "value", [1], {"A": "B"}]');
SELECT * FROM test_return_setof_bool('[None, True, "value", [1], {"A": "B"}]');
SELECT test_return_setof_bool('[None, True, "value", [1], {"A": "B"}]') 
FROM gp_single_row;

-- Error conditions

-- [ERROR] From Python None
SELECT test_return_setof_bool('None');
SELECT * FROM test_return_setof_bool('None');

-- [ERROR] From Python Bool
SELECT test_return_setof_bool('True');
SELECT * FROM test_return_setof_bool('True');


-- Questionable Success conditions

-- From Python empty string
SELECT test_return_setof_bool('""');
SELECT * FROM test_return_setof_bool('""');

-- From Python empty dictionary
SELECT test_return_setof_bool('{}');
SELECT * FROM test_return_setof_bool('{}');

-- From Python string
SELECT test_return_setof_bool('"value"');
SELECT * FROM test_return_setof_bool('"value"');

-- From Python string with null values (which are also true)
SELECT test_return_setof_bool(E'"test\\0"');
SELECT * FROM test_return_setof_bool(E'"test\\0"');

-- From Python dictionary 
SELECT test_return_setof_bool('{True: "ignored", False: "ignored"}');
SELECT * FROM test_return_setof_bool('{True: "ignored", False: "ignored"}');


--
-- Case 3: Return SETOF Text
--

-- From Python Empty List
SELECT test_return_setof_text('[]');
SELECT * FROM test_return_setof_text('[]');
SELECT test_return_setof_text('[]') 
FROM gp_single_row;

-- From Python List of values
SELECT test_return_setof_text('[None, True, "value", [1], {"A": "B"}]');
SELECT * FROM test_return_setof_text('[None, True, "value", [1], {"A": "B"}]');
SELECT test_return_setof_text('[None, True, "value", [1], {"A": "B"}]') 
FROM gp_single_row;


-- Error conditions

-- [ERROR] From Python None
SELECT test_return_setof_text('None');
SELECT * FROM test_return_setof_text('None');

-- [ERROR] From Python Bool
SELECT test_return_setof_text('True');
SELECT * FROM test_return_setof_text('True');

-- [ERROR] From Python string with null values
SELECT test_return_setof_text(E'"test\\0"');
SELECT * FROM test_return_setof_text(E'"test\\0"');


-- Questionable Success conditions

-- From Python empty string
SELECT test_return_setof_text('""');
SELECT * FROM test_return_setof_text('""');

-- From Python empty dictionary
SELECT test_return_setof_text('{}');
SELECT * FROM test_return_setof_text('{}');

-- From Python string
SELECT test_return_setof_text('"value"');
SELECT * FROM test_return_setof_text('"value"');

-- From Python dictionary 
SELECT test_return_setof_text('{True: "ignored", False: "ignored"}');
SELECT * FROM test_return_setof_text('{True: "ignored", False: "ignored"}');


--
-- Case 4: Return SETOF Bytea
--

-- From Python Empty List
SELECT test_return_setof_bytea('[]');
SELECT * FROM test_return_setof_bytea('[]');
SELECT test_return_setof_bytea('[]') 
FROM gp_single_row;

-- From Python List of values
SELECT test_return_setof_bytea(E'[None, True, "value", [1], {"\\0": "B"}]');
SELECT * FROM test_return_setof_bytea(E'[None, True, "value", [1], {"\\0": "B"}]');
SELECT test_return_setof_bytea(E'[None, True, "value", [1], {"\\0": "B"}]') 
FROM gp_single_row;


-- Error conditions

-- [ERROR] From Python None
SELECT test_return_setof_bytea('None');
SELECT * FROM test_return_setof_bytea('None');

-- [ERROR] From Python Bool
SELECT test_return_setof_bytea('True');
SELECT * FROM test_return_setof_bytea('True');


-- Questionable Success conditions

-- From Python empty string
SELECT test_return_setof_bytea('""');
SELECT * FROM test_return_setof_bytea('""');

-- From Python empty dictionary
SELECT test_return_setof_bytea('{}');
SELECT * FROM test_return_setof_bytea('{}');

-- From Python string
SELECT test_return_setof_bytea('"value"');
SELECT * FROM test_return_setof_bytea('"value"');

-- From Python string with null values
SELECT test_return_setof_bytea(E'"test\\0"');
SELECT * FROM test_return_setof_bytea(E'"test\\0"');

-- From Python dictionary 
SELECT test_return_setof_bytea('{True: "ignored", False: "ignored"}');
SELECT * FROM test_return_setof_bytea('{True: "ignored", False: "ignored"}');


--
-- Case 5: Return SETOF Circle
--

-- From Python Empty List
SELECT test_return_setof_circle('[]');
SELECT * FROM test_return_setof_circle('[]');
SELECT test_return_setof_circle('[]') 
FROM gp_single_row;

-- From Python List of compatable values
SELECT test_return_setof_circle('[None, ((3,1),4), "((5,3),1)"]');
SELECT * FROM test_return_setof_circle('[None, ((3,1),4), "((5,3),1)"]');
SELECT test_return_setof_circle('[None, ((3,1),4), "((5,3),1)"]')
FROM gp_single_row;

-- Questionable Error conditions

-- [ERROR] From Python List of tuples
SELECT test_return_setof_circle('[ [[3,1],4] ]');
SELECT * FROM test_return_setof_circle('[ [[3,1],4] ]');


-- Error conditions

-- [ERROR] From Python None
SELECT test_return_setof_circle('None');
SELECT * FROM test_return_setof_circle('None');

-- [ERROR] From Python Bool
SELECT test_return_setof_circle('True');
SELECT * FROM test_return_setof_circle('True');

-- [ERROR] From Python string
SELECT test_return_setof_circle('"value"');
SELECT * FROM test_return_setof_circle('"value"');

-- [ERROR] From Python List of incompatable values
SELECT test_return_setof_circle('[None, "value"]');
SELECT * FROM test_return_setof_circle('[None, "value"]');

-- Questionable Success conditions

-- From Python empty string
SELECT test_return_setof_circle('""');
SELECT * FROM test_return_setof_circle('""');

-- From Python empty dictionary
SELECT test_return_setof_circle('{}');
SELECT * FROM test_return_setof_circle('{}');

-- From Python dictionary 
SELECT test_return_setof_circle('{((3,1),4): "ignored"}');
SELECT * FROM test_return_setof_circle('{((3,1),4): "ignored"}');



-- ===============================================
-- TEST 3:  RETURN VALUE TESTING - Compound values
-- ===============================================

-- Compound types, aka record types, are simply built up from
-- the existing primitive types.  
--
-- Within PL/Python the most common way of dealing with compound
-- types is to return them as a python dictionary with the keys
-- of the dictionary being the names of the compound elements
-- and the values being the values.


--
-- Case 1: From a User Defined Type
--

-- From Python None
SELECT test_return_type_record('None');
SELECT * FROM test_return_type_record('None');
SELECT test_return_type_record('None') 
FROM gp_single_row;

SELECT (test_return_type_record('None')).*;
SELECT (test_return_type_record('None')).*
FROM gp_single_row;

-- From Python List
SELECT test_return_type_record('("value", 4)');
SELECT * FROM test_return_type_record('("value", 4)');
SELECT test_return_type_record('("value", 4)')
FROM gp_single_row;

SELECT (test_return_type_record('("value", 4)')).*;
SELECT (test_return_type_record('("value", 4)')).*
FROM gp_single_row;


-- From Python Tuple
SELECT test_return_type_record('["value", 4]');
SELECT * FROM test_return_type_record('["value", 4]');
SELECT test_return_type_record('["value", 4]')
FROM gp_single_row;

SELECT (test_return_type_record('["value", 4]')).*;
SELECT (test_return_type_record('["value", 4]')).*
FROM gp_single_row;

-- From Python Dictionary
SELECT test_return_type_record('{"first": "value", "second": 4}');
SELECT * FROM test_return_type_record('{"first": "value", "second": 4}');
SELECT test_return_type_record('{"first": "value", "second": 4}')
FROM gp_single_row;

SELECT (test_return_type_record('{"first": "value", "second": 4}')).*;
SELECT (test_return_type_record('{"first": "value", "second": 4}')).*
FROM gp_single_row;

-- Error conditions

-- [ERROR] From Python bool
SELECT test_return_type_record('True');
SELECT * FROM test_return_type_record('True');

-- [ERROR] From Python empty string
SELECT test_return_type_record('""');
SELECT * FROM test_return_type_record('""');

-- [ERROR] From Python string
SELECT test_return_type_record('"value"');
SELECT * FROM test_return_type_record('"value"');

-- [ERROR] From Python non-empty list, wrong number of columns
SELECT test_return_type_record('[False]');
SELECT * FROM test_return_type_record('[False]');

-- [ERROR] From Python non-empty list, wrong datatypes
SELECT test_return_type_record('[4, "value"]');
SELECT * FROM test_return_type_record('[4, "value"]');

-- [ERROR] From Python empty dict
SELECT test_return_type_record('{}');
SELECT * FROM test_return_type_record('{}');

-- [ERROR] From Python without the needed columns
SELECT test_return_type_record('{None: None}');
SELECT * FROM test_return_type_record('{None: None}');

-- Questionable Success conditions

--  From Python String (one character per column)
SELECT test_return_type_record('"a4"');
SELECT * FROM test_return_type_record('"a4"');

-- From Python Dictionary, with extra fields
SELECT test_return_type_record('{"first": "value", "second": 4, "third": "ignored"}');
SELECT * FROM test_return_type_record('{"first": "value", "second": 4, "third": "ignored"}');


--
-- Case 2: From a Table Type
--

-- From Python None
SELECT test_return_table_record('None');
SELECT * FROM test_return_table_record('None');
SELECT test_return_table_record('None') 
FROM gp_single_row;

SELECT (test_return_table_record('None')).*;
SELECT (test_return_table_record('None')).*
FROM gp_single_row;

-- From Python List
SELECT test_return_table_record('("value", 4)');
SELECT * FROM test_return_table_record('("value", 4)');
SELECT test_return_table_record('("value", 4)')
FROM gp_single_row;

SELECT (test_return_table_record('("value", 4)')).*;
SELECT (test_return_table_record('("value", 4)')).*
FROM gp_single_row;

-- From Python Tuple
SELECT test_return_table_record('["value", 4]');
SELECT * FROM test_return_table_record('["value", 4]');
SELECT test_return_table_record('["value", 4]')
FROM gp_single_row;

SELECT (test_return_table_record('["value", 4]')).*;
SELECT (test_return_table_record('["value", 4]')).*
FROM gp_single_row;

-- From Python Dictionary
SELECT test_return_table_record('{"first": "value", "second": 4}');
SELECT * FROM test_return_table_record('{"first": "value", "second": 4}');
SELECT test_return_table_record('{"first": "value", "second": 4}')
FROM gp_single_row;

SELECT (test_return_table_record('{"first": "value", "second": 4}')).*;
SELECT (test_return_table_record('{"first": "value", "second": 4}')).*
FROM gp_single_row;

-- Error conditions

-- [ERROR] From Python bool
SELECT test_return_table_record('True');
SELECT * FROM test_return_table_record('True');

-- [ERROR] From Python empty string
SELECT test_return_table_record('""');
SELECT * FROM test_return_table_record('""');

-- [ERROR] From Python string
SELECT test_return_table_record('"value"');
SELECT * FROM test_return_table_record('"value"');

-- [ERROR] From Python non-empty list, wrong number of columns
SELECT test_return_table_record('[False]');
SELECT * FROM test_return_table_record('[False]');

-- [ERROR] From Python non-empty list, wrong datatypes
SELECT test_return_table_record('[4, "value"]');
SELECT * FROM test_return_table_record('[4, "value"]');

-- [ERROR] From Python empty dict
SELECT test_return_table_record('{}');
SELECT * FROM test_return_table_record('{}');

-- [ERROR] From Python without the needed columns
SELECT test_return_table_record('{None: None}');
SELECT * FROM test_return_table_record('{None: None}');

-- Questionable Success conditions

--  From Python String (one character per column)
SELECT test_return_table_record('"a4"');
SELECT * FROM test_return_table_record('"a4"');

-- From Python Dictionary, with extra fields
SELECT test_return_table_record('{"first": "value", "second": 4, "third": "ignored"}');
SELECT * FROM test_return_table_record('{"first": "value", "second": 4}, "third": "ignored"}');


-- =====================================================
-- TEST 4:  RETURN VALUE TESTING - SETOF Compound values
-- =====================================================

-- The test for the scalar Compound values also need to be tested with SETOF 
-- since this goes through a different codepath.
--
-- In this case the returned Python Object is converted into an iterator object
-- and each value of the resulting iterator is returned as a row matching the
-- return datatype


--
-- Case 1: From a User Defined Type
--

-- From Python empty list
SELECT test_return_setof_type_record('[]');
SELECT * FROM test_return_setof_type_record('[]');
SELECT test_return_setof_type_record('[]')
FROM gp_single_row;

SELECT (test_return_setof_type_record('[]')).*;
SELECT (test_return_setof_type_record('[]')).*
FROM gp_single_row;

-- From Python List
SELECT test_return_setof_type_record('[ ("value", 4), {"first": "test", "second": 2} ]');
SELECT * FROM test_return_setof_type_record('[ None, ("value", 4), {"first": "test", "second": 2} ]');
SELECT test_return_setof_type_record('[ ("value", 4), {"first": "test", "second": 2} ]')
FROM gp_single_row;

SELECT (test_return_setof_type_record('[ ("value", 4), {"first": "test", "second": 2} ]')).*;
SELECT (test_return_setof_type_record('[ ("value", 4), {"first": "test", "second": 2} ]')).*
FROM gp_single_row;

-- Error conditions

-- [ERROR] From Python None
SELECT test_return_setof_type_record('None');
SELECT * FROM test_return_setof_type_record('None');

-- [ERROR] From Python bool
SELECT test_return_setof_type_record('True');
SELECT * FROM test_return_setof_type_record('True');

-- [ERROR] From Python string
SELECT test_return_setof_type_record('"value"');
SELECT * FROM test_return_setof_type_record('"value"');

-- [ERROR] From Python List
SELECT test_return_setof_type_record('("value", 4)');
SELECT * FROM test_return_setof_type_record('("value", 4)');

-- [ERROR] From Python non-empty list, wrong number of columns
SELECT test_return_setof_type_record('[ [False] ]');
SELECT * FROM test_return_setof_type_record('[ [False] ]');

-- [ERROR] From Python non-empty list, wrong datatypes
SELECT test_return_setof_type_record('[ [4, "value"] ]');
SELECT * FROM test_return_setof_type_record('[ [4, "value"] ]');

-- [ERROR] From Python non-empty list, cointaining None
SELECT test_return_setof_type_record('[None]');
SELECT * FROM test_return_setof_type_record('[None]');

-- [ERROR] From Python dict without the needed columns
SELECT test_return_setof_type_record('[ {None: None} ]');
SELECT * FROM test_return_setof_type_record('[ {None: None} ]');

-- [ERROR] From Python List of String (one character per column)
SELECT test_return_setof_type_record('["a4"]');
SELECT * FROM test_return_setof_type_record('["a4"]');

-- Questionable Success conditions

-- From Python empty string
SELECT test_return_setof_type_record('""');
SELECT * FROM test_return_setof_type_record('""');

-- From Python empty dict
SELECT test_return_setof_type_record('{}');
SELECT * FROM test_return_setof_type_record('{}');

-- From Python Dictionary, with extra fields
SELECT test_return_setof_type_record('[{"first": "value", "second": 4, "third": "ignored"}]');
SELECT * FROM test_return_setof_type_record('[{"first": "value", "second": 4}, "third": "ignored"}]');



--
-- Case 2: From a setof Table Type
--

-- From Python empty list
SELECT test_return_setof_table_record('[]');
SELECT * FROM test_return_setof_table_record('[]');
SELECT test_return_setof_table_record('[]')
FROM gp_single_row;

SELECT (test_return_setof_table_record('[]')).*;
SELECT (test_return_setof_table_record('[]')).*
FROM gp_single_row;

-- From Python List
SELECT test_return_setof_table_record('[ None, ("value", 4), {"first": "test", "second": 2} ]');
SELECT * FROM test_return_setof_table_record('[ None, ("value", 4), {"first": "test", "second": 2} ]');
SELECT test_return_setof_table_record('[ None, ("value", 4), {"first": "test", "second": 2} ]')
FROM gp_single_row;

SELECT (test_return_setof_table_record('[ None, ("value", 4), {"first": "test", "second": 2} ]')).*;
SELECT (test_return_setof_table_record('[ None, ("value", 4), {"first": "test", "second": 2} ]')).*
FROM gp_single_row;

-- Error conditions

-- [ERROR] From Python None
SELECT test_return_setof_table_record('None');
SELECT * FROM test_return_setof_table_record('None');

-- [ERROR] From Python bool
SELECT test_return_setof_table_record('True');
SELECT * FROM test_return_setof_table_record('True');

-- [ERROR] From Python string
SELECT test_return_setof_table_record('"value"');
SELECT * FROM test_return_setof_table_record('"value"');

-- [ERROR] From Python List
SELECT test_return_setof_table_record('("value", 4)');
SELECT * FROM test_return_setof_table_record('("value", 4)');

-- [ERROR] From Python non-empty list, wrong number of columns
SELECT test_return_setof_table_record('[ [False] ]');
SELECT * FROM test_return_setof_table_record('[ [False] ]');

-- [ERROR] From Python non-empty list, wrong datatypes
SELECT test_return_setof_table_record('[ [4, "value"] ]');
SELECT * FROM test_return_setof_table_record('[ [4, "value"] ]');

-- [ERROR] From Python non-empty list, cointaining None
SELECT test_return_setof_table_record('[None]');
SELECT * FROM test_return_setof_table_record('[None]');

-- [ERROR] From Python dict without the needed columns
SELECT test_return_setof_table_record('[ {None: None} ]');
SELECT * FROM test_return_setof_table_record('[ {None: None} ]');

-- [ERROR] From Python List of String (one character per column)
SELECT test_return_setof_table_record('["a4"]');
SELECT * FROM test_return_setof_table_record('["a4"]');

-- Questionable Success conditions

-- From Python empty string
SELECT test_return_setof_table_record('""');
SELECT * FROM test_return_setof_table_record('""');

-- From Python empty dict
SELECT test_return_setof_table_record('{}');
SELECT * FROM test_return_setof_table_record('{}');

-- From Python Dictionary, with extra fields
SELECT test_return_setof_table_record('[{"first": "value", "second": 4, "third": "ignored"}]');
SELECT * FROM test_return_setof_table_record('[{"first": "value", "second": 4}, "third": "ignored"}]');



-- =====================================================
-- TEST 5:  RETURN VALUE TESTING - OUT Parameters
-- =====================================================

--
-- Case 1: RETURNS SCALAR OUT Parameters
--

-- From Python None
SELECT test_return_out_text('None');
SELECT * FROM test_return_out_text('None');
SELECT test_return_out_text('None') 
FROM gp_single_row;

-- From Python bool
SELECT test_return_out_text('True');
SELECT * FROM test_return_out_text('True');
SELECT test_return_out_text('True') 
FROM gp_single_row;

-- From Python empty string
SELECT test_return_out_text('""');
SELECT * FROM test_return_out_text('""');
SELECT test_return_out_text('""') 
FROM gp_single_row;

-- From Python string
SELECT test_return_out_text('"value"');
SELECT * FROM test_return_out_text('"value"');
SELECT test_return_out_text('"value"') 
FROM gp_single_row;

-- From Python empty list
SELECT test_return_out_text('[]');
SELECT * FROM test_return_out_text('[]');
SELECT test_return_out_text('[]') 
FROM gp_single_row;

-- From Python non-empty list
SELECT test_return_out_text('[False]');
SELECT * FROM test_return_out_text('[False]');
SELECT test_return_out_text('[False]') 
FROM gp_single_row;

-- From Python empty dict
SELECT test_return_out_text('{}');
SELECT * FROM test_return_out_text('{}');
SELECT test_return_out_text('{}') 
FROM gp_single_row;

-- From Python non-empty dict
SELECT test_return_out_text('{None: None}');
SELECT * FROM test_return_out_text('{None: None}');
SELECT test_return_out_text('{None: None}') 
FROM gp_single_row;

-- From Python Object with str() function
SELECT test_return_out_text(
    E'1; exec("class Foo:\\n  def __str__(self): return \\"test\\"", globals()); y = Foo()'
);
SELECT * FROM test_return_out_text(
    E'1; exec("class Foo:\\n  def __str__(self): return \\"test\\"", globals()); y = Foo()'
);
SELECT test_return_out_text(
    E'1; exec("class Foo:\\n  def __str__(self): return \\"test\\"", globals()); y = Foo()'
) FROM gp_single_row;


-- Error conditions:
--   There are two ways that a "text" return can fail:
--     a) There isn't a __str__ function for the returned object
--     b) The returned string contains null characters (valid in Python)

-- [ERROR] From Python String with null characters
SELECT test_return_out_text(E'"test\\0"');
SELECT * FROM test_return_out_text(E'"test\\0"');

-- [ERROR] From Python Object with bad str() function
SELECT test_return_out_text(
    E'1; exec("class Foo:\\n  def __str__(self): return None", globals()); y = Foo()'
);
SELECT * FROM test_return_out_text(
    E'1; exec("class Foo:\\n  def __str__(self): return None", globals()); y = Foo()'
);



--
-- Case 2: RETURNS RECORD
--

-- From Python None
SELECT test_return_out_record('None');
SELECT * FROM test_return_out_record('None');
SELECT test_return_out_record('None') 
FROM gp_single_row;

SELECT (test_return_out_record('None')).*;
SELECT (test_return_out_record('None')).*
FROM gp_single_row;

-- From Python List
SELECT test_return_out_record('("value", 4)');
SELECT * FROM test_return_out_record('("value", 4)');
SELECT test_return_out_record('("value", 4)')
FROM gp_single_row;

SELECT (test_return_out_record('("value", 4)')).*;
SELECT (test_return_out_record('("value", 4)')).*
FROM gp_single_row;


-- From Python Tuple
SELECT test_return_out_record('["value", 4]');
SELECT * FROM test_return_out_record('["value", 4]');
SELECT test_return_out_record('["value", 4]')
FROM gp_single_row;

SELECT (test_return_out_record('["value", 4]')).*;
SELECT (test_return_out_record('["value", 4]')).*
FROM gp_single_row;

-- From Python Dictionary
SELECT test_return_out_record('{"first": "value", "second": 4}');
SELECT * FROM test_return_out_record('{"first": "value", "second": 4}');
SELECT test_return_out_record('{"first": "value", "second": 4}')
FROM gp_single_row;

SELECT (test_return_out_record('{"first": "value", "second": 4}')).*;
SELECT (test_return_out_record('{"first": "value", "second": 4}')).*
FROM gp_single_row;

-- Error conditions

-- [ERROR] From Python bool
SELECT test_return_out_record('True');
SELECT * FROM test_return_out_record('True');

-- [ERROR] From Python empty string
SELECT test_return_out_record('""');
SELECT * FROM test_return_out_record('""');

-- [ERROR] From Python string
SELECT test_return_out_record('"value"');
SELECT * FROM test_return_out_record('"value"');

-- [ERROR] From Python non-empty list, wrong number of columns
SELECT test_return_out_record('[False]');
SELECT * FROM test_return_out_record('[False]');

-- [ERROR] From Python non-empty list, wrong datatypes
SELECT test_return_out_record('[4, "value"]');
SELECT * FROM test_return_out_record('[4, "value"]');

-- [ERROR] From Python empty dict
SELECT test_return_out_record('{}');
SELECT * FROM test_return_out_record('{}');

-- [ERROR] From Python without the needed columns
SELECT test_return_out_record('{None: None}');
SELECT * FROM test_return_out_record('{None: None}');

-- Questionable Success conditions

--  From Python String (one character per column)
SELECT test_return_out_record('"a4"');
SELECT * FROM test_return_out_record('"a4"');

-- From Python Dictionary, with extra fields
SELECT test_return_out_record('{"first": "value", "second": 4, "third": "ignored"}');
SELECT * FROM test_return_out_record('{"first": "value", "second": 4, "third": "ignored"}');

--
-- Case 3: RETURNS SETOF SCALAR
--
-- From Python Empty List
SELECT test_return_out_setof_text('[]');
SELECT * FROM test_return_out_setof_text('[]');
SELECT test_return_out_setof_text('[]') 
FROM gp_single_row;

-- From Python List of values
SELECT test_return_out_setof_text('[None, True, "value", [1], {"A": "B"}]');
SELECT * FROM test_return_out_setof_text('[None, True, "value", [1], {"A": "B"}]');
SELECT test_return_out_setof_text('[None, True, "value", [1], {"A": "B"}]') 
FROM gp_single_row;


-- Error conditions

-- [ERROR] From Python None
SELECT test_return_out_setof_text('None');
SELECT * FROM test_return_out_setof_text('None');

-- [ERROR] From Python Bool
SELECT test_return_out_setof_text('True');
SELECT * FROM test_return_out_setof_text('True');

-- [ERROR] From Python string with null values
SELECT test_return_out_setof_text(E'"test\\0"');
SELECT * FROM test_return_out_setof_text(E'"test\\0"');


-- Questionable Success conditions

-- From Python empty string
SELECT test_return_out_setof_text('""');
SELECT * FROM test_return_out_setof_text('""');

-- From Python empty dictionary
SELECT test_return_out_setof_text('{}');
SELECT * FROM test_return_out_setof_text('{}');

-- From Python string
SELECT test_return_out_setof_text('"value"');
SELECT * FROM test_return_out_setof_text('"value"');

-- From Python dictionary 
SELECT test_return_out_setof_text('{True: "ignored", False: "ignored"}');
SELECT * FROM test_return_out_setof_text('{True: "ignored", False: "ignored"}');

--
-- Case 4: RETURNS SETOF RECORD
--

-- From Python empty list
SELECT test_return_out_setof_record('[]');
SELECT * FROM test_return_out_setof_record('[]');
SELECT test_return_out_setof_record('[]')
FROM gp_single_row;

SELECT (test_return_out_setof_record('[]')).*;
SELECT (test_return_out_setof_record('[]')).*
FROM gp_single_row;

-- From Python List
SELECT test_return_out_setof_record('[ ("value", 4), {"first": "test", "second": 2} ]');
SELECT * FROM test_return_out_setof_record('[ ("value", 4), {"first": "test", "second": 2} ]');
SELECT test_return_out_setof_record('[ ("value", 4), {"first": "test", "second": 2} ]')
FROM gp_single_row;

SELECT (test_return_out_setof_record('[ ("value", 4), {"first": "test", "second": 2} ]')).*;
SELECT (test_return_out_setof_record('[ ("value", 4), {"first": "test", "second": 2} ]')).*
FROM gp_single_row;

-- Error conditions

-- [ERROR] From Python None
SELECT test_return_out_setof_record('None');
SELECT * FROM test_return_out_setof_record('None');

-- [ERROR] From Python bool
SELECT test_return_out_setof_record('True');
SELECT * FROM test_return_out_setof_record('True');

-- [ERROR] From Python string
SELECT test_return_out_setof_record('"value"');
SELECT * FROM test_return_out_setof_record('"value"');

-- [ERROR] From Python List
SELECT test_return_out_setof_record('("value", 4)');
SELECT * FROM test_return_out_setof_record('("value", 4)');

-- [ERROR] From Python non-empty list, wrong number of columns
SELECT test_return_out_setof_record('[ [False] ]');
SELECT * FROM test_return_out_setof_record('[ [False] ]');

-- [ERROR] From Python non-empty list, wrong datatypes
SELECT test_return_out_setof_record('[ [4, "value"] ]');
SELECT * FROM test_return_out_setof_record('[ [4, "value"] ]');

-- [ERROR] From Python non-empty list, containing None
SELECT test_return_out_setof_record('[ None ]');
SELECT * FROM test_return_out_setof_record('[ None ]');

-- [ERROR] From Python dict without the needed columns
SELECT test_return_out_setof_record('[ {None: None} ]');
SELECT * FROM test_return_out_setof_record('[ {None: None} ]');

-- Questionable Success conditions

-- From Python empty string
SELECT test_return_out_setof_record('""');
SELECT * FROM test_return_out_setof_record('""');

-- From Python empty dict
SELECT test_return_out_setof_record('{}');
SELECT * FROM test_return_out_setof_record('{}');

-- From Python Dictionary, with extra fields
SELECT test_return_out_setof_record('[{"first": "value", "second": 4, "third": "ignored"}]');
SELECT * FROM test_return_out_setof_record('[{"first": "value", "second": 4}, "third": "ignored"}]');

-- From Python List of String (one character per column)
SELECT test_return_out_setof_record('["a4"]');
SELECT * FROM test_return_out_setof_record('["a4"]');



--
-- Case 5: RETURNS TABLE
--


-- From Python empty list
SELECT test_return_table('[]');
SELECT * FROM test_return_table('[]');
SELECT test_return_table('[]')
FROM gp_single_row;

SELECT (test_return_table('[]')).*;
SELECT (test_return_table('[]')).*
FROM gp_single_row;

-- From Python List
SELECT test_return_table('[ ("value", 4), {"first": "test", "second": 2} ]');
SELECT * FROM test_return_table('[ ("value", 4), {"first": "test", "second": 2} ]');
SELECT test_return_table('[ None, ("value", 4), {"first": "test", "second": 2} ]')
FROM gp_single_row;

SELECT (test_return_table('[ ("value", 4), {"first": "test", "second": 2} ]')).*;
SELECT (test_return_table('[ ("value", 4), {"first": "test", "second": 2} ]')).*
FROM gp_single_row;

-- Error conditions

-- [ERROR] From Python None
SELECT test_return_table('None');
SELECT * FROM test_return_table('None');

-- [ERROR] From Python bool
SELECT test_return_table('True');
SELECT * FROM test_return_table('True');

-- [ERROR] From Python string
SELECT test_return_table('"value"');
SELECT * FROM test_return_table('"value"');

-- [ERROR] From Python List
SELECT test_return_table('("value", 4)');
SELECT * FROM test_return_table('("value", 4)');

-- [ERROR] From Python non-empty list, wrong number of columns
SELECT test_return_table('[ [False] ]');
SELECT * FROM test_return_table('[ [False] ]');

-- [ERROR] From Python non-empty list, wrong datatypes
SELECT test_return_table('[ [4, "value"] ]');
SELECT * FROM test_return_table('[ [4, "value"] ]');

-- [ERROR] From Python non-empty list, containing None
SELECT test_return_table('[ None, ["value", 4] ]');
SELECT * FROM test_return_table('[ None, ["value", 4] ]');

-- [ERROR] From Python dict without the needed columns
SELECT test_return_table('[ {None: None} ]');
SELECT * FROM test_return_table('[ {None: None} ]');

-- [ERROR] From Python List of String (one character per column)
SELECT test_return_table('["a4"]');
SELECT * FROM test_return_table('["a4"]');

-- Questionable Success conditions

-- From Python empty string
SELECT test_return_table('""');
SELECT * FROM test_return_table('""');

-- From Python empty dict
SELECT test_return_table('{}');
SELECT * FROM test_return_table('{}');

-- From Python Dictionary, with extra fields
SELECT test_return_table('[{"first": "value", "second": 4, "third": "ignored"}]');
SELECT * FROM test_return_table('[{"first": "value", "second": 4, "third": "ignored"}]');

--
-- Cleanup 
--
DROP TABLE gp_single_row;
