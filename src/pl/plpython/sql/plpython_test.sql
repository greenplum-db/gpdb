-- first some tests of basic functionality
--
-- better succeed
--
select stupid();

-- check static and global data
--
SELECT static_test();
SELECT static_test();
SELECT global_test_one();
SELECT global_test_two();

-- import python modules
--
SELECT import_fail();
SELECT import_succeed();

-- test import and simple argument handling
--
SELECT import_test_one('sha hash of this string');

-- test import and tuple argument handling
--
select import_test_two(users) from users where fname = 'willem';

-- test multiple arguments
--
select argument_test_one(users, fname, lname) from users where lname = 'doe' order by 1;


-- spi and nested calls
--
select nested_call_one('pass this along');
select spi_prepared_plan_test_one('doe');
select spi_prepared_plan_test_one('smith');
select spi_prepared_plan_test_nested('smith');

-- quick peek at the table
--
SELECT * FROM users order by userid;

-- should fail
--
UPDATE users SET fname = 'william' WHERE fname = 'willem';

-- should modify william to willem and create username
--
INSERT INTO users (fname, lname) VALUES ('william', 'smith');
INSERT INTO users (fname, lname, username) VALUES ('charles', 'darwin', 'beagle');

SELECT * FROM users order by userid;

-- Greenplum doesn't support functions that execute SQL from segments
--
--  SELECT join_sequences(sequences) FROM sequences;
--  SELECT join_sequences(sequences) FROM sequences
--  	WHERE join_sequences(sequences) ~* '^A';
--  SELECT join_sequences(sequences) FROM sequences
--  	WHERE join_sequences(sequences) ~* '^B';

SELECT result_nrows_test();

-- test of exception handling
SELECT queryexec('SELECT 2'); 

SELECT queryexec('SELECT X'); 


select module_contents();

SELECT elog_test();


-- error in trigger
--

--
-- Check Universal Newline Support
--

SELECT newline_lf();
SELECT newline_cr();
SELECT newline_crlf();

-- Tests for functions returning void
SELECT test_void_func1(), test_void_func1() IS NULL AS "is null";
SELECT test_void_func2(); -- should fail
SELECT test_return_none(), test_return_none() IS NULL AS "is null";

-- Test for functions with named and nameless parameters
SELECT test_param_names0(2,7);
SELECT test_param_names1(1,'text');
SELECT test_param_names2(users) from users;
SELECT test_param_names3(1);

-- Test set returning functions
SELECT test_setof_as_list(0, 'list');
SELECT test_setof_as_list(1, 'list');
SELECT test_setof_as_list(2, 'list');
SELECT test_setof_as_list(2, null);

SELECT test_setof_as_tuple(0, 'tuple');
SELECT test_setof_as_tuple(1, 'tuple');
SELECT test_setof_as_tuple(2, 'tuple');
SELECT test_setof_as_tuple(2, null);

SELECT test_setof_as_iterator(0, 'list');
SELECT test_setof_as_iterator(1, 'list');
SELECT test_setof_as_iterator(2, 'list');
SELECT test_setof_as_iterator(2, null);

SELECT test_setof_spi_in_iterator();

-- Test tuple returning functions
SELECT * FROM test_table_record_as('dict', null, null, false);
SELECT * FROM test_table_record_as('dict', 'one', null, false);
SELECT * FROM test_table_record_as('dict', null, 2, false);
SELECT * FROM test_table_record_as('dict', 'three', 3, false);
SELECT * FROM test_table_record_as('dict', null, null, true);

SELECT * FROM test_table_record_as('tuple', null, null, false);
SELECT * FROM test_table_record_as('tuple', 'one', null, false);
SELECT * FROM test_table_record_as('tuple', null, 2, false);
SELECT * FROM test_table_record_as('tuple', 'three', 3, false);
SELECT * FROM test_table_record_as('tuple', null, null, true);

SELECT * FROM test_table_record_as('list', null, null, false);
SELECT * FROM test_table_record_as('list', 'one', null, false);
SELECT * FROM test_table_record_as('list', null, 2, false);
SELECT * FROM test_table_record_as('list', 'three', 3, false);
SELECT * FROM test_table_record_as('list', null, null, true);

SELECT * FROM test_table_record_as('obj', null, null, false);
SELECT * FROM test_table_record_as('obj', 'one', null, false);
SELECT * FROM test_table_record_as('obj', null, 2, false);
SELECT * FROM test_table_record_as('obj', 'three', 3, false);
SELECT * FROM test_table_record_as('obj', null, null, true);

SELECT * FROM test_type_record_as('dict', null, null, false);
SELECT * FROM test_type_record_as('dict', 'one', null, false);
SELECT * FROM test_type_record_as('dict', null, 2, false);
SELECT * FROM test_type_record_as('dict', 'three', 3, false);
SELECT * FROM test_type_record_as('dict', null, null, true);

