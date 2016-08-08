-- start_ignore
DROP SCHEMA javatest CASCADE;
CREATE SCHEMA javatest;
set search_path=javatest,public;
set pljava.classpath='examples.jar';
set log_min_messages=info;  -- XXX
create language pljava;
-- end_ignore

CREATE TABLE javatest.test AS SELECT 1 as i distributed by (i);

CREATE FUNCTION javatest.java_getTimestamp() 
  RETURNS timestamp
  AS 'org.postgresql.example.Parameters.getTimestamp' 
  LANGUAGE pljava;

  SELECT javatest.java_getTimestamp();
  SELECT javatest.java_getTimestamp() FROM javatest.test;
  SELECT * FROM javatest.java_getTimestamp();

CREATE FUNCTION javatest.java_getTimestamptz() 
  RETURNS timestamptz
  AS 'org.postgresql.example.Parameters.getTimestamp'
  LANGUAGE pljava;

  SELECT javatest.java_getTimestamptz();
  SELECT javatest.java_getTimestamptz() FROM javatest.test;
  SELECT * FROM javatest.java_getTimestamptz();

CREATE FUNCTION javatest.print(date)
  RETURNS void
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE pljava;

  SELECT javatest.print('10-10-2010'::date);
  SELECT javatest.print('10-10-2010'::date) FROM javatest.test;
  SELECT * FROM javatest.print('10-10-2010'::date);

CREATE FUNCTION javatest.print(timetz)
  RETURNS void
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE pljava;

  SELECT javatest.print('12:00 PST'::timetz);
  SELECT javatest.print('12:00 PST'::timetz) FROM javatest.test;
  SELECT * FROM javatest.print('12:00 PST'::timetz);

CREATE FUNCTION javatest.print(timestamptz)
  RETURNS void
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE pljava;

  SELECT javatest.print('12:00 PST'::timestamptz);
  SELECT javatest.print('12:00 PST'::timestamptz) FROM javatest.test;
  SELECT * FROM javatest.print('12:00 PST'::timestamptz);

CREATE FUNCTION javatest.print("char")
  RETURNS "char"
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE pljava;

  SELECT javatest.print('a'::char);
  SELECT javatest.print('a'::char) FROM javatest.test;
  SELECT * FROM javatest.print('a'::char);

CREATE FUNCTION javatest.print(bytea)
  RETURNS bytea
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE pljava;

  SELECT javatest.print('a'::bytea);
  SELECT javatest.print('a'::bytea) FROM javatest.test;
  SELECT * FROM javatest.print('a'::bytea);


CREATE FUNCTION javatest.print(int2)
  RETURNS int2
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE pljava;

  SELECT javatest.print(2::int2);
  SELECT javatest.print(2::int2) FROM javatest.test;
  SELECT * FROM javatest.print(2::int2);

CREATE FUNCTION javatest.print(int2[])
  RETURNS int2[]
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE pljava;

  SELECT javatest.print('{2}'::int2[]);
  SELECT javatest.print('{2}'::int2[]) FROM javatest.test;
  SELECT * FROM javatest.print('{2}'::int2[]]);

CREATE FUNCTION javatest.print(int4)
  RETURNS int4
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE pljava;

  SELECT javatest.print(4::int4);
  SELECT javatest.print(4::int4) FROM javatest.test;
  SELECT * FROM javatest.print(4::int4);


CREATE FUNCTION javatest.print(int4[])
  RETURNS int4[]
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE pljava;

  SELECT javatest.print('{4}'::int4[]);
  SELECT javatest.print('{4}'::int4[]) FROM javatest.test;
  SELECT * FROM javatest.print('{4}'::int4[]]);

CREATE FUNCTION javatest.print(int8)
  RETURNS int8
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE pljava;

  SELECT javatest.print(8::int8);
  SELECT javatest.print(8::int8) FROM javatest.test;
  SELECT * FROM javatest.print(8::int8);

CREATE FUNCTION javatest.print(int8[])
  RETURNS int8[]
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE pljava;

  SELECT javatest.print('{8}'::int8[]);
  SELECT javatest.print('{8}'::int8[]) FROM javatest.test;
  SELECT * FROM javatest.print('{8}'::int8[]);

CREATE FUNCTION javatest.print(real)
  RETURNS real
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE pljava;

  SELECT javatest.print(4.4::real);
  SELECT javatest.print(4.4::real) FROM javatest.test;
  SELECT * FROM javatest.print(4.4::real);

