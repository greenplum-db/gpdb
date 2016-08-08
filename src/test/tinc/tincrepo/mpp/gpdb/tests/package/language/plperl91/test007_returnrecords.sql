----Case 0 begin----
-- 
-- 			CREATE OR REPLACE FUNCTION functionname() RETURN RECORD;
-- 			CREATE OR REPLACE FUNCTION functionname(OUT column1 b, OUT column2 b, ...) RETURN RECORD.
-- 		
CREATE or REPLACE function pltest.PLPERLreturnrecord_0 (  ) RETURNS RECORD as $$  
				my $rv = spi_exec_query('select * from pltest.tupleCOMPOSITE_BIT ORDER BY ID1');
				return $rv->{rows}[0];
$$ language PLPERL;

-- BUG select pltest.PLPERLreturnrecord_0() from pltest.tableTEXT;

\echo '--ERROR:  function returning record called in context that cannot accept type record'
select pltest.PLPERLreturnrecord_0();

\echo '--ERROR:  record type has not been registered'
select (pltest.PLPERLreturnrecord_0()).*;

\echo '--ERROR:  a column definition list is required for functions returning "record"'
select * from pltest.PLPERLreturnrecord_0();

select * from pltest.PLPERLreturnrecord_0() f(id1 SMALLINT,id2 BIT(8),id3 BIT VARYING(10),id4 BOOLEAN,id5 BYTEA);

DROP function pltest.PLPERLreturnrecord_0 (  );
----Case 0 end----


----Case 2 begin----
-- 	Column is less than target data type
-- 			CREATE OR REPLACE FUNCTION functionname() RETURN RECORD;
-- 			CREATE OR REPLACE FUNCTION functionname(OUT column1 b, OUT column2 b, ...) RETURN RECORD.
		
CREATE or REPLACE function pltest.PLPERLreturnrecord_2 (  ) RETURNS RECORD as $$  
				my $rv = spi_exec_query('select id1 from pltest.tupleCOMPOSITE_INTERNET ORDER BY id1');
				return $rv->{rows}[0];
$$ language PLPERL;

-- BUG select pltest.PLPERLreturnrecord_2() from pltest.tableTEXT;

select * from pltest.PLPERLreturnrecord_2() f(id1 SMALLINT,id2 INET,id3 CIDR,id4 MACADDR);

DROP function pltest.PLPERLreturnrecord_2 (  );

----Case 2 end----


----Case 4 begin----
-- Column is larger than target data type
--
--                      CREATE OR REPLACE FUNCTION functionname() RETURN RECORD;
--                      CREATE OR REPLACE FUNCTION functionname(OUT column1 b, OUT column2 b, ...) RETURN RECORD.
--
CREATE or REPLACE function pltest.PLPERLreturnrecord_4 (  ) RETURNS RECORD as $$
                                my $rv = spi_exec_query('select id1,* from pltest.tupleCOMPOSITE_DATE');
                                return $rv->{rows}[0];
$$ language PLPERL;

-- BUG select pltest.PLPERLreturnrecord_4() from pltest.tableTEXT;

\echo '--ERROR:  Perl hash contains nonexistent column "id5"'
select * from pltest.PLPERLreturnrecord_4() f(id1 DATE, id1_dup DATE,id2 TIME,id3 TIMETZ,id4 TIMESTAMP);

DROP function pltest.PLPERLreturnrecord_4 (  );



----Case 1 begin----
-- 
-- 			CREATE OR REPLACE FUNCTION functionname() RETURN RECORD;
-- 			CREATE OR REPLACE FUNCTION functionname(OUT column1 b, OUT column2 b, ...) RETURN RECORD.
-- 		
CREATE or REPLACE function pltest.PLPERLreturnrecord_1 ( OUT id1 SMALLINT , OUT id2 BIT(8) , OUT id3 BIT VARYING(10) , OUT id4 BOOLEAN , OUT id5 BYTEA ) RETURNS RECORD as $$  
				my $rv = spi_exec_query('select * from pltest.tupleCOMPOSITE_BIT ORDER BY id1');
				return $rv->{rows}[0];
$$ language PLPERL;

-- BUG select pltest.PLPERLreturnrecord_1() from pltest.tableTEXT;

select pltest.PLPERLreturnrecord_1();

select (pltest.PLPERLreturnrecord_1()).*;

select * from pltest.PLPERLreturnrecord_1();


DROP function pltest.PLPERLreturnrecord_1 ( OUT id1 SMALLINT , OUT id2 BIT(8) , OUT id3 BIT VARYING(10) , OUT id4 BOOLEAN , OUT id5 BYTEA );
----Case 1 end----



----Case 3 begin----
-- Column is less than target data type
-- 			CREATE OR REPLACE FUNCTION functionname() RETURN RECORD;
-- 			CREATE OR REPLACE FUNCTION functionname(OUT column1 b, OUT column2 b, ...) RETURN RECORD.
-- 		
CREATE or REPLACE function pltest.PLPERLreturnrecord_3 ( OUT id1 SMALLINT , OUT id2 INET , OUT id3 CIDR , OUT id4 MACADDR ) RETURNS RECORD as $$  
				my $rv = spi_exec_query('select id1 from pltest.tupleCOMPOSITE_INTERNET order by id1');
				return $rv->{rows}[0];
$$ language PLPERL;

-- BUG select pltest.PLPERLreturnrecord_3() from pltest.tableTEXT;

select pltest.PLPERLreturnrecord_3();

select (pltest.PLPERLreturnrecord_3()).*;

select * from pltest.PLPERLreturnrecord_3();

DROP function pltest.PLPERLreturnrecord_3 ( OUT id1 SMALLINT , OUT id2 INET , OUT id3 CIDR , OUT id4 MACADDR );
----Case 3 end----



----Case 5 begin----
-- Column is larger than target data type
-- 			CREATE OR REPLACE FUNCTION functionname(OUT column1 b, OUT column2 b, ...) RETURN RECORD.
-- 		
CREATE or REPLACE function pltest.PLPERLreturnrecord_5( OUT id1 DATE , OUT id2 TIME , OUT id3 TIMETZ ) RETURNS RECORD as $$  
				my $rv = spi_exec_query('select * from pltest.tupleCOMPOSITE_DATE');
				return $rv->{rows}[0];
$$ language PLPERL;

-- BUG select pltest.PLPERLreturnrecord_5() from pltest.tableTEXT;

\echo '--ERROR:  Perl hash contains nonexistent column "id5"'
select pltest.PLPERLreturnrecord_5();
\echo '--ERROR:  Perl hash contains nonexistent column "id5"'
select (pltest.PLPERLreturnrecord_5()).*;
\echo '--ERROR:  Perl hash contains nonexistent column "id5"'
select * from pltest.PLPERLreturnrecord_5();

DROP function pltest.PLPERLreturnrecord_5 ( OUT id1 DATE , OUT id2 TIME , OUT id3 TIMETZ );
----Case 5 end----
