
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_0 ( IN  REAL ) RETURNS REAL as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_0(pltest.tableREAL.key) AS ID from pltest.tableREAL ORDER BY ID;

create table pltest.tableREAL1 as select pltest.PLPERLinpara_unname_0(pltest.tableREAL.key) AS ID from pltest.tableREAL DISTRIBUTED randomly;

select * from pltest.tableREAL1 ORDER BY ID;

drop table pltest.tableREAL1;

select pltest.PLPERLinpara_unname_0(39.333::REAL);

select pltest.PLPERLinpara_unname_0('NAN'::REAL);

select pltest.PLPERLinpara_unname_0('-INFINITY'::REAL);

select pltest.PLPERLinpara_unname_0('+INFINITY'::REAL);

select pltest.PLPERLinpara_unname_0(NULL);

select * from pltest.PLPERLinpara_unname_0(39.333::REAL);

select * from pltest.PLPERLinpara_unname_0('NAN'::REAL);

select * from pltest.PLPERLinpara_unname_0('-INFINITY'::REAL);

select * from pltest.PLPERLinpara_unname_0('+INFINITY'::REAL);

select * from pltest.PLPERLinpara_unname_0(NULL);

DROP function pltest.PLPERLinpara_unname_0 ( IN  REAL );
----Case 0 end----


----Case 1 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_1 ( IN  BOX ) RETURNS BOX as $$     
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinpara_unname_1('((1,2),(3,4))'::BOX);

select pltest.PLPERLinpara_unname_1(NULL);

select * from pltest.PLPERLinpara_unname_1('((1,2),(3,4))'::BOX);

select * from pltest.PLPERLinpara_unname_1(NULL);

DROP function pltest.PLPERLinpara_unname_1 ( IN  BOX );
----Case 1 end----


----Case 2 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_2 ( IN  VARCHAR(16) ) RETURNS VARCHAR(16) as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_2(pltest.tableVARCHAR16.key) AS ID from pltest.tableVARCHAR16 ORDER BY ID;

create table pltest.tableVARCHAR161 as select pltest.PLPERLinpara_unname_2(pltest.tableVARCHAR16.key) AS ID from pltest.tableVARCHAR16 DISTRIBUTED randomly;

select * from pltest.tableVARCHAR161 ORDER BY ID;

drop table pltest.tableVARCHAR161;

select pltest.PLPERLinpara_unname_2('ASDFGHJKL'::VARCHAR(16));

select pltest.PLPERLinpara_unname_2(NULL);

select * from pltest.PLPERLinpara_unname_2('ASDFGHJKL'::VARCHAR(16));

select * from pltest.PLPERLinpara_unname_2(NULL);

DROP function pltest.PLPERLinpara_unname_2 ( IN  VARCHAR(16) );
----Case 2 end----


----Case 3 begin----
--
--              Unnamed parameter test;
--              CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
--
\echo '--ERROR:  PL/Perl functions cannot return type anyarray'
CREATE or REPLACE function pltest.PLPERLinpara_unname_3 ( IN  ANYARRAY ) RETURNS ANYARRAY as  
$$
        my $key = shift; return $key;
$$ language PLPERL;
----Case 3 end----


----Case 4 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_4 ( IN  UNKNOWN ) RETURNS UNKNOWN as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_4('QAZWSXEDCRFV'::UNKNOWN);

select pltest.PLPERLinpara_unname_4(NULL::UNKNOWN);

select * from pltest.PLPERLinpara_unname_4('QAZWSXEDCRFV'::UNKNOWN);

select * from pltest.PLPERLinpara_unname_4(NULL::UNKNOWN);

DROP function pltest.PLPERLinpara_unname_4 ( IN  UNKNOWN );
----Case 4 end----


----Case 5 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_5 ( IN  BIT VARYING(10) ) RETURNS BIT VARYING(10) as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_5(pltest.tableBITVARYING10.key) AS ID from pltest.tableBITVARYING10 ORDER BY ID;

create table pltest.tableBITVARYING101 as select pltest.PLPERLinpara_unname_5(pltest.tableBITVARYING10.key) AS ID from pltest.tableBITVARYING10 DISTRIBUTED randomly;

select * from pltest.tableBITVARYING101 ORDER BY ID;

drop table pltest.tableBITVARYING101;

select pltest.PLPERLinpara_unname_5(B'11001111'::BIT VARYING(10));

select pltest.PLPERLinpara_unname_5(NULL);

select * from pltest.PLPERLinpara_unname_5(B'11001111'::BIT VARYING(10));

select * from pltest.PLPERLinpara_unname_5(NULL);

DROP function pltest.PLPERLinpara_unname_5 ( IN  BIT VARYING(10) );
----Case 5 end----


----Case 6 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_6 ( IN  TIMESTAMPTZ ) RETURNS TIMESTAMPTZ as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_6(pltest.tableTIMESTAMPTZ.key) AS ID from pltest.tableTIMESTAMPTZ ORDER BY ID;

create table pltest.tableTIMESTAMPTZ1 as select pltest.PLPERLinpara_unname_6(pltest.tableTIMESTAMPTZ.key) AS ID from pltest.tableTIMESTAMPTZ DISTRIBUTED randomly;

select * from pltest.tableTIMESTAMPTZ1 ORDER BY ID;

drop table pltest.tableTIMESTAMPTZ1;

select pltest.PLPERLinpara_unname_6('2011-08-12 10:00:52.14'::TIMESTAMPTZ);

select pltest.PLPERLinpara_unname_6(NULL);

select * from pltest.PLPERLinpara_unname_6('2011-08-12 10:00:52.14'::TIMESTAMPTZ);

select * from pltest.PLPERLinpara_unname_6(NULL);

DROP function pltest.PLPERLinpara_unname_6 ( IN  TIMESTAMPTZ );
----Case 6 end----


----Case 7 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_7 ( IN  "char" ) RETURNS "char" as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_7(pltest.tableQUTOCHAR.key) AS ID from pltest.tableQUTOCHAR ORDER BY ID;

create table pltest.tableQUTOCHAR1 as select pltest.PLPERLinpara_unname_7(pltest.tableQUTOCHAR.key) AS ID from pltest.tableQUTOCHAR DISTRIBUTED randomly;

select * from pltest.tableQUTOCHAR1 ORDER BY ID;

drop table pltest.tableQUTOCHAR1;

select pltest.PLPERLinpara_unname_7('a'::"char");

select pltest.PLPERLinpara_unname_7(null::"char");

select * from pltest.PLPERLinpara_unname_7('a'::"char");

select * from pltest.PLPERLinpara_unname_7(null::"char");

DROP function pltest.PLPERLinpara_unname_7 ( IN  "char" );
----Case 7 end----


----Case 8 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_8 ( IN  CHAR ) RETURNS CHAR as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_8(pltest.tableCHAR.key) AS ID from pltest.tableCHAR ORDER BY ID;

create table pltest.tableCHAR1 as select pltest.PLPERLinpara_unname_8(pltest.tableCHAR.key) AS ID from pltest.tableCHAR DISTRIBUTED randomly;

select * from pltest.tableCHAR1 ORDER BY ID;

drop table pltest.tableCHAR1;

select pltest.PLPERLinpara_unname_8('B'::CHAR);

select pltest.PLPERLinpara_unname_8(NULL::CHAR);

select * from pltest.PLPERLinpara_unname_8('B'::CHAR);

select * from pltest.PLPERLinpara_unname_8(NULL::CHAR);

DROP function pltest.PLPERLinpara_unname_8 ( IN  CHAR );
----Case 8 end----


----Case 9 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_9 ( IN  BIGINT ) RETURNS BIGINT as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_9(pltest.tableBIGINT.key) AS ID from pltest.tableBIGINT ORDER BY ID;

create table pltest.tableBIGINT1 as select pltest.PLPERLinpara_unname_9(pltest.tableBIGINT.key) AS ID from pltest.tableBIGINT DISTRIBUTED randomly;

select * from pltest.tableBIGINT1 ORDER BY ID;

drop table pltest.tableBIGINT1;

select pltest.PLPERLinpara_unname_9((-9223372036854775808)::BIGINT);

select pltest.PLPERLinpara_unname_9(9223372036854775807::BIGINT);

select pltest.PLPERLinpara_unname_9(123456789::BIGINT);

select pltest.PLPERLinpara_unname_9(NULL);

select * from pltest.PLPERLinpara_unname_9((-9223372036854775808)::BIGINT);

select * from pltest.PLPERLinpara_unname_9(9223372036854775807::BIGINT);

select * from pltest.PLPERLinpara_unname_9(123456789::BIGINT);