CREATE FUNCTION javatest.print(real[])
  RETURNS real[]
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE pljava;

  SELECT javatest.print('{4.4}'::real[]);
  SELECT javatest.print('{4.4}'::real[]) FROM javatest.test;
  SELECT * FROM javatest.print('{4.4}'::real[]);

CREATE FUNCTION javatest.print(double precision)
  RETURNS double precision
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE pljava;

  SELECT javatest.print(8.8::double precision);
  SELECT javatest.print(8.8::double precision) FROM javatest.test;
  SELECT * FROM javatest.print(8.8::double precision);

CREATE FUNCTION javatest.print(double precision[])
  RETURNS double precision[]
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE pljava;

  SELECT javatest.print('{8.8}'::double precision[]);
  SELECT javatest.print('{8.8}'::double precision[]) FROM javatest.test;
  SELECT * FROM javatest.print('{8.8}'::double precision[]);

CREATE FUNCTION javatest.printObj(int[])
  RETURNS int[]
  AS 'org.postgresql.example.Parameters.print(java.lang.Integer[])'
  LANGUAGE pljava;

  SELECT javatest.printObj('{4}'::int[]);
  SELECT javatest.printObj('{4}'::int[]) FROM javatest.test;
  SELECT * FROM javatest.printObj('{4}'::int[]);

CREATE FUNCTION javatest.java_addOne(int)
  RETURNS int
  AS 'org.postgresql.example.Parameters.addOne(java.lang.Integer)'
  IMMUTABLE LANGUAGE pljava;

  SELECT javatest.java_addOne(1);
  SELECT javatest.java_addOne(1) FROM javatest.test;
  SELECT * FROM javatest.java_addOne(1);

CREATE FUNCTION javatest.nullOnEven(int)
  RETURNS int
  AS 'org.postgresql.example.Parameters.nullOnEven'
  IMMUTABLE LANGUAGE pljava;

  SELECT javatest.nullOnEven(1);
  SELECT javatest.nullOnEven(2);
  SELECT javatest.nullOnEven(1) FROM javatest.test;
  SELECT javatest.nullOnEven(2) FROM javatest.test;
  SELECT * FROM javatest.nullOnEven(1);
  SELECT * FROM javatest.nullOnEven(2);

CREATE FUNCTION javatest.java_getSystemProperty(varchar)
  RETURNS varchar
  AS 'java.lang.System.getProperty'
  LANGUAGE pljava;

  SELECT javatest.java_getSystemProperty('java.home');
  SELECT javatest.java_getSystemProperty('java.home') FROM javatest.test;
  SELECT * FROM javatest.java_getSystemProperty('java.home');

/*
 * This function should fail since file system access is
 * prohibited when the language is trusted.
 */
CREATE FUNCTION javatest.create_temp_file_trusted()
  RETURNS varchar
  AS 'org.postgresql.example.Security.createTempFile'
  LANGUAGE pljava;
-- start_ignore
  SELECT javatest.create_temp_file_trusted();
  SELECT javatest.create_temp_file_trusted() FROM javatest.test;
  SELECT * FROM javatest.create_temp_file_trusted();
-- end_ignore
/*
 * XXX: GP doesn't support triggers
 *
 *   -- TRIGGER test cases deleted --
 */


CREATE FUNCTION javatest.transferPeople(int)
  RETURNS int
  AS 'org.postgresql.example.SPIActions.transferPeopleWithSalary'
  LANGUAGE pljava;

  CREATE TABLE javatest.employees1(
    id        int PRIMARY KEY,
    name      varchar(200),	
    salary    int
    );

  CREATE TABLE javatest.employees2(
    id            int PRIMARY KEY,
    name          varchar(200),
    salary        int,
    transferDay   date,
    transferTime  time
    );

  insert into employees1 values (1, 'Adam', 100);
  insert into employees1 values (2, 'Brian', 200);
  insert into employees1 values (3, 'Caleb', 300);
  insert into employees1 values (4, 'David', 400);

  SELECT javatest.transferPeople(1);
  SELECT * FROM employees1;
  SELECT * FROM employees2;
  SELECT javatest.transferPeople(1) FROM javatest.test;  -- should error

  CREATE TYPE javatest._testSetReturn
    AS (base integer, incbase integer, ctime timestamptz);

