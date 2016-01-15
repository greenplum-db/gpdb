CREATE FUNCTION pljava_call_handler()  RETURNS language_handler AS 'pljava' LANGUAGE C;
CREATE FUNCTION pljavau_call_handler() RETURNS language_handler AS 'pljava' LANGUAGE C;
CREATE TRUSTED LANGUAGE java HANDLER pljava_call_handler;
CREATE LANGUAGE javaU HANDLER pljavau_call_handler;
\c test test
DROP SCHEMA javatest CASCADE;
alter database test owner to test;
CREATE SCHEMA javatest;
set search_path=javatest,public;
#set pljava.classpath='examples.jar';
set log_min_messages=info;  -- XXX

CREATE TABLE javatest.test AS SELECT 1 as i distributed by (i);

CREATE FUNCTION javatest.print("char")
  RETURNS "char"
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE java;

  SELECT javatest.print('a'::char);
  SELECT javatest.print('a'::char) FROM javatest.test;
  SELECT * FROM javatest.print('a'::char);

CREATE FUNCTION javatest.print(bytea)
  RETURNS bytea
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE java;

  SELECT javatest.print('a'::bytea);
  SELECT javatest.print('a'::bytea) FROM javatest.test;
  SELECT * FROM javatest.print('a'::bytea);


CREATE FUNCTION javatest.print(int2)
  RETURNS int2
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE java;

  SELECT javatest.print(2::int2);
  SELECT javatest.print(2::int2) FROM javatest.test;
  SELECT * FROM javatest.print(2::int2);

CREATE FUNCTION javatest.print(int2[])
  RETURNS int2[]
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE java;

  SELECT javatest.print('{2}'::int2[]);
  SELECT javatest.print('{2}'::int2[]) FROM javatest.test;
  SELECT * FROM javatest.print('{2}'::int2[]);

CREATE FUNCTION javatest.print(int4)
  RETURNS int4
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE java;

  SELECT javatest.print(4::int4);
  SELECT javatest.print(4::int4) FROM javatest.test;
  SELECT * FROM javatest.print(4::int4);


CREATE FUNCTION javatest.print(int4[])
  RETURNS int4[]
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE java;

  SELECT javatest.print('{4}'::int4[]);
  SELECT javatest.print('{4}'::int4[]) FROM javatest.test;
  SELECT * FROM javatest.print('{4}'::int4[]);

CREATE FUNCTION javatest.print(int8)
  RETURNS int8
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE java;

  SELECT javatest.print(8::int8);
  SELECT javatest.print(8::int8) FROM javatest.test;
  SELECT * FROM javatest.print(8::int8);

CREATE FUNCTION javatest.print(int8[])
  RETURNS int8[]
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE java;

  SELECT javatest.print('{8}'::int8[]);
  SELECT javatest.print('{8}'::int8[]) FROM javatest.test;
  SELECT * FROM javatest.print('{8}'::int8[]);

CREATE FUNCTION javatest.print(real)
  RETURNS real
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE java;

  SELECT javatest.print(4.4::real);
  SELECT javatest.print(4.4::real) FROM javatest.test;
  SELECT * FROM javatest.print(4.4::real);

CREATE FUNCTION javatest.print(real[])
  RETURNS real[]
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE java;

  SELECT javatest.print('{4.4}'::real[]);
  SELECT javatest.print('{4.4}'::real[]) FROM javatest.test;
  SELECT * FROM javatest.print('{4.4}'::real[]);

CREATE FUNCTION javatest.print(double precision)
  RETURNS double precision
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE java;

  SELECT javatest.print(8.8::double precision);
  SELECT javatest.print(8.8::double precision) FROM javatest.test;
  SELECT * FROM javatest.print(8.8::double precision);

CREATE FUNCTION javatest.print(double precision[])
  RETURNS double precision[]
  AS 'org.postgresql.example.Parameters.print'
  LANGUAGE java;

  SELECT javatest.print('{8.8}'::double precision[]);
  SELECT javatest.print('{8.8}'::double precision[]) FROM javatest.test;
  SELECT * FROM javatest.print('{8.8}'::double precision[]);

CREATE FUNCTION javatest.printObj(int[])
  RETURNS int[]
  AS 'org.postgresql.example.Parameters.print(java.lang.Integer[])'
  LANGUAGE java;

  SELECT javatest.printObj('{4}'::int[]);
  SELECT javatest.printObj('{4}'::int[]) FROM javatest.test;
  SELECT * FROM javatest.printObj('{4}'::int[]);

CREATE FUNCTION javatest.java_addOne(int)
  RETURNS int
  AS 'org.postgresql.example.Parameters.addOne(java.lang.Integer)'
  IMMUTABLE LANGUAGE java;

  SELECT javatest.java_addOne(1);
  SELECT javatest.java_addOne(1) FROM javatest.test;
  SELECT * FROM javatest.java_addOne(1);

CREATE FUNCTION javatest.nullOnEven(int)
  RETURNS int
  AS 'org.postgresql.example.Parameters.nullOnEven'
  IMMUTABLE LANGUAGE java;

  SELECT javatest.nullOnEven(1);
  SELECT javatest.nullOnEven(2);
  SELECT javatest.nullOnEven(1) FROM javatest.test;
  SELECT javatest.nullOnEven(2) FROM javatest.test;
  SELECT * FROM javatest.nullOnEven(1);
  SELECT * FROM javatest.nullOnEven(2);

/*
 * This function should fail since file system access is
 * prohibited when the language is trusted.
 */
CREATE FUNCTION javatest.create_temp_file_trusted()
  RETURNS varchar
  AS 'org.postgresql.example.Security.createTempFile'
  LANGUAGE java;

  SELECT javatest.create_temp_file_trusted();
  SELECT javatest.create_temp_file_trusted() FROM javatest.test;
  SELECT * FROM javatest.create_temp_file_trusted();
 


CREATE FUNCTION javatest.transferPeople(int)
  RETURNS int
  AS 'org.postgresql.example.SPIActions.transferPeopleWithSalary'
  LANGUAGE java;

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
  SELECT id,name, salary FROM employees2;
  SELECT javatest.transferPeople(1) FROM javatest.test;  -- should error