SELECT * FROM test_type_record_as('tuple', null, null, false);
SELECT * FROM test_type_record_as('tuple', 'one', null, false);
SELECT * FROM test_type_record_as('tuple', null, 2, false);
SELECT * FROM test_type_record_as('tuple', 'three', 3, false);
SELECT * FROM test_type_record_as('tuple', null, null, true);

SELECT * FROM test_type_record_as('list', null, null, false);
SELECT * FROM test_type_record_as('list', 'one', null, false);
SELECT * FROM test_type_record_as('list', null, 2, false);
SELECT * FROM test_type_record_as('list', 'three', 3, false);
SELECT * FROM test_type_record_as('list', null, null, true);

SELECT * FROM test_type_record_as('obj', null, null, false);
SELECT * FROM test_type_record_as('obj', 'one', null, false);
SELECT * FROM test_type_record_as('obj', null, 2, false);
SELECT * FROM test_type_record_as('obj', 'three', 3, false);
SELECT * FROM test_type_record_as('obj', null, null, true);

SELECT * FROM test_in_out_params('test_in');
-- this doesn't work yet :-(
SELECT * FROM test_in_out_params_multi('test_in');
SELECT * FROM test_inout_params('test_in');

SELECT * FROM test_type_conversion_bool(true);
SELECT * FROM test_type_conversion_bool(false);
SELECT * FROM test_type_conversion_bool(null);
SELECT * FROM test_type_conversion_char('a');
SELECT * FROM test_type_conversion_char(null);
SELECT * FROM test_type_conversion_int2(100::int2);
SELECT * FROM test_type_conversion_int2(null);
SELECT * FROM test_type_conversion_int4(100);
SELECT * FROM test_type_conversion_int4(null);
SELECT * FROM test_type_conversion_int8(100);
SELECT * FROM test_type_conversion_int8(null);
SELECT * FROM test_type_conversion_float4(100);
SELECT * FROM test_type_conversion_float4(null);
SELECT * FROM test_type_conversion_float8(100);
SELECT * FROM test_type_conversion_float8(null);
SELECT * FROM test_type_conversion_numeric(100);
SELECT * FROM test_type_conversion_numeric(null);
SELECT * FROM test_type_conversion_text('hello world');
SELECT * FROM test_type_conversion_text(null);
SELECT * FROM test_type_conversion_bytea('hello world');
SELECT * FROM test_type_conversion_bytea(null);
-- 1-dimensional arrays
SELECT * FROM test_type_conversion_array_int4(array[1,2,null,3,4]::int4[]);
SELECT * FROM test_type_conversion_array_int4(null);
SELECT * FROM test_type_conversion_array_numeric(array[null,1.23,2.34,3.45,null]::numeric[]);
SELECT * FROM test_type_conversion_array_text(array['abc','def','ghij',null]::text[]);
SELECT * FROM test_type_conversion_array_text(null);
-- Multi-dimensional arrays
SELECT a, array_dims(a) FROM test_type_conversion_array_int4(array[
        array[1,2,3,4], array[5,null,7,8], array[null,null,11,12]
    ]::int4[]) as a;
SELECT a, array_dims(a) FROM test_type_conversion_array_int4(array[
    array[ array[1,2,3,4], array[5,null,7,8], array[null,null,11,12] ],
    array[ array[13,null,15,16], array[17,18,19,20], array[null,null,23,null] ],
    array[ array[25,26,27,null], array[29,null,null,32], array[null,null,null,null] ]
    ]::int4[]) as a;
SELECT a, array_dims(a) FROM test_type_conversion_array_numeric(array[
        array[null,1.23,2.34,3.45,null],
        array[4.56,5.67,null,null,6.78]
    ]::numeric[]) as a;
SELECT a, array_dims(a) FROM test_type_conversion_array_text(array[
        array['abc','def','ghij',null],
        array[null,'kl','mnopq','rst'],
        array['uvw','',null,'xyz']
    ]::text[]) as a;
SELECT a, array_dims(a) FROM test_type_conversion_array_text(array[
        array[ array['abc','def','ghij',null],
               array[null,'kl','mnopq','rst'],
               array['uvw','',null,'xyz'] ],
        array[ array['A','BCD',null,'EFG'],
               array[null,'HIJK','LMN','OPQR'],
               array['STUV','WXYZ',null,null] ]
    ]::text[]) as a;

SELECT test_type_unmarshal(x) FROM test_type_marshal() x;

SELECT (split(10)).*; 

SELECT * FROM split(100); 

select unnamed_tuple_test(); 

select named_tuple_test(); 

select oneline() 
union all 
select oneline2()
union all 
select multiline() 
union all 
select multiline2() 
union all 
select multiline3()