CREATE FUNCTION javatest.tupleReturnExample(int, int)
  RETURNS _testSetReturn
  AS 'org.postgresql.example.TupleReturn.tupleReturn'
  IMMUTABLE LANGUAGE pljava;

  SELECT javatest.tupleReturnExample(2,4);
  SELECT (x).* FROM (
    SELECT javatest.tupleReturnExample(2,4) as x) q;
  SELECT javatest.tupleReturnExample(2,4) FROM javatest.test;
  SELECT (x).* FROM (
    SELECT javatest.tupleReturnExample(2,4) as x FROM javatest.test) q;
  SELECT * FROM javatest.tupleReturnExample(2,4);

-- XXX: TypeMaps not supported
/*
CREATE FUNCTION javatest.tupleReturnExample2(int, int)
  RETURNS _testSetReturn
  AS 'org.postgresql.example.TupleReturn.tupleReturn(java.lang.Integer, java.lang.Inte  gejava.sql.ResultSet)'
  IMMUTABLE LANGUAGE pljava;

  SELECT javatest.tupleReturnExample2(2,4);
  SELECT javatest.tupleReturnExample2(2,4) FROM javatest.test;
  SELECT * FROM javatest.tupleReturnExample2(2,4);
*/

CREATE FUNCTION javatest.tupleReturnToString(_testSetReturn)
  RETURNS VARCHAR
  AS 'org.postgresql.example.TupleReturn.makeString'
  IMMUTABLE LANGUAGE pljava;

-- XXX: TupleDesc reference Leak (Exists in postgres too)
  SELECT javatest.tupleReturnToString(javatest.tupleReturnExample(2,4));
  SELECT javatest.tupleReturnToString(javatest.tupleReturnExample(2,4)) 
    FROM javatest.test;
  SELECT javatest.tupleReturnToString(x) 
    FROM javatest.tupleReturnExample(2,4) x;

CREATE FUNCTION javatest.setReturnExample(int, int)
  RETURNS SETOF javatest._testSetReturn
  AS 'org.postgresql.example.TupleReturn.setReturn'
  IMMUTABLE LANGUAGE pljava;

  SELECT javatest.setReturnExample(2,4);
  SELECT (x).* FROM (
    SELECT javatest.setReturnExample(2,4) as x) q;
  SELECT javatest.setReturnExample(2,4) FROM javatest.test;
  SELECT (x).* FROM (
    SELECT javatest.setReturnExample(2,4) as x FROM javatest.test) q;
  SELECT * FROM javatest.setReturnExample(2,4);

CREATE FUNCTION javatest.hugeResult(int)
  RETURNS SETOF javatest._testSetReturn
  AS 'org.postgresql.example.HugeResultSet.executeSelect'
  IMMUTABLE LANGUAGE pljava;

  SELECT javatest.hugeResult(4);
  SELECT (x).* FROM (SELECT javatest.hugeResult(4) as x) q;
  SELECT javatest.hugeResult(4) FROM javatest.test;
  SELECT (x).* FROM (SELECT javatest.hugeResult(4) as x FROM javatest.test) q;
  SELECT * FROM javatest.hugeResult(4);

CREATE FUNCTION javatest.hugeNonImmutableResult(int)
  RETURNS SETOF javatest._testSetReturn
  AS 'org.postgresql.example.HugeResultSet.executeSelect'
  LANGUAGE pljava;

  SELECT javatest.hugeNonImmutableResult(4);
  SELECT (x).* FROM (SELECT javatest.hugeNonImmutableResult(4) as x) q;
  SELECT javatest.hugeNonImmutableResult(4) FROM javatest.test;
  SELECT (x).* FROM (
    SELECT javatest.hugeNonImmutableResult(4) as x FROM javatest.test) q;
  SELECT * FROM javatest.hugeNonImmutableResult(4);

CREATE FUNCTION javatest.maxFromSetReturnExample(int, int)
  RETURNS int
  AS 'org.postgresql.example.SPIActions.maxFromSetReturnExample'
  IMMUTABLE LANGUAGE pljava;

  SELECT javatest.maxFromSetReturnExample(2,1);
  SELECT javatest.maxFromSetReturnExample(2,1) FROM javatest.test;
  SELECT * FROM javatest.maxFromSetReturnExample(2,1);

CREATE FUNCTION javatest.nestedStatements(int)
  RETURNS void
  AS 'org.postgresql.example.SPIActions.nestedStatements'
  LANGUAGE pljava;

  SELECT javatest.nestedStatements(2);
  SELECT javatest.nestedStatements(2) FROM javatest.test;
  SELECT * FROM javatest.nestedStatements(2);


  CREATE TYPE javatest._properties
    AS (name varchar(200), value varchar(200));