select * from pltest.PLPERLinpara_unname_9(NULL);

DROP function pltest.PLPERLinpara_unname_9 ( IN  BIGINT );
----Case 9 end----


----Case 10 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_10 ( IN  DATE ) RETURNS DATE as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_10(pltest.tableDATE.key) AS ID from pltest.tableDATE ORDER BY ID;

create table pltest.tableDATE1 as select pltest.PLPERLinpara_unname_10(pltest.tableDATE.key) AS ID from pltest.tableDATE DISTRIBUTED randomly;

select * from pltest.tableDATE1 ORDER BY ID;

drop table pltest.tableDATE1;

select pltest.PLPERLinpara_unname_10('2011-08-12'::DATE);

select pltest.PLPERLinpara_unname_10('EPOCH'::DATE);

select pltest.PLPERLinpara_unname_10(NULL);

select * from pltest.PLPERLinpara_unname_10('2011-08-12'::DATE);

select * from pltest.PLPERLinpara_unname_10('EPOCH'::DATE);

select * from pltest.PLPERLinpara_unname_10(NULL);

DROP function pltest.PLPERLinpara_unname_10 ( IN  DATE );
----Case 10 end----


----Case 11 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_11 ( IN  PATH ) RETURNS PATH as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_11('(1,1),(2,2)'::PATH);

select pltest.PLPERLinpara_unname_11(NULL);

select * from pltest.PLPERLinpara_unname_11('(1,1),(2,2)'::PATH);

select * from pltest.PLPERLinpara_unname_11(NULL);

DROP function pltest.PLPERLinpara_unname_11 ( IN  PATH );
----Case 11 end----


----Case 12 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_12 ( IN  SMALLINT ) RETURNS SMALLINT as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_12(pltest.tableSMALLINT.key) AS ID from pltest.tableSMALLINT ORDER BY ID;

create table pltest.tableSMALLINT1 as select pltest.PLPERLinpara_unname_12(pltest.tableSMALLINT.key) AS ID from pltest.tableSMALLINT DISTRIBUTED randomly;

select * from pltest.tableSMALLINT1 ORDER BY ID;

drop table pltest.tableSMALLINT1;

select pltest.PLPERLinpara_unname_12((-32768)::SMALLINT);

select pltest.PLPERLinpara_unname_12(32767::SMALLINT);

select pltest.PLPERLinpara_unname_12(1234::SMALLINT);

select pltest.PLPERLinpara_unname_12(NULL);

select * from pltest.PLPERLinpara_unname_12((-32768)::SMALLINT);

select * from pltest.PLPERLinpara_unname_12(32767::SMALLINT);

select * from pltest.PLPERLinpara_unname_12(1234::SMALLINT);

select * from pltest.PLPERLinpara_unname_12(NULL);

DROP function pltest.PLPERLinpara_unname_12 ( IN  SMALLINT );
----Case 12 end----


----Case 13 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_13 ( IN  MACADDR ) RETURNS MACADDR as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_13(pltest.tableMACADDR.key) AS ID from pltest.tableMACADDR ORDER BY ID;

create table pltest.tableMACADDR1 as select pltest.PLPERLinpara_unname_13(pltest.tableMACADDR.key) AS ID from pltest.tableMACADDR DISTRIBUTED randomly;

select * from pltest.tableMACADDR1 ORDER BY ID;

drop table pltest.tableMACADDR1;

select pltest.PLPERLinpara_unname_13('12:34:56:78:90:AB'::MACADDR);

select pltest.PLPERLinpara_unname_13(NULL);

select * from pltest.PLPERLinpara_unname_13('12:34:56:78:90:AB'::MACADDR);

select * from pltest.PLPERLinpara_unname_13(NULL);

DROP function pltest.PLPERLinpara_unname_13 ( IN  MACADDR );
----Case 13 end----


----Case 14 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_14 ( IN  POINT ) RETURNS POINT as $$     
	my $key = shift; return $key;
$$ language PLPERL;



select pltest.PLPERLinpara_unname_14('(1,2)'::POINT);

select pltest.PLPERLinpara_unname_14(NULL);

select * from pltest.PLPERLinpara_unname_14('(1,2)'::POINT);

select * from pltest.PLPERLinpara_unname_14(NULL);

DROP function pltest.PLPERLinpara_unname_14 ( IN  POINT );
----Case 14 end----


----Case 15 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_15 ( IN  INTERVAL ) RETURNS INTERVAL as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_15(pltest.tableINTERVAL.key) AS ID from pltest.tableINTERVAL ORDER BY ID;

create table pltest.tableINTERVAL1 as select pltest.PLPERLinpara_unname_15(pltest.tableINTERVAL.key) AS ID from pltest.tableINTERVAL DISTRIBUTED randomly;

select * from pltest.tableINTERVAL1 ORDER BY ID;

drop table pltest.tableINTERVAL1;

select pltest.PLPERLinpara_unname_15('1 DAY 12 HOURS 59 MIN 10 SEC'::INTERVAL);

select pltest.PLPERLinpara_unname_15('1 12:59:10'::INTERVAL);

select pltest.PLPERLinpara_unname_15(NULL);

select * from pltest.PLPERLinpara_unname_15('1 DAY 12 HOURS 59 MIN 10 SEC'::INTERVAL);

select * from pltest.PLPERLinpara_unname_15('1 12:59:10'::INTERVAL);

select * from pltest.PLPERLinpara_unname_15(NULL);

DROP function pltest.PLPERLinpara_unname_15 ( IN  INTERVAL );
----Case 15 end----


----Case 16 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_16 ( IN  NUMERIC ) RETURNS NUMERIC as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_16(pltest.tableNUMERIC.key) AS ID from pltest.tableNUMERIC ORDER BY ID;

create table pltest.tableNUMERIC1 as select pltest.PLPERLinpara_unname_16(pltest.tableNUMERIC.key) AS ID from pltest.tableNUMERIC DISTRIBUTED randomly;

select * from pltest.tableNUMERIC1 ORDER BY ID;

drop table pltest.tableNUMERIC1;

select pltest.PLPERLinpara_unname_16(1234567890.0987654321::NUMERIC);

select pltest.PLPERLinpara_unname_16('NAN'::NUMERIC);

select pltest.PLPERLinpara_unname_16(2::NUMERIC^128 / 3::NUMERIC^129);

select pltest.PLPERLinpara_unname_16(2::NUMERIC^128);

select pltest.PLPERLinpara_unname_16(0.5::NUMERIC);

select pltest.PLPERLinpara_unname_16(NULL);

select * from pltest.PLPERLinpara_unname_16(1234567890.0987654321::NUMERIC);

select * from pltest.PLPERLinpara_unname_16('NAN'::NUMERIC);

select * from pltest.PLPERLinpara_unname_16(2::NUMERIC^128 / 3::NUMERIC^129);

select * from pltest.PLPERLinpara_unname_16(2::NUMERIC^128);

select * from pltest.PLPERLinpara_unname_16(0.5::NUMERIC);

select * from pltest.PLPERLinpara_unname_16(NULL);

DROP function pltest.PLPERLinpara_unname_16 ( IN  NUMERIC );
----Case 16 end----


----Case 17 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_17 ( IN  INT4 ) RETURNS INT4 as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_17(pltest.tableINT4.key) AS ID from pltest.tableINT4 ORDER BY ID;

create table pltest.tableINT41 as select pltest.PLPERLinpara_unname_17(pltest.tableINT4.key) AS ID from pltest.tableINT4 DISTRIBUTED randomly;

select * from pltest.tableINT41 ORDER BY ID;

drop table pltest.tableINT41;

select pltest.PLPERLinpara_unname_17((-2147483648)::INT4);

select pltest.PLPERLinpara_unname_17(2147483647::INT4);

select pltest.PLPERLinpara_unname_17(1234);

select pltest.PLPERLinpara_unname_17(NULL);

select * from pltest.PLPERLinpara_unname_17((-2147483648)::INT4);

select * from pltest.PLPERLinpara_unname_17(2147483647::INT4);

select * from pltest.PLPERLinpara_unname_17(1234);

select * from pltest.PLPERLinpara_unname_17(NULL);

DROP function pltest.PLPERLinpara_unname_17 ( IN  INT4 );
----Case 17 end----


----Case 18 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_18 ( IN  CHAR(4) ) RETURNS CHAR(4) as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_18(pltest.tableCHAR4.key) AS ID from pltest.tableCHAR4 ORDER BY ID;

create table pltest.tableCHAR41 as select pltest.PLPERLinpara_unname_18(pltest.tableCHAR4.key) AS ID from pltest.tableCHAR4 DISTRIBUTED randomly;