-- XXX: fails to fetch properties
CREATE FUNCTION javatest.propertyExample()
  RETURNS SETOF javatest._properties
  AS 'org.postgresql.example.UsingProperties.getProperties'
  IMMUTABLE LANGUAGE pljava;

-- XXX: fails to fetch properties
CREATE FUNCTION javatest.resultSetPropertyExample()
  RETURNS SETOF javatest._properties
  AS 'org.postgresql.example.UsingPropertiesAsResultSet.getProperties'
  IMMUTABLE LANGUAGE pljava;

-- XXX: fails to fetch properties
CREATE FUNCTION javatest.scalarPropertyExample()
  RETURNS SETOF varchar
  AS 'org.postgresql.example.UsingPropertiesAsScalarSet.getProperties'
  IMMUTABLE LANGUAGE pljava;

CREATE FUNCTION javatest.randomInts(int)
  RETURNS SETOF int
  AS 'org.postgresql.example.RandomInts.createIterator'
  IMMUTABLE LANGUAGE pljava;

  SELECT javatest.randomInts(3);
  SELECT javatest.randomInts(3) FROM javatest.test;
  SELECT * FROM javatest.randomInts(3);

CREATE FUNCTION javatest.listSupers()
  RETURNS SETOF pg_user
  AS 'org.postgresql.example.Users.listSupers'
  LANGUAGE pljava;

  SELECT javatest.listSupers();
  SELECT (x).* FROM (SELECT javatest.listSupers() as x) q;
  SELECT javatest.listSupers() FROM javatest.test;
  SELECT (x).* FROM (SELECT javatest.listSupers() as x FROM javatest.test) q;
  SELECT * FROM javatest.listSupers();

CREATE FUNCTION javatest.listNonSupers()
  RETURNS SETOF pg_user
  AS 'org.postgresql.example.Users.listNonSupers'
  LANGUAGE pljava;

  SELECT javatest.listNonSupers();
  SELECT (x).* FROM (SELECT javatest.listNonSupers() as x) q;
  SELECT javatest.listNonSupers() FROM javatest.test;
  SELECT (x).* FROM (SELECT javatest.listNonSupers() as x FROM javatest.test) q;
  SELECT * FROM javatest.listNonSupers();

CREATE FUNCTION javatest.testSavepointSanity()
  RETURNS int
  AS 'org.postgresql.example.SPIActions.testSavepointSanity'
  IMMUTABLE LANGUAGE pljava;

CREATE FUNCTION javatest.testTransactionRecovery()
  RETURNS int
  AS 'org.postgresql.example.SPIActions.testTransactionRecovery'
  IMMUTABLE LANGUAGE pljava;

CREATE FUNCTION javatest.getDateAsString()
  RETURNS varchar
  AS 'org.postgresql.example.SPIActions.getDateAsString'
  STABLE LANGUAGE pljava;

  SELECT javatest.getDateAsString();
  SELECT javatest.getDateAsString() FROM javatest.test;
  SELECT * FROM javatest.getDateAsString();

CREATE FUNCTION javatest.getTimeAsString()
  RETURNS varchar
  AS 'org.postgresql.example.SPIActions.getTimeAsString'
  STABLE LANGUAGE pljava;

  SELECT javatest.getTimeAsString();
  SELECT javatest.getTimeAsString() FROM javatest.test;
  SELECT * FROM javatest.getTimeAsString();

-- Test LOGGING
CREATE FUNCTION javatest.logMessage(varchar, varchar)
  RETURNS void
  AS 'org.postgresql.example.LoggerTest.logMessage'
  IMMUTABLE LANGUAGE pljava;
  
  SELECT javatest.logMessage('SEVERE', 'hello');
  SELECT javatest.logMessage('WARNING', 'hello');
  SELECT javatest.logMessage('INFO', 'hello');
  SELECT javatest.logMessage('FINEST', 'hello'); -- not (usually) logged

  CREATE TYPE javatest.BinaryColumnPair
    AS (col1 bytea, col2 bytea);

CREATE FUNCTION javatest.binaryColumnTest()
  RETURNS SETOF javatest.BinaryColumnPair
  AS 'org.postgresql.example.BinaryColumnTest.getBinaryPairs'
  IMMUTABLE LANGUAGE pljava;

  SELECT javatest.binaryColumnTest();
  SELECT javatest.binaryColumnTest() FROM javatest.test;

  CREATE TYPE javatest.MetaDataBooleans
    AS (method_name varchar(200), result boolean);

CREATE FUNCTION javatest.getMetaDataBooleans()
  RETURNS SETOF javatest.MetaDataBooleans
  AS 'org.postgresql.example.MetaDataBooleans.getDatabaseMetaDataBooleans'
  LANGUAGE pljava;

  SELECT javatest.getMetaDataBooleans();
  SELECT javatest.getMetaDataBooleans() FROM javatest.test;
  SELECT * FROM javatest.getMetaDataBooleans();

  CREATE TYPE javatest.MetaDataStrings
    AS (method_name varchar(200), result varchar);

CREATE FUNCTION javatest.getMetaDataStrings()
  RETURNS SETOF javatest.MetaDataStrings
  AS 'org.postgresql.example.MetaDataStrings.getDatabaseMetaDataStrings'
  LANGUAGE pljava;

  SELECT javatest.getMetaDataStrings();
  SELECT javatest.getMetaDataStrings() FROM javatest.test;
  SELECT * FROM javatest.getMetaDataStrings();

  CREATE TYPE javatest.MetaDataInts
    AS (method_name varchar(200), result int);

CREATE FUNCTION javatest.getMetaDataInts()
  RETURNS SETOF javatest.MetaDataInts
  AS 'org.postgresql.example.MetaDataInts.getDatabaseMetaDataInts'
  LANGUAGE pljava;

  SELECT javatest.getMetaDataInts();
  SELECT javatest.getMetaDataInts() FROM javatest.test;
  SELECT * FROM javatest.getMetaDataInts();

CREATE FUNCTION javatest.callMetaDataMethod(varchar)
  RETURNS SETOF varchar
  AS 'org.postgresql.example.MetaDataTest.callMetaDataMethod'
  LANGUAGE pljava;

  SELECT * from callMetaDataMethod('getTables((String)null,"javatest","%",{"TABLE"})');

CREATE FUNCTION javatest.executeSelect(varchar)
  RETURNS SETOF VARCHAR
  AS 'org.postgresql.example.ResultSetTest.executeSelect'
  LANGUAGE pljava;

  SELECT javatest.executeSelect('select * from javatest.test');
  SELECT javatest.executeSelect('select * from javatest.test') 
    FROM javatest.test; -- expected to error
  SELECT javatest.executeSelect(
    'select oid, relname from pg_class where relname=''pg_class''');
  SELECT javatest.executeSelect(
    'select oid, relname from pg_class where relname=''pg_class''')
    FROM javatest.test;

-- called in context that cannot accept a set
create function mytest(q text) returns setof record AS $$ 
DECLARE
  row RECORD;
BEGIN 
  FOR row IN EXECUTE q LOOP
    RETURN NEXT row; 
  END LOOP; 
END;
$$ language plpgsql;

drop function mytest(text);
create function mytest(q text) returns record AS $$ 
DECLARE
  row RECORD;
BEGIN 
  FOR row IN EXECUTE q LOOP
    RETURN row; 
  END LOOP; 
END;
$$ language plpgsql;


CREATE FUNCTION javatest.executeSelectToRecords(varchar)
  RETURNS SETOF RECORD
  AS 'org.postgresql.example.SetOfRecordTest.executeSelect'
  LANGUAGE pljava;

-- XXX - SIGBUS (existing problem with pljava)
/*
  SELECT javatest.executeSelectToRecords('select * from javatest.test');
  SELECT javatest.executeSelectToRecords('select * from javatest.test') 
    FROM javatest.test; -- expected to error
  SELECT javatest.executeSelectToRecords(
    'select oid, relname from pg_class where relname=''pg_class''');
  SELECT javatest.executeSelectToRecords(
    'select oid, relname from pg_class where relname=''pg_class''')
    FROM javatest.test;
  SELECT * FROM javatest.executeSelectToRecords(
    'select * from javatest.test');  -- expected to error
*/
  SELECT * FROM javatest.executeSelectToRecords(
    'select * from javatest.test') test(i int);


CREATE FUNCTION javatest.countNulls(record)
  RETURNS int
  AS 'org.postgresql.example.Parameters.countNulls'
  LANGUAGE pljava;