select * from pltest.tableCHAR41 ORDER BY ID;

drop table pltest.tableCHAR41;

select pltest.PLPERLinpara_unname_18('SSSS'::CHAR(4));

select pltest.PLPERLinpara_unname_18(NULL);

select * from pltest.PLPERLinpara_unname_18('SSSS'::CHAR(4));

select * from pltest.PLPERLinpara_unname_18(NULL);

DROP function pltest.PLPERLinpara_unname_18 ( IN  CHAR(4) );
----Case 18 end----


----Case 19 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_19 ( IN  TEXT ) RETURNS TEXT as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_19(pltest.tableTEXT.key) AS ID from pltest.tableTEXT ORDER BY ID;

create table pltest.tableTEXT1 as select pltest.PLPERLinpara_unname_19(pltest.tableTEXT.key) AS ID from pltest.tableTEXT DISTRIBUTED randomly;

select * from pltest.tableTEXT1 ORDER BY ID;

drop table pltest.tableTEXT1;

select pltest.PLPERLinpara_unname_19('QAZWSXEDCRFVTGB'::TEXT);

select pltest.PLPERLinpara_unname_19(NULL);

select * from pltest.PLPERLinpara_unname_19('QAZWSXEDCRFVTGB'::TEXT);

select * from pltest.PLPERLinpara_unname_19(NULL);

DROP function pltest.PLPERLinpara_unname_19 ( IN  TEXT );
----Case 19 end----


----Case 20 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_20 ( IN  INET ) RETURNS INET as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_20(pltest.tableINET.key) AS ID from pltest.tableINET ORDER BY ID;

create table pltest.tableINET1 as select pltest.PLPERLinpara_unname_20(pltest.tableINET.key) AS ID from pltest.tableINET DISTRIBUTED randomly;

select * from pltest.tableINET1 ORDER BY ID;

drop table pltest.tableINET1;

select pltest.PLPERLinpara_unname_20('192.168.0.1'::INET);

select pltest.PLPERLinpara_unname_20(NULL);

select * from pltest.PLPERLinpara_unname_20('192.168.0.1'::INET);

select * from pltest.PLPERLinpara_unname_20(NULL);

DROP function pltest.PLPERLinpara_unname_20 ( IN  INET );
----Case 20 end----


----Case 21 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_21 ( IN  POLYGON ) RETURNS POLYGON as $$     
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinpara_unname_21('(0,0),(1,1),(2,0)'::POLYGON);

select pltest.PLPERLinpara_unname_21(NULL);

select * from pltest.PLPERLinpara_unname_21('(0,0),(1,1),(2,0)'::POLYGON);

select * from pltest.PLPERLinpara_unname_21(NULL);

DROP function pltest.PLPERLinpara_unname_21 ( IN  POLYGON );
----Case 21 end----