-- XXX: TupleDesc leak  (Exists in postgres too)
CREATE FUNCTION javatest.countNulls(int[])
  RETURNS int
  AS 'org.postgresql.example.Parameters.countNulls(java.lang.Integer[])'
  LANGUAGE pljava;

  SELECT javatest.countNulls(row(1,null,2,null,3));
  SELECT javatest.countNulls(row(1,null,2,null,3)) FROM javatest.test;
  SELECT * FROM javatest.countNulls(row(1,null,2,null,3));


  /* Here is an example of a scalar type that maps to a Java class. */
  /* Create the dummy shell */ 
CREATE TYPE javatest.complex;

  /* The scalar input function */
CREATE FUNCTION javatest.complex_in(cstring)
  RETURNS javatest.complex
  AS 'UDT[org.postgresql.example.ComplexScalar] input'
  LANGUAGE pljava IMMUTABLE STRICT;

  /* The scalar output function */
CREATE FUNCTION javatest.complex_out(javatest.complex)
  RETURNS cstring
  AS 'UDT[org.postgresql.example.ComplexScalar] output'
  LANGUAGE pljava IMMUTABLE STRICT;

  /* The scalar receive function */
CREATE FUNCTION javatest.complex_recv(internal)
  RETURNS javatest.complex
  AS 'UDT[org.postgresql.example.ComplexScalar] receive'
  LANGUAGE pljava IMMUTABLE STRICT;

  /* The scalar send function */
CREATE FUNCTION javatest.complex_send(javatest.complex)
  RETURNS bytea
  AS 'UDT[org.postgresql.example.ComplexScalar] send'
  LANGUAGE pljava IMMUTABLE STRICT;

  /* The scalar type declaration */
  CREATE TYPE javatest.complex (
    internallength = 16,
    input = javatest.complex_in,
    output = javatest.complex_out,
    receive = javatest.complex_recv,
    send = javatest.complex_send,
    alignment = double
  );

  create table javatest.complextest(x javatest.complex);
  insert into javatest.complextest values('(1,2)');
  select * from javatest.complextest;

		/* A test function that just logs and returns its argument.
		 */
CREATE FUNCTION javatest.logcomplex(javatest.complex)
  RETURNS javatest.complex		
  AS 'org.postgresql.example.ComplexScalar.logAndReturn'
  LANGUAGE pljava IMMUTABLE STRICT;

-- XXX: TypeMaps not supported
/*
		-- Here's an example of a tuple based UDT that maps to a Java class.
		CREATE TYPE javatest.complextuple AS (x float8, y float8);

		-- Install the actual type mapping.
		SELECT sqlj.add_type_mapping('javatest.complextuple', 'org.postgresql.example.ComplexTuple');

		-- test function that just logs and returns its argument.
CREATE FUNCTION javatest.logcomplex(javatest.complextuple)
  RETURNS javatest.complextuple		
  AS 'org.postgresql.example.ComplexTuple.logAndReturn'
  LANGUAGE pljava IMMUTABLE STRICT;
*/

CREATE FUNCTION javatest.loganyelement(anyelement)
  RETURNS anyelement
  AS 'org.postgresql.example.AnyTest.logAnyElement'
  LANGUAGE pljava IMMUTABLE STRICT;

  SELECT javatest.loganyelement(1::smallint);
  SELECT javatest.loganyelement(1::int);
  SELECT javatest.loganyelement(1::bigint);
  SELECT javatest.loganyelement(1::float4);
  SELECT javatest.loganyelement(1::float8);
  SELECT javatest.loganyelement(1::numeric);
  SELECT javatest.loganyelement('1'::text);
  SELECT javatest.loganyelement('1'::bytea);  -- odd output format in postgres
  SELECT javatest.loganyelement('(0,0)'::point);
  SELECT javatest.loganyelement(i) FROM javatest.test;
  SELECT * FROM javatest.loganyelement(1);


CREATE FUNCTION javatest.logany("any")
  RETURNS void
  AS 'org.postgresql.example.AnyTest.logAny'
  LANGUAGE pljava IMMUTABLE STRICT;

  SELECT javatest.logany(1);
  SELECT javatest.logany(i) FROM javatest.test;
  SELECT * FROM javatest.logany(1);

CREATE FUNCTION javatest.makearray(anyelement)
  RETURNS anyarray
  AS 'org.postgresql.example.AnyTest.makeArray'
  LANGUAGE pljava IMMUTABLE STRICT;

  SELECT javatest.makearray(1);
  SELECT javatest.makearray(i) FROM javatest.test;
  SELECT * FROM javatest.makearray(1);