----Case 22 begin----
-- 
--              Unnamed parameter test;
--              CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].[
--           
\echo '--ERROR:  PL/Perl functions cannot return type cstring' 
CREATE or REPLACE function pltest.PLPERLinpara_unname_22 ( IN  CSTRING ) RETURNS CSTRING as $$
$     R
        my $key = shift; return $key;
$$ language PLPERL;
----Case 22 end----


----Case 23 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_23 ( IN  LSEG ) RETURNS LSEG as $$     
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinpara_unname_23('((1,1),(2,2))'::LSEG);

select pltest.PLPERLinpara_unname_23(NULL);

select * from pltest.PLPERLinpara_unname_23('((1,1),(2,2))'::LSEG);

select * from pltest.PLPERLinpara_unname_23(NULL);

DROP function pltest.PLPERLinpara_unname_23 ( IN  LSEG );
----Case 23 end----



----Case 24 begin----
--
--              Unnamed parameter test;
--              CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
--
\echo '--ERROR:  PL/Perl functions cannot return type anyelement'
CREATE or REPLACE function pltest.PLPERLinpara_unname_24 ( IN  ANYELEMENT ) RETURNS ANYELEMENT as $$
        my $key = shift; return $key;
$$ language PLPERL;
----Case 24 end----

----Case 25 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_25 ( IN  CIDR ) RETURNS CIDR as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_25(pltest.tableCIDR.key) AS ID from pltest.tableCIDR ORDER BY ID;

create table pltest.tableCIDR1 as select pltest.PLPERLinpara_unname_25(pltest.tableCIDR.key) AS ID from pltest.tableCIDR DISTRIBUTED randomly;

select * from pltest.tableCIDR1 ORDER BY ID;

drop table pltest.tableCIDR1;

select pltest.PLPERLinpara_unname_25('192.168.0.1/32'::CIDR);

select pltest.PLPERLinpara_unname_25(NULL);

select * from pltest.PLPERLinpara_unname_25('192.168.0.1/32'::CIDR);

select * from pltest.PLPERLinpara_unname_25(NULL);

DROP function pltest.PLPERLinpara_unname_25 ( IN  CIDR );
----Case 25 end----


----Case 26 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_26 ( IN  BYTEA ) RETURNS BYTEA as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_26(pltest.tableBYTEA.key) AS ID from pltest.tableBYTEA ORDER BY ID;

create table pltest.tableBYTEA1 as select pltest.PLPERLinpara_unname_26(pltest.tableBYTEA.key) AS ID from pltest.tableBYTEA DISTRIBUTED randomly;

select * from pltest.tableBYTEA1 ORDER BY ID;

drop table pltest.tableBYTEA1;

select pltest.PLPERLinpara_unname_26(E'\\000'::BYTEA);

select pltest.PLPERLinpara_unname_26(NULL);

select * from pltest.PLPERLinpara_unname_26(E'\\000'::BYTEA);

select * from pltest.PLPERLinpara_unname_26(NULL);

DROP function pltest.PLPERLinpara_unname_26 ( IN  BYTEA );
----Case 26 end----


----Case 27 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_27 ( IN  MONEY ) RETURNS MONEY as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_27(pltest.tableMONEY.key) AS ID from pltest.tableMONEY ORDER BY ID;

create table pltest.tableMONEY1 as select pltest.PLPERLinpara_unname_27(pltest.tableMONEY.key) AS ID from pltest.tableMONEY DISTRIBUTED randomly;

select * from pltest.tableMONEY1 ORDER BY ID;

drop table pltest.tableMONEY1;

select pltest.PLPERLinpara_unname_27('$-21474836.48'::MONEY);

select pltest.PLPERLinpara_unname_27('$21474836.47'::MONEY);

select pltest.PLPERLinpara_unname_27('$1234'::MONEY);

select pltest.PLPERLinpara_unname_27(NULL);

select * from pltest.PLPERLinpara_unname_27('$-21474836.48'::MONEY);

select * from pltest.PLPERLinpara_unname_27('$21474836.47'::MONEY);

select * from pltest.PLPERLinpara_unname_27('$1234'::MONEY);

select * from pltest.PLPERLinpara_unname_27(NULL);

DROP function pltest.PLPERLinpara_unname_27 ( IN  MONEY );
----Case 27 end----


----Case 28 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_28 ( IN  TIMESTAMP ) RETURNS TIMESTAMP as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_28(pltest.tableTIMESTAMP.key) AS ID from pltest.tableTIMESTAMP ORDER BY ID;

create table pltest.tableTIMESTAMP1 as select pltest.PLPERLinpara_unname_28(pltest.tableTIMESTAMP.key) AS ID from pltest.tableTIMESTAMP DISTRIBUTED randomly;

select * from pltest.tableTIMESTAMP1 ORDER BY ID;

drop table pltest.tableTIMESTAMP1;

select pltest.PLPERLinpara_unname_28('2011-08-12 10:00:52.14'::TIMESTAMP);

select pltest.PLPERLinpara_unname_28(NULL);

select * from pltest.PLPERLinpara_unname_28('2011-08-12 10:00:52.14'::TIMESTAMP);

select * from pltest.PLPERLinpara_unname_28(NULL);

DROP function pltest.PLPERLinpara_unname_28 ( IN  TIMESTAMP );
----Case 28 end----


----Case 29 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_29 ( IN  BOOLEAN ) RETURNS BOOLEAN as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_29(pltest.tableBOOLEAN.key) AS ID from pltest.tableBOOLEAN ORDER BY ID;

create table pltest.tableBOOLEAN1 as select pltest.PLPERLinpara_unname_29(pltest.tableBOOLEAN.key) AS ID from pltest.tableBOOLEAN DISTRIBUTED randomly;

select * from pltest.tableBOOLEAN1 ORDER BY ID;

drop table pltest.tableBOOLEAN1;

select pltest.PLPERLinpara_unname_29(FALSE::BOOLEAN);

select pltest.PLPERLinpara_unname_29(TRUE::BOOLEAN);

select pltest.PLPERLinpara_unname_29(NULL);

select * from pltest.PLPERLinpara_unname_29(FALSE::BOOLEAN);

select * from pltest.PLPERLinpara_unname_29(TRUE::BOOLEAN);

select * from pltest.PLPERLinpara_unname_29(NULL);

DROP function pltest.PLPERLinpara_unname_29 ( IN  BOOLEAN );
----Case 29 end----


----Case 30 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_30 ( IN  FLOAT8 ) RETURNS FLOAT8 as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_30(pltest.tableFLOAT8.key) AS ID from pltest.tableFLOAT8 ORDER BY ID;

create table pltest.tableFLOAT81 as select pltest.PLPERLinpara_unname_30(pltest.tableFLOAT8.key) AS ID from pltest.tableFLOAT8 DISTRIBUTED randomly;

select * from pltest.tableFLOAT81 ORDER BY ID;

drop table pltest.tableFLOAT81;

select pltest.PLPERLinpara_unname_30(123456789.98765432::FLOAT8);

select pltest.PLPERLinpara_unname_30('NAN'::FLOAT8);

select pltest.PLPERLinpara_unname_30('-INFINITY'::FLOAT8);

select pltest.PLPERLinpara_unname_30('+INFINITY'::FLOAT8);

select pltest.PLPERLinpara_unname_30(NULL);

select * from pltest.PLPERLinpara_unname_30(123456789.98765432::FLOAT8);

select * from pltest.PLPERLinpara_unname_30('NAN'::FLOAT8);

select * from pltest.PLPERLinpara_unname_30('-INFINITY'::FLOAT8);

select * from pltest.PLPERLinpara_unname_30('+INFINITY'::FLOAT8);

select * from pltest.PLPERLinpara_unname_30(NULL);

DROP function pltest.PLPERLinpara_unname_30 ( IN  FLOAT8 );
----Case 30 end----


----Case 31 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_31 ( IN  TIME ) RETURNS TIME as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_31(pltest.tableTIME.key) AS ID from pltest.tableTIME ORDER BY ID;

create table pltest.tableTIME1 as select pltest.PLPERLinpara_unname_31(pltest.tableTIME.key) AS ID from pltest.tableTIME DISTRIBUTED randomly;

select * from pltest.tableTIME1 ORDER BY ID;

drop table pltest.tableTIME1;

select pltest.PLPERLinpara_unname_31('10:00:52.14'::TIME);

select pltest.PLPERLinpara_unname_31('ALLBALLS'::TIME);

select pltest.PLPERLinpara_unname_31(NULL);

select * from pltest.PLPERLinpara_unname_31('10:00:52.14'::TIME);

select * from pltest.PLPERLinpara_unname_31('ALLBALLS'::TIME);

select * from pltest.PLPERLinpara_unname_31(NULL);

DROP function pltest.PLPERLinpara_unname_31 ( IN  TIME );
----Case 31 end----


----Case 32 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_32 ( IN  CIRCLE ) RETURNS CIRCLE as $$     
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinpara_unname_32('<(1,1),2>'::CIRCLE);

select pltest.PLPERLinpara_unname_32(NULL);

select * from pltest.PLPERLinpara_unname_32('<(1,1),2>'::CIRCLE);

select * from pltest.PLPERLinpara_unname_32(NULL);

DROP function pltest.PLPERLinpara_unname_32 ( IN  CIRCLE );
----Case 32 end----


----Case 33 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_33 ( IN  TIMETZ ) RETURNS TIMETZ as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_33(pltest.tableTIMETZ.key) AS ID from pltest.tableTIMETZ ORDER BY ID;

create table pltest.tableTIMETZ1 as select pltest.PLPERLinpara_unname_33(pltest.tableTIMETZ.key) AS ID from pltest.tableTIMETZ DISTRIBUTED randomly;

select * from pltest.tableTIMETZ1 ORDER BY ID;

drop table pltest.tableTIMETZ1;

select pltest.PLPERLinpara_unname_33('2011-08-12 10:00:52.14'::TIMETZ);

select pltest.PLPERLinpara_unname_33(NULL);

select * from pltest.PLPERLinpara_unname_33('2011-08-12 10:00:52.14'::TIMETZ);

select * from pltest.PLPERLinpara_unname_33(NULL);

DROP function pltest.PLPERLinpara_unname_33 ( IN  TIMETZ );
----Case 33 end----


----Case 34 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_34 ( IN  BIT(8) ) RETURNS BIT(8) as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_34(pltest.tableBIT8.key) AS ID from pltest.tableBIT8 ORDER BY ID;

create table pltest.tableBIT81 as select pltest.PLPERLinpara_unname_34(pltest.tableBIT8.key) AS ID from pltest.tableBIT8 DISTRIBUTED randomly;

select * from pltest.tableBIT81 ORDER BY ID;

drop table pltest.tableBIT81;

select pltest.PLPERLinpara_unname_34(B'11001111'::BIT(8));

select pltest.PLPERLinpara_unname_34(NULL);

select * from pltest.PLPERLinpara_unname_34(B'11001111'::BIT(8));

select * from pltest.PLPERLinpara_unname_34(NULL);

DROP function pltest.PLPERLinpara_unname_34 ( IN  BIT(8) );
----Case 34 end----


----Case 36 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_36 ( IN  BOOLEAN[] ) RETURNS BOOLEAN[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_36(pltest.tableBOOLEANarray.key) AS ID from pltest.tableBOOLEANarray ORDER BY ID;

create table pltest.tableBOOLEANarray1 as select pltest.PLPERLinpara_unname_36(pltest.tableBOOLEANarray.key) AS ID from pltest.tableBOOLEANarray DISTRIBUTED randomly;

select * from pltest.tableBOOLEANarray1 ORDER BY ID;

drop table pltest.tableBOOLEANarray1;

select pltest.PLPERLinpara_unname_36(array[FALSE::BOOLEAN,TRUE::BOOLEAN,NULL]::BOOLEAN[]);

select * from pltest.PLPERLinpara_unname_36(array[FALSE::BOOLEAN,TRUE::BOOLEAN,NULL]::BOOLEAN[]);

DROP function pltest.PLPERLinpara_unname_36 ( IN  BOOLEAN[] );
----Case 36 end----


----Case 37 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_37 ( IN  INTERVAL[] ) RETURNS INTERVAL[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_37(pltest.tableINTERVALarray.key) AS ID from pltest.tableINTERVALarray ORDER BY ID;

create table pltest.tableINTERVALarray1 as select pltest.PLPERLinpara_unname_37(pltest.tableINTERVALarray.key) AS ID from pltest.tableINTERVALarray DISTRIBUTED randomly;

select * from pltest.tableINTERVALarray1 ORDER BY ID;

drop table pltest.tableINTERVALarray1;

select pltest.PLPERLinpara_unname_37(array['1 DAY 12 HOURS 59 MIN 10 SEC'::INTERVAL,'1 12:59:10'::INTERVAL,NULL]::INTERVAL[]);

select * from pltest.PLPERLinpara_unname_37(array['1 DAY 12 HOURS 59 MIN 10 SEC'::INTERVAL,'1 12:59:10'::INTERVAL,NULL]::INTERVAL[]);

DROP function pltest.PLPERLinpara_unname_37 ( IN  INTERVAL[] );
----Case 37 end----


----Case 38 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_38 ( IN  BIT(8)[] ) RETURNS BIT(8)[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_38(pltest.tableBIT8array.key) AS ID from pltest.tableBIT8array ORDER BY ID;

create table pltest.tableBIT8array1 as select pltest.PLPERLinpara_unname_38(pltest.tableBIT8array.key) AS ID from pltest.tableBIT8array DISTRIBUTED randomly;

select * from pltest.tableBIT8array1 ORDER BY ID;

drop table pltest.tableBIT8array1;

select pltest.PLPERLinpara_unname_38(array[B'11001111'::BIT(8),NULL]::BIT(8)[]);

select * from pltest.PLPERLinpara_unname_38(array[B'11001111'::BIT(8),NULL]::BIT(8)[]);

DROP function pltest.PLPERLinpara_unname_38 ( IN  BIT(8)[] );
----Case 38 end----


----Case 39 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_39 ( IN  TIMETZ[] ) RETURNS TIMETZ[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_39(pltest.tableTIMETZarray.key) AS ID from pltest.tableTIMETZarray ORDER BY ID;

create table pltest.tableTIMETZarray1 as select pltest.PLPERLinpara_unname_39(pltest.tableTIMETZarray.key) AS ID from pltest.tableTIMETZarray DISTRIBUTED randomly;

select * from pltest.tableTIMETZarray1 ORDER BY ID;

drop table pltest.tableTIMETZarray1;

select pltest.PLPERLinpara_unname_39(array['2011-08-12 10:00:52.14'::TIMETZ,NULL]::TIMETZ[]);

select * from pltest.PLPERLinpara_unname_39(array['2011-08-12 10:00:52.14'::TIMETZ,NULL]::TIMETZ[]);

DROP function pltest.PLPERLinpara_unname_39 ( IN  TIMETZ[] );
----Case 39 end----


----Case 40 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_40 ( IN  CIRCLE[] ) RETURNS CIRCLE[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinpara_unname_40(array['<(1,1),2>'::CIRCLE,NULL]::CIRCLE[]);

select * from pltest.PLPERLinpara_unname_40(array['<(1,1),2>'::CIRCLE,NULL]::CIRCLE[]);

DROP function pltest.PLPERLinpara_unname_40 ( IN  CIRCLE[] );
----Case 40 end----


----Case 41 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_41 ( IN  BYTEA[] ) RETURNS BYTEA[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_41(pltest.tableBYTEAarray.key) AS ID from pltest.tableBYTEAarray ORDER BY ID;

create table pltest.tableBYTEAarray1 as select pltest.PLPERLinpara_unname_41(pltest.tableBYTEAarray.key) AS ID from pltest.tableBYTEAarray DISTRIBUTED randomly;

select * from pltest.tableBYTEAarray1 ORDER BY ID;

drop table pltest.tableBYTEAarray1;

select pltest.PLPERLinpara_unname_41(array[E'\\000'::BYTEA,NULL]::BYTEA[]);

select * from pltest.PLPERLinpara_unname_41(array[E'\\000'::BYTEA,NULL]::BYTEA[]);

DROP function pltest.PLPERLinpara_unname_41 ( IN  BYTEA[] );
----Case 41 end----


----Case 42 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_42 ( IN  TIME[] ) RETURNS TIME[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_42(pltest.tableTIMEarray.key) AS ID from pltest.tableTIMEarray ORDER BY ID;

create table pltest.tableTIMEarray1 as select pltest.PLPERLinpara_unname_42(pltest.tableTIMEarray.key) AS ID from pltest.tableTIMEarray DISTRIBUTED randomly;

select * from pltest.tableTIMEarray1 ORDER BY ID;

drop table pltest.tableTIMEarray1;

select pltest.PLPERLinpara_unname_42(array['10:00:52.14'::TIME,'ALLBALLS'::TIME,NULL]::TIME[]);

select * from pltest.PLPERLinpara_unname_42(array['10:00:52.14'::TIME,'ALLBALLS'::TIME,NULL]::TIME[]);

DROP function pltest.PLPERLinpara_unname_42 ( IN  TIME[] );
----Case 42 end----


----Case 43 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_43 ( IN  REAL[] ) RETURNS REAL[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_43(pltest.tableREALarray.key) AS ID from pltest.tableREALarray ORDER BY ID;

create table pltest.tableREALarray1 as select pltest.PLPERLinpara_unname_43(pltest.tableREALarray.key) AS ID from pltest.tableREALarray DISTRIBUTED randomly;

select * from pltest.tableREALarray1 ORDER BY ID;

drop table pltest.tableREALarray1;

select pltest.PLPERLinpara_unname_43(array[39.333::REAL,'NAN'::REAL,'-INFINITY'::REAL,'+INFINITY'::REAL,NULL]::REAL[]);

select * from pltest.PLPERLinpara_unname_43(array[39.333::REAL,'NAN'::REAL,'-INFINITY'::REAL,'+INFINITY'::REAL,NULL]::REAL[]);

DROP function pltest.PLPERLinpara_unname_43 ( IN  REAL[] );
----Case 43 end----




----Case 45 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_45 ( IN  MONEY[] ) RETURNS MONEY[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_45(pltest.tableMONEYarray.key) AS ID from pltest.tableMONEYarray ORDER BY ID;

create table pltest.tableMONEYarray1 as select pltest.PLPERLinpara_unname_45(pltest.tableMONEYarray.key) AS ID from pltest.tableMONEYarray DISTRIBUTED randomly;

select * from pltest.tableMONEYarray1 ORDER BY ID;

drop table pltest.tableMONEYarray1;

select pltest.PLPERLinpara_unname_45(array['$-21474836.48'::MONEY,'$21474836.47'::MONEY,'$1234'::MONEY,NULL]::MONEY[]);

select * from pltest.PLPERLinpara_unname_45(array['$-21474836.48'::MONEY,'$21474836.47'::MONEY,'$1234'::MONEY,NULL]::MONEY[]);

DROP function pltest.PLPERLinpara_unname_45 ( IN  MONEY[] );
----Case 45 end----


----Case 46 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_46 ( IN  POINT[] ) RETURNS POINT[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;
select pltest.PLPERLinpara_unname_46(array['(1,2)'::POINT,NULL]::POINT[]);

select * from pltest.PLPERLinpara_unname_46(array['(1,2)'::POINT,NULL]::POINT[]);

DROP function pltest.PLPERLinpara_unname_46 ( IN  POINT[] );
----Case 46 end----


----Case 47 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_47 ( IN  CHAR[] ) RETURNS CHAR[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_47(pltest.tableCHARarray.key) AS ID from pltest.tableCHARarray ORDER BY ID;

create table pltest.tableCHARarray1 as select pltest.PLPERLinpara_unname_47(pltest.tableCHARarray.key) AS ID from pltest.tableCHARarray DISTRIBUTED randomly;

select * from pltest.tableCHARarray1 ORDER BY ID;

drop table pltest.tableCHARarray1;

select pltest.PLPERLinpara_unname_47(array['B'::CHAR,NULL::CHAR]::CHAR[]);

select * from pltest.PLPERLinpara_unname_47(array['B'::CHAR,NULL::CHAR]::CHAR[]);

DROP function pltest.PLPERLinpara_unname_47 ( IN  CHAR[] );
----Case 47 end----


----Case 48 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_48 ( IN  MACADDR[] ) RETURNS MACADDR[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_48(pltest.tableMACADDRarray.key) AS ID from pltest.tableMACADDRarray ORDER BY ID;

create table pltest.tableMACADDRarray1 as select pltest.PLPERLinpara_unname_48(pltest.tableMACADDRarray.key) AS ID from pltest.tableMACADDRarray DISTRIBUTED randomly;

select * from pltest.tableMACADDRarray1 ORDER BY ID;

drop table pltest.tableMACADDRarray1;

select pltest.PLPERLinpara_unname_48(array['12:34:56:78:90:AB'::MACADDR,NULL]::MACADDR[]);

select * from pltest.PLPERLinpara_unname_48(array['12:34:56:78:90:AB'::MACADDR,NULL]::MACADDR[]);

DROP function pltest.PLPERLinpara_unname_48 ( IN  MACADDR[] );
----Case 48 end----


----Case 49 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_49 ( IN  FLOAT8[] ) RETURNS FLOAT8[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_49(pltest.tableFLOAT8array.key) AS ID from pltest.tableFLOAT8array ORDER BY ID;

create table pltest.tableFLOAT8array1 as select pltest.PLPERLinpara_unname_49(pltest.tableFLOAT8array.key) AS ID from pltest.tableFLOAT8array DISTRIBUTED randomly;

select * from pltest.tableFLOAT8array1 ORDER BY ID;

drop table pltest.tableFLOAT8array1;

select pltest.PLPERLinpara_unname_49(array[123456789.98765432::FLOAT8,'NAN'::FLOAT8,'-INFINITY'::FLOAT8,'+INFINITY'::FLOAT8,NULL]::FLOAT8[]);

select * from pltest.PLPERLinpara_unname_49(array[123456789.98765432::FLOAT8,'NAN'::FLOAT8,'-INFINITY'::FLOAT8,'+INFINITY'::FLOAT8,NULL]::FLOAT8[]);

DROP function pltest.PLPERLinpara_unname_49 ( IN  FLOAT8[] );
----Case 49 end----


----Case 50 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_50 ( IN  LSEG[] ) RETURNS LSEG[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinpara_unname_50(array['((1,1),(2,2))'::LSEG,NULL]::LSEG[]);

select * from pltest.PLPERLinpara_unname_50(array['((1,1),(2,2))'::LSEG,NULL]::LSEG[]);

DROP function pltest.PLPERLinpara_unname_50 ( IN  LSEG[] );
----Case 50 end----


----Case 51 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_51 ( IN  DATE[] ) RETURNS DATE[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_51(pltest.tableDATEarray.key) AS ID from pltest.tableDATEarray ORDER BY ID;

create table pltest.tableDATEarray1 as select pltest.PLPERLinpara_unname_51(pltest.tableDATEarray.key) AS ID from pltest.tableDATEarray DISTRIBUTED randomly;

select * from pltest.tableDATEarray1 ORDER BY ID;

drop table pltest.tableDATEarray1;

select pltest.PLPERLinpara_unname_51(array['2011-08-12'::DATE,'EPOCH'::DATE,NULL]::DATE[]);

select * from pltest.PLPERLinpara_unname_51(array['2011-08-12'::DATE,'EPOCH'::DATE,NULL]::DATE[]);

DROP function pltest.PLPERLinpara_unname_51 ( IN  DATE[] );
----Case 51 end----


----Case 52 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_52 ( IN  BIGINT[] ) RETURNS BIGINT[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_52(pltest.tableBIGINTarray.key) AS ID from pltest.tableBIGINTarray ORDER BY ID;

create table pltest.tableBIGINTarray1 as select pltest.PLPERLinpara_unname_52(pltest.tableBIGINTarray.key) AS ID from pltest.tableBIGINTarray DISTRIBUTED randomly;

select * from pltest.tableBIGINTarray1 ORDER BY ID;

drop table pltest.tableBIGINTarray1;

select pltest.PLPERLinpara_unname_52(array[(-9223372036854775808)::BIGINT,9223372036854775807::BIGINT,123456789::BIGINT,NULL]::BIGINT[]);

select * from pltest.PLPERLinpara_unname_52(array[(-9223372036854775808)::BIGINT,9223372036854775807::BIGINT,123456789::BIGINT,NULL]::BIGINT[]);

DROP function pltest.PLPERLinpara_unname_52 ( IN  BIGINT[] );
----Case 52 end----

----Case 54 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_54 ( IN  PATH[] ) RETURNS PATH[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinpara_unname_54(array['(1,1),(2,2)'::PATH,NULL]::PATH[]);

select * from pltest.PLPERLinpara_unname_54(array['(1,1),(2,2)'::PATH,NULL]::PATH[]);

DROP function pltest.PLPERLinpara_unname_54 ( IN  PATH[] );
----Case 54 end----


----Case 55 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_55 ( IN  POLYGON[] ) RETURNS POLYGON[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinpara_unname_55(array['(0,0),(1,1),(2,0)'::POLYGON,NULL]::POLYGON[]);

select * from pltest.PLPERLinpara_unname_55(array['(0,0),(1,1),(2,0)'::POLYGON,NULL]::POLYGON[]);

DROP function pltest.PLPERLinpara_unname_55 ( IN  POLYGON[] );
----Case 55 end----


----Case 56 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_56 ( IN  INET[] ) RETURNS INET[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_56(pltest.tableINETarray.key) AS ID from pltest.tableINETarray ORDER BY ID;

create table pltest.tableINETarray1 as select pltest.PLPERLinpara_unname_56(pltest.tableINETarray.key) AS ID from pltest.tableINETarray DISTRIBUTED randomly;

select * from pltest.tableINETarray1 ORDER BY ID;

drop table pltest.tableINETarray1;

select pltest.PLPERLinpara_unname_56(array['192.168.0.1'::INET,NULL]::INET[]);

select * from pltest.PLPERLinpara_unname_56(array['192.168.0.1'::INET,NULL]::INET[]);

DROP function pltest.PLPERLinpara_unname_56 ( IN  INET[] );
----Case 56 end----



----Case 58 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_58 ( IN  TIMESTAMPTZ[] ) RETURNS TIMESTAMPTZ[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_58(pltest.tableTIMESTAMPTZarray.key) AS ID from pltest.tableTIMESTAMPTZarray ORDER BY ID;

create table pltest.tableTIMESTAMPTZarray1 as select pltest.PLPERLinpara_unname_58(pltest.tableTIMESTAMPTZarray.key) AS ID from pltest.tableTIMESTAMPTZarray DISTRIBUTED randomly;

select * from pltest.tableTIMESTAMPTZarray1 ORDER BY ID;

drop table pltest.tableTIMESTAMPTZarray1;

select pltest.PLPERLinpara_unname_58(array['2011-08-12 10:00:52.14'::TIMESTAMPTZ,NULL]::TIMESTAMPTZ[]);

select * from pltest.PLPERLinpara_unname_58(array['2011-08-12 10:00:52.14'::TIMESTAMPTZ,NULL]::TIMESTAMPTZ[]);

DROP function pltest.PLPERLinpara_unname_58 ( IN  TIMESTAMPTZ[] );
----Case 58 end----


----Case 59 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_59 ( IN  CHAR(4)[] ) RETURNS CHAR(4)[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_59(pltest.tableCHAR4array.key) AS ID from pltest.tableCHAR4array ORDER BY ID;

create table pltest.tableCHAR4array1 as select pltest.PLPERLinpara_unname_59(pltest.tableCHAR4array.key) AS ID from pltest.tableCHAR4array DISTRIBUTED randomly;

select * from pltest.tableCHAR4array1 ORDER BY ID;

drop table pltest.tableCHAR4array1;

select pltest.PLPERLinpara_unname_59(array['SSSS'::CHAR(4),NULL]::CHAR(4)[]);

select * from pltest.PLPERLinpara_unname_59(array['SSSS'::CHAR(4),NULL]::CHAR(4)[]);

DROP function pltest.PLPERLinpara_unname_59 ( IN  CHAR(4)[] );
----Case 59 end----


----Case 60 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_60 ( IN  VARCHAR(16)[] ) RETURNS VARCHAR(16)[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_60(pltest.tableVARCHAR16array.key) AS ID from pltest.tableVARCHAR16array ORDER BY ID;

create table pltest.tableVARCHAR16array1 as select pltest.PLPERLinpara_unname_60(pltest.tableVARCHAR16array.key) AS ID from pltest.tableVARCHAR16array DISTRIBUTED randomly;

select * from pltest.tableVARCHAR16array1 ORDER BY ID;

drop table pltest.tableVARCHAR16array1;

select pltest.PLPERLinpara_unname_60(array['ASDFGHJKL'::VARCHAR(16),NULL]::VARCHAR(16)[]);

select * from pltest.PLPERLinpara_unname_60(array['ASDFGHJKL'::VARCHAR(16),NULL]::VARCHAR(16)[]);

DROP function pltest.PLPERLinpara_unname_60 ( IN  VARCHAR(16)[] );
----Case 60 end----


----Case 61 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_61 ( IN  INT4[] ) RETURNS INT4[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_61(pltest.tableINT4array.key) AS ID from pltest.tableINT4array ORDER BY ID;

create table pltest.tableINT4array1 as select pltest.PLPERLinpara_unname_61(pltest.tableINT4array.key) AS ID from pltest.tableINT4array DISTRIBUTED randomly;

select * from pltest.tableINT4array1 ORDER BY ID;

drop table pltest.tableINT4array1;

select pltest.PLPERLinpara_unname_61(array[(-2147483648)::INT4,2147483647::INT4,1234,NULL]::INT4[]);

select * from pltest.PLPERLinpara_unname_61(array[(-2147483648)::INT4,2147483647::INT4,1234,NULL]::INT4[]);

DROP function pltest.PLPERLinpara_unname_61 ( IN  INT4[] );
----Case 61 end----


----Case 62 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_62 ( IN  CIDR[] ) RETURNS CIDR[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_62(pltest.tableCIDRarray.key) AS ID from pltest.tableCIDRarray ORDER BY ID;

create table pltest.tableCIDRarray1 as select pltest.PLPERLinpara_unname_62(pltest.tableCIDRarray.key) AS ID from pltest.tableCIDRarray DISTRIBUTED randomly;

select * from pltest.tableCIDRarray1 ORDER BY ID;

drop table pltest.tableCIDRarray1;

select pltest.PLPERLinpara_unname_62(array['192.168.0.1/32'::CIDR,NULL]::CIDR[]);

select * from pltest.PLPERLinpara_unname_62(array['192.168.0.1/32'::CIDR,NULL]::CIDR[]);

DROP function pltest.PLPERLinpara_unname_62 ( IN  CIDR[] );
----Case 62 end----


----Case 63 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_63 ( IN  "char"[] ) RETURNS "char"[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_63(pltest.tableQUTOCHARarray.key) AS ID from pltest.tableQUTOCHARarray ORDER BY ID;

create table pltest.tableQUTOCHARarray1 as select pltest.PLPERLinpara_unname_63(pltest.tableQUTOCHARarray.key) AS ID from pltest.tableQUTOCHARarray DISTRIBUTED randomly;

select * from pltest.tableQUTOCHARarray1 ORDER BY ID;

drop table pltest.tableQUTOCHARarray1;

select pltest.PLPERLinpara_unname_63(array['a'::"char",null::"char"]::"char"[]);

select * from pltest.PLPERLinpara_unname_63(array['a'::"char",null::"char"]::"char"[]);

DROP function pltest.PLPERLinpara_unname_63 ( IN  "char"[] );
----Case 63 end----


----Case 64 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_64 ( IN  TIMESTAMP[] ) RETURNS TIMESTAMP[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_64(pltest.tableTIMESTAMParray.key) AS ID from pltest.tableTIMESTAMParray ORDER BY ID;

create table pltest.tableTIMESTAMParray1 as select pltest.PLPERLinpara_unname_64(pltest.tableTIMESTAMParray.key) AS ID from pltest.tableTIMESTAMParray DISTRIBUTED randomly;

select * from pltest.tableTIMESTAMParray1 ORDER BY ID;

drop table pltest.tableTIMESTAMParray1;

select pltest.PLPERLinpara_unname_64(array['2011-08-12 10:00:52.14'::TIMESTAMP,NULL]::TIMESTAMP[]);

select * from pltest.PLPERLinpara_unname_64(array['2011-08-12 10:00:52.14'::TIMESTAMP,NULL]::TIMESTAMP[]);

DROP function pltest.PLPERLinpara_unname_64 ( IN  TIMESTAMP[] );
----Case 64 end----


----Case 65 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_65 ( IN  BOX[] ) RETURNS BOX[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinpara_unname_65(array['((1,2),(3,4))'::BOX,NULL]::BOX[]);

select * from pltest.PLPERLinpara_unname_65(array['((1,2),(3,4))'::BOX,NULL]::BOX[]);

DROP function pltest.PLPERLinpara_unname_65 ( IN  BOX[] );
----Case 65 end----


----Case 66 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_66 ( IN  TEXT[] ) RETURNS TEXT[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_66(pltest.tableTEXTarray.key) AS ID from pltest.tableTEXTarray ORDER BY ID;

create table pltest.tableTEXTarray1 as select pltest.PLPERLinpara_unname_66(pltest.tableTEXTarray.key) AS ID from pltest.tableTEXTarray DISTRIBUTED randomly;

select * from pltest.tableTEXTarray1 ORDER BY ID;

drop table pltest.tableTEXTarray1;

select pltest.PLPERLinpara_unname_66(array['QAZWSXEDCRFVTGB'::TEXT,NULL]::TEXT[]);

select * from pltest.PLPERLinpara_unname_66(array['QAZWSXEDCRFVTGB'::TEXT,NULL]::TEXT[]);

DROP function pltest.PLPERLinpara_unname_66 ( IN  TEXT[] );
----Case 66 end----


----Case 67 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_67 ( IN  BIT VARYING(10)[] ) RETURNS BIT VARYING(10)[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_67(pltest.tableBITVARYING10array.key) AS ID from pltest.tableBITVARYING10array ORDER BY ID;

create table pltest.tableBITVARYING10array1 as select pltest.PLPERLinpara_unname_67(pltest.tableBITVARYING10array.key) AS ID from pltest.tableBITVARYING10array DISTRIBUTED randomly;

select * from pltest.tableBITVARYING10array1 ORDER BY ID;

drop table pltest.tableBITVARYING10array1;

select pltest.PLPERLinpara_unname_67(array[B'11001111'::BIT VARYING(10),NULL]::BIT VARYING(10)[]);

select * from pltest.PLPERLinpara_unname_67(array[B'11001111'::BIT VARYING(10),NULL]::BIT VARYING(10)[]);

DROP function pltest.PLPERLinpara_unname_67 ( IN  BIT VARYING(10)[] );
----Case 67 end----


----Case 68 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_68 ( IN  NUMERIC[] ) RETURNS NUMERIC[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_68(pltest.tableNUMERICarray.key) AS ID from pltest.tableNUMERICarray ORDER BY ID;

create table pltest.tableNUMERICarray1 as select pltest.PLPERLinpara_unname_68(pltest.tableNUMERICarray.key) AS ID from pltest.tableNUMERICarray DISTRIBUTED randomly;

select * from pltest.tableNUMERICarray1 ORDER BY ID;

drop table pltest.tableNUMERICarray1;

select pltest.PLPERLinpara_unname_68(array[1234567890.0987654321::NUMERIC,'NAN'::NUMERIC,2::NUMERIC^128 / 3::NUMERIC^129,2::NUMERIC^128,0.5::NUMERIC,NULL]::NUMERIC[]);

select * from pltest.PLPERLinpara_unname_68(array[1234567890.0987654321::NUMERIC,'NAN'::NUMERIC,2::NUMERIC^128 / 3::NUMERIC^129,2::NUMERIC^128,0.5::NUMERIC,NULL]::NUMERIC[]);

DROP function pltest.PLPERLinpara_unname_68 ( IN  NUMERIC[] );
----Case 68 end----



----Case 70 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_70 ( IN  SMALLINT[] ) RETURNS SMALLINT[] as $$     
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinpara_unname_70(pltest.tableSMALLINTarray.key) AS ID from pltest.tableSMALLINTarray ORDER BY ID;

create table pltest.tableSMALLINTarray1 as select pltest.PLPERLinpara_unname_70(pltest.tableSMALLINTarray.key) AS ID from pltest.tableSMALLINTarray DISTRIBUTED randomly;

select * from pltest.tableSMALLINTarray1 ORDER BY ID;

drop table pltest.tableSMALLINTarray1;

select pltest.PLPERLinpara_unname_70(array[(-32768)::SMALLINT,32767::SMALLINT,1234::SMALLINT,NULL]::SMALLINT[]);

select * from pltest.PLPERLinpara_unname_70(array[(-32768)::SMALLINT,32767::SMALLINT,1234::SMALLINT,NULL]::SMALLINT[]);

DROP function pltest.PLPERLinpara_unname_70 ( IN  SMALLINT[] );
----Case 70 end----


----Case 71 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_71 ( IN  pltest.COMPOSITE_INTERNET ) RETURNS pltest.COMPOSITE_INTERNET as $$     
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinpara_unname_71(((-32768)::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET);

select pltest.PLPERLinpara_unname_71((32767::SMALLINT, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET);

select pltest.PLPERLinpara_unname_71((1234::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET);

select pltest.PLPERLinpara_unname_71((NULL, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET);

select (pltest.PLPERLinpara_unname_71(((-32768)::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET)).*;

select (pltest.PLPERLinpara_unname_71((32767::SMALLINT, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET)).*;

select (pltest.PLPERLinpara_unname_71((1234::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET)).*;

select (pltest.PLPERLinpara_unname_71((NULL, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET)).*;

select * from pltest.PLPERLinpara_unname_71(((-32768)::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET);

select * from pltest.PLPERLinpara_unname_71((32767::SMALLINT, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET);

select * from pltest.PLPERLinpara_unname_71((1234::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET);

select * from pltest.PLPERLinpara_unname_71((NULL, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET);

DROP function pltest.PLPERLinpara_unname_71 ( IN  pltest.COMPOSITE_INTERNET );
----Case 71 end----


----Case 72 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_72 ( IN  pltest.COMPOSITE_CHAR ) RETURNS pltest.COMPOSITE_CHAR as $$     
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinpara_unname_72(('SSSS'::CHAR(4), 'ASDFGHJKL'::VARCHAR(16), 'QAZWSXEDCRFVTGB'::TEXT)::pltest.COMPOSITE_CHAR);

select pltest.PLPERLinpara_unname_72((NULL, NULL, NULL)::pltest.COMPOSITE_CHAR);

select (pltest.PLPERLinpara_unname_72(('SSSS'::CHAR(4), 'ASDFGHJKL'::VARCHAR(16), 'QAZWSXEDCRFVTGB'::TEXT)::pltest.COMPOSITE_CHAR)).*;

select (pltest.PLPERLinpara_unname_72((NULL, NULL, NULL)::pltest.COMPOSITE_CHAR)).*;

select * from pltest.PLPERLinpara_unname_72(('SSSS'::CHAR(4), 'ASDFGHJKL'::VARCHAR(16), 'QAZWSXEDCRFVTGB'::TEXT)::pltest.COMPOSITE_CHAR);

select * from pltest.PLPERLinpara_unname_72((NULL, NULL, NULL)::pltest.COMPOSITE_CHAR);

DROP function pltest.PLPERLinpara_unname_72 ( IN  pltest.COMPOSITE_CHAR );
----Case 72 end----


----Case 73 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_73 ( IN  pltest.COMPOSITE_BIT ) RETURNS pltest.COMPOSITE_BIT as $$     
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinpara_unname_73(((-32768)::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), FALSE::BOOLEAN, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT);

select pltest.PLPERLinpara_unname_73((32767::SMALLINT, NULL, NULL, TRUE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT);

select pltest.PLPERLinpara_unname_73((1234::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), NULL, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT);

select pltest.PLPERLinpara_unname_73((NULL, NULL, NULL, FALSE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT);

select (pltest.PLPERLinpara_unname_73(((-32768)::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), FALSE::BOOLEAN, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT)).*;

select (pltest.PLPERLinpara_unname_73((32767::SMALLINT, NULL, NULL, TRUE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT)).*;

select (pltest.PLPERLinpara_unname_73((1234::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), NULL, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT)).*;

select (pltest.PLPERLinpara_unname_73((NULL, NULL, NULL, FALSE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT)).*;

select * from pltest.PLPERLinpara_unname_73(((-32768)::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), FALSE::BOOLEAN, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT);

select * from pltest.PLPERLinpara_unname_73((32767::SMALLINT, NULL, NULL, TRUE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT);

select * from pltest.PLPERLinpara_unname_73((1234::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), NULL, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT);

select * from pltest.PLPERLinpara_unname_73((NULL, NULL, NULL, FALSE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT);

DROP function pltest.PLPERLinpara_unname_73 ( IN  pltest.COMPOSITE_BIT );
----Case 73 end----


----Case 74 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_74 ( IN  pltest.COMPOSITE_GEOMETRIC ) RETURNS pltest.COMPOSITE_GEOMETRIC as $$     
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinpara_unname_74(((-32768)::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC);

select pltest.PLPERLinpara_unname_74((32767::SMALLINT, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC);

select pltest.PLPERLinpara_unname_74((1234::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC);

select pltest.PLPERLinpara_unname_74((NULL, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC);

select (pltest.PLPERLinpara_unname_74(((-32768)::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC)).*;

select (pltest.PLPERLinpara_unname_74((32767::SMALLINT, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC)).*;

select (pltest.PLPERLinpara_unname_74((1234::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC)).*;

select (pltest.PLPERLinpara_unname_74((NULL, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC)).*;

select * from pltest.PLPERLinpara_unname_74(((-32768)::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC);

select * from pltest.PLPERLinpara_unname_74((32767::SMALLINT, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC);

select * from pltest.PLPERLinpara_unname_74((1234::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC);

select * from pltest.PLPERLinpara_unname_74((NULL, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC);

DROP function pltest.PLPERLinpara_unname_74 ( IN  pltest.COMPOSITE_GEOMETRIC );
----Case 74 end----


----Case 75 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_75 ( IN  pltest.COMPOSITE_DATE ) RETURNS pltest.COMPOSITE_DATE as $$     
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinpara_unname_75(('2011-08-12'::DATE, '10:00:52.14'::TIME, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE);

select pltest.PLPERLinpara_unname_75(('EPOCH'::DATE, 'ALLBALLS'::TIME, NULL, NULL, NULL)::pltest.COMPOSITE_DATE);

select pltest.PLPERLinpara_unname_75((NULL, NULL, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE);

select (pltest.PLPERLinpara_unname_75(('2011-08-12'::DATE, '10:00:52.14'::TIME, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE)).*;

select (pltest.PLPERLinpara_unname_75(('EPOCH'::DATE, 'ALLBALLS'::TIME, NULL, NULL, NULL)::pltest.COMPOSITE_DATE)).*;

select (pltest.PLPERLinpara_unname_75((NULL, NULL, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE)).*;

select * from pltest.PLPERLinpara_unname_75(('2011-08-12'::DATE, '10:00:52.14'::TIME, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE);

select * from pltest.PLPERLinpara_unname_75(('EPOCH'::DATE, 'ALLBALLS'::TIME, NULL, NULL, NULL)::pltest.COMPOSITE_DATE);

select * from pltest.PLPERLinpara_unname_75((NULL, NULL, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE);

DROP function pltest.PLPERLinpara_unname_75 ( IN  pltest.COMPOSITE_DATE );
----Case 75 end----


----Case 76 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinpara_unname_76 ( IN  pltest.COMPOSITE_NUMERIC ) RETURNS pltest.COMPOSITE_NUMERIC as $$     
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinpara_unname_76(((-9223372036854775808)::BIGINT, 1234567890.0987654321::NUMERIC, 123456789.98765432::FLOAT8, (-2147483648)::INT4, '$-21474836.48'::MONEY, 39.333::REAL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select pltest.PLPERLinpara_unname_76((9223372036854775807::BIGINT, 'NAN'::NUMERIC, 'NAN'::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 'NAN'::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select pltest.PLPERLinpara_unname_76((123456789::BIGINT, 2::NUMERIC^128 / 3::NUMERIC^129, '-INFINITY'::FLOAT8, 1234, '$1234'::MONEY, '-INFINITY'::REAL, 1234::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select pltest.PLPERLinpara_unname_76((NULL, 2::NUMERIC^128, '+INFINITY'::FLOAT8, NULL, NULL, '+INFINITY'::REAL, NULL)::pltest.COMPOSITE_NUMERIC);

select pltest.PLPERLinpara_unname_76(((-9223372036854775808)::BIGINT, 0.5::NUMERIC, NULL, (-2147483648)::INT4, '$-21474836.48'::MONEY, NULL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select pltest.PLPERLinpara_unname_76((9223372036854775807::BIGINT, NULL, 123456789.98765432::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 39.333::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select (pltest.PLPERLinpara_unname_76(((-9223372036854775808)::BIGINT, 1234567890.0987654321::NUMERIC, 123456789.98765432::FLOAT8, (-2147483648)::INT4, '$-21474836.48'::MONEY, 39.333::REAL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC)).*;

select (pltest.PLPERLinpara_unname_76((9223372036854775807::BIGINT, 'NAN'::NUMERIC, 'NAN'::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 'NAN'::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC)).*;

select (pltest.PLPERLinpara_unname_76((123456789::BIGINT, 2::NUMERIC^128 / 3::NUMERIC^129, '-INFINITY'::FLOAT8, 1234, '$1234'::MONEY, '-INFINITY'::REAL, 1234::SMALLINT)::pltest.COMPOSITE_NUMERIC)).*;

select (pltest.PLPERLinpara_unname_76((NULL, 2::NUMERIC^128, '+INFINITY'::FLOAT8, NULL, NULL, '+INFINITY'::REAL, NULL)::pltest.COMPOSITE_NUMERIC)).*;

select (pltest.PLPERLinpara_unname_76(((-9223372036854775808)::BIGINT, 0.5::NUMERIC, NULL, (-2147483648)::INT4, '$-21474836.48'::MONEY, NULL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC)).*;

select (pltest.PLPERLinpara_unname_76((9223372036854775807::BIGINT, NULL, 123456789.98765432::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 39.333::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC)).*;

select * from pltest.PLPERLinpara_unname_76(((-9223372036854775808)::BIGINT, 1234567890.0987654321::NUMERIC, 123456789.98765432::FLOAT8, (-2147483648)::INT4, '$-21474836.48'::MONEY, 39.333::REAL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select * from pltest.PLPERLinpara_unname_76((9223372036854775807::BIGINT, 'NAN'::NUMERIC, 'NAN'::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 'NAN'::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select * from pltest.PLPERLinpara_unname_76((123456789::BIGINT, 2::NUMERIC^128 / 3::NUMERIC^129, '-INFINITY'::FLOAT8, 1234, '$1234'::MONEY, '-INFINITY'::REAL, 1234::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select * from pltest.PLPERLinpara_unname_76((NULL, 2::NUMERIC^128, '+INFINITY'::FLOAT8, NULL, NULL, '+INFINITY'::REAL, NULL)::pltest.COMPOSITE_NUMERIC);

select * from pltest.PLPERLinpara_unname_76(((-9223372036854775808)::BIGINT, 0.5::NUMERIC, NULL, (-2147483648)::INT4, '$-21474836.48'::MONEY, NULL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select * from pltest.PLPERLinpara_unname_76((9223372036854775807::BIGINT, NULL, 123456789.98765432::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 39.333::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC);

DROP function pltest.PLPERLinpara_unname_76 ( IN  pltest.COMPOSITE_NUMERIC );
----Case 76 end----

