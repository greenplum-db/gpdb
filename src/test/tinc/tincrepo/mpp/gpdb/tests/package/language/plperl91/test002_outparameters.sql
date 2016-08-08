----Case 77 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_77 ( IN  REAL , OUT  REAL ) RETURNS REAL as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_77(pltest.tableREAL.key) AS ID from pltest.tableREAL ORDER BY ID;

create table pltest.tableREAL1 as select pltest.PLPERLoutpara_unname_77(pltest.tableREAL.key) AS ID from pltest.tableREAL DISTRIBUTED randomly;

select * from pltest.tableREAL1 ORDER BY ID;

drop table pltest.tableREAL1;

select pltest.PLPERLoutpara_unname_77(39.333::REAL);

select pltest.PLPERLoutpara_unname_77('NAN'::REAL);

select pltest.PLPERLoutpara_unname_77('-INFINITY'::REAL);

select pltest.PLPERLoutpara_unname_77('+INFINITY'::REAL);

select pltest.PLPERLoutpara_unname_77(NULL);

select * from pltest.PLPERLoutpara_unname_77(39.333::REAL);

select * from pltest.PLPERLoutpara_unname_77('NAN'::REAL);

select * from pltest.PLPERLoutpara_unname_77('-INFINITY'::REAL);

select * from pltest.PLPERLoutpara_unname_77('+INFINITY'::REAL);

select * from pltest.PLPERLoutpara_unname_77(NULL);

DROP function pltest.PLPERLoutpara_unname_77 ( IN  REAL , OUT  REAL );
----Case 77 end----


----Case 78 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_78 ( IN  BOX , OUT  BOX ) RETURNS BOX as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLoutpara_unname_78('((1,2),(3,4))'::BOX);

select pltest.PLPERLoutpara_unname_78(NULL);

select * from pltest.PLPERLoutpara_unname_78('((1,2),(3,4))'::BOX);

select * from pltest.PLPERLoutpara_unname_78(NULL);

DROP function pltest.PLPERLoutpara_unname_78 ( IN  BOX , OUT  BOX );
----Case 78 end----


----Case 79 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_79 ( IN  VARCHAR(16) , OUT  VARCHAR(16) ) RETURNS VARCHAR(16) as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_79(pltest.tableVARCHAR16.key) AS ID from pltest.tableVARCHAR16 ORDER BY ID;

create table pltest.tableVARCHAR161 as select pltest.PLPERLoutpara_unname_79(pltest.tableVARCHAR16.key) AS ID from pltest.tableVARCHAR16 DISTRIBUTED randomly;

select * from pltest.tableVARCHAR161 ORDER BY ID;

drop table pltest.tableVARCHAR161;

select pltest.PLPERLoutpara_unname_79('ASDFGHJKL'::VARCHAR(16));

select pltest.PLPERLoutpara_unname_79(NULL);

select * from pltest.PLPERLoutpara_unname_79('ASDFGHJKL'::VARCHAR(16));

select * from pltest.PLPERLoutpara_unname_79(NULL);

DROP function pltest.PLPERLoutpara_unname_79 ( IN  VARCHAR(16) , OUT  VARCHAR(16) );
----Case 79 end----


----Case 80 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 
\echo '--ERROR:  PL/Perl functions cannot return type nyarray'		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_80 ( IN  ANYARRAY , OUT  ANYARRAY ) RETURNS ANYARRAY as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

----Case 80 end----


----Case 81 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_81 ( IN  UNKNOWN , OUT  UNKNOWN ) RETURNS UNKNOWN as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_81('QAZWSXEDCRFV'::UNKNOWN);

select pltest.PLPERLoutpara_unname_81(NULL::UNKNOWN);

select * from pltest.PLPERLoutpara_unname_81('QAZWSXEDCRFV'::UNKNOWN);

select * from pltest.PLPERLoutpara_unname_81(NULL::UNKNOWN);

DROP function pltest.PLPERLoutpara_unname_81 ( IN  UNKNOWN , OUT  UNKNOWN );
----Case 81 end----


----Case 82 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_82 ( IN  BIT VARYING(10) , OUT  BIT VARYING(10) ) RETURNS BIT VARYING(10) as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_82(pltest.tableBITVARYING10.key) AS ID from pltest.tableBITVARYING10 ORDER BY ID;

create table pltest.tableBITVARYING101 as select pltest.PLPERLoutpara_unname_82(pltest.tableBITVARYING10.key) AS ID from pltest.tableBITVARYING10 DISTRIBUTED randomly;

select * from pltest.tableBITVARYING101 ORDER BY ID;

drop table pltest.tableBITVARYING101;

select pltest.PLPERLoutpara_unname_82(B'11001111'::BIT VARYING(10));

select pltest.PLPERLoutpara_unname_82(NULL);

select * from pltest.PLPERLoutpara_unname_82(B'11001111'::BIT VARYING(10));

select * from pltest.PLPERLoutpara_unname_82(NULL);

DROP function pltest.PLPERLoutpara_unname_82 ( IN  BIT VARYING(10) , OUT  BIT VARYING(10) );
----Case 82 end----


----Case 83 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_83 ( IN  TIMESTAMPTZ , OUT  TIMESTAMPTZ ) RETURNS TIMESTAMPTZ as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_83(pltest.tableTIMESTAMPTZ.key) AS ID from pltest.tableTIMESTAMPTZ ORDER BY ID;

create table pltest.tableTIMESTAMPTZ1 as select pltest.PLPERLoutpara_unname_83(pltest.tableTIMESTAMPTZ.key) AS ID from pltest.tableTIMESTAMPTZ DISTRIBUTED randomly;

select * from pltest.tableTIMESTAMPTZ1 ORDER BY ID;

drop table pltest.tableTIMESTAMPTZ1;

select pltest.PLPERLoutpara_unname_83('2011-08-12 10:00:52.14'::TIMESTAMPTZ);

select pltest.PLPERLoutpara_unname_83(NULL);

select * from pltest.PLPERLoutpara_unname_83('2011-08-12 10:00:52.14'::TIMESTAMPTZ);

select * from pltest.PLPERLoutpara_unname_83(NULL);

DROP function pltest.PLPERLoutpara_unname_83 ( IN  TIMESTAMPTZ , OUT  TIMESTAMPTZ );
----Case 83 end----


----Case 84 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_84 ( IN  "char" , OUT  "char" ) RETURNS "char" as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_84(pltest.tableQUTOCHAR.key) AS ID from pltest.tableQUTOCHAR ORDER BY ID;

create table pltest.tableQUTOCHAR1 as select pltest.PLPERLoutpara_unname_84(pltest.tableQUTOCHAR.key) AS ID from pltest.tableQUTOCHAR DISTRIBUTED randomly;

select * from pltest.tableQUTOCHAR1 ORDER BY ID;

drop table pltest.tableQUTOCHAR1;

select pltest.PLPERLoutpara_unname_84('a'::"char");

select pltest.PLPERLoutpara_unname_84(null::"char");

select * from pltest.PLPERLoutpara_unname_84('a'::"char");

select * from pltest.PLPERLoutpara_unname_84(null::"char");

DROP function pltest.PLPERLoutpara_unname_84 ( IN  "char" , OUT  "char" );
----Case 84 end----


----Case 85 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_85 ( IN  CHAR , OUT  CHAR ) RETURNS CHAR as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_85(pltest.tableCHAR.key) AS ID from pltest.tableCHAR ORDER BY ID;

create table pltest.tableCHAR1 as select pltest.PLPERLoutpara_unname_85(pltest.tableCHAR.key) AS ID from pltest.tableCHAR DISTRIBUTED randomly;

select * from pltest.tableCHAR1 ORDER BY ID;

drop table pltest.tableCHAR1;

select pltest.PLPERLoutpara_unname_85('B'::CHAR);

select pltest.PLPERLoutpara_unname_85(NULL::CHAR);

select * from pltest.PLPERLoutpara_unname_85('B'::CHAR);

select * from pltest.PLPERLoutpara_unname_85(NULL::CHAR);

DROP function pltest.PLPERLoutpara_unname_85 ( IN  CHAR , OUT  CHAR );
----Case 85 end----


----Case 86 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_86 ( IN  BIGINT , OUT  BIGINT ) RETURNS BIGINT as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_86(pltest.tableBIGINT.key) AS ID from pltest.tableBIGINT ORDER BY ID;

create table pltest.tableBIGINT1 as select pltest.PLPERLoutpara_unname_86(pltest.tableBIGINT.key) AS ID from pltest.tableBIGINT DISTRIBUTED randomly;

select * from pltest.tableBIGINT1 ORDER BY ID;

drop table pltest.tableBIGINT1;

select pltest.PLPERLoutpara_unname_86((-9223372036854775808)::BIGINT);

select pltest.PLPERLoutpara_unname_86(9223372036854775807::BIGINT);

select pltest.PLPERLoutpara_unname_86(123456789::BIGINT);

select pltest.PLPERLoutpara_unname_86(NULL);

select * from pltest.PLPERLoutpara_unname_86((-9223372036854775808)::BIGINT);

select * from pltest.PLPERLoutpara_unname_86(9223372036854775807::BIGINT);

select * from pltest.PLPERLoutpara_unname_86(123456789::BIGINT);

select * from pltest.PLPERLoutpara_unname_86(NULL);

DROP function pltest.PLPERLoutpara_unname_86 ( IN  BIGINT , OUT  BIGINT );
----Case 86 end----


----Case 87 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_87 ( IN  DATE , OUT  DATE ) RETURNS DATE as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_87(pltest.tableDATE.key) AS ID from pltest.tableDATE ORDER BY ID;

create table pltest.tableDATE1 as select pltest.PLPERLoutpara_unname_87(pltest.tableDATE.key) AS ID from pltest.tableDATE DISTRIBUTED randomly;

select * from pltest.tableDATE1 ORDER BY ID;

drop table pltest.tableDATE1;

select pltest.PLPERLoutpara_unname_87('2011-08-12'::DATE);

select pltest.PLPERLoutpara_unname_87('EPOCH'::DATE);

select pltest.PLPERLoutpara_unname_87(NULL);

select * from pltest.PLPERLoutpara_unname_87('2011-08-12'::DATE);

select * from pltest.PLPERLoutpara_unname_87('EPOCH'::DATE);

select * from pltest.PLPERLoutpara_unname_87(NULL);

DROP function pltest.PLPERLoutpara_unname_87 ( IN  DATE , OUT  DATE );
----Case 87 end----


----Case 88 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_88 ( IN  PATH , OUT  PATH ) RETURNS PATH as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLoutpara_unname_88('(1,1),(2,2)'::PATH);

select pltest.PLPERLoutpara_unname_88(NULL);

select * from pltest.PLPERLoutpara_unname_88('(1,1),(2,2)'::PATH);

select * from pltest.PLPERLoutpara_unname_88(NULL);

DROP function pltest.PLPERLoutpara_unname_88 ( IN  PATH , OUT  PATH );
----Case 88 end----


----Case 89 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_89 ( IN  SMALLINT , OUT  SMALLINT ) RETURNS SMALLINT as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_89(pltest.tableSMALLINT.key) AS ID from pltest.tableSMALLINT ORDER BY ID;

create table pltest.tableSMALLINT1 as select pltest.PLPERLoutpara_unname_89(pltest.tableSMALLINT.key) AS ID from pltest.tableSMALLINT DISTRIBUTED randomly;

select * from pltest.tableSMALLINT1 ORDER BY ID;

drop table pltest.tableSMALLINT1;

select pltest.PLPERLoutpara_unname_89((-32768)::SMALLINT);

select pltest.PLPERLoutpara_unname_89(32767::SMALLINT);

select pltest.PLPERLoutpara_unname_89(1234::SMALLINT);

select pltest.PLPERLoutpara_unname_89(NULL);

select * from pltest.PLPERLoutpara_unname_89((-32768)::SMALLINT);

select * from pltest.PLPERLoutpara_unname_89(32767::SMALLINT);

select * from pltest.PLPERLoutpara_unname_89(1234::SMALLINT);

select * from pltest.PLPERLoutpara_unname_89(NULL);

DROP function pltest.PLPERLoutpara_unname_89 ( IN  SMALLINT , OUT  SMALLINT );
----Case 89 end----


----Case 90 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_90 ( IN  MACADDR , OUT  MACADDR ) RETURNS MACADDR as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_90(pltest.tableMACADDR.key) AS ID from pltest.tableMACADDR ORDER BY ID;

create table pltest.tableMACADDR1 as select pltest.PLPERLoutpara_unname_90(pltest.tableMACADDR.key) AS ID from pltest.tableMACADDR DISTRIBUTED randomly;

select * from pltest.tableMACADDR1 ORDER BY ID;

drop table pltest.tableMACADDR1;

select pltest.PLPERLoutpara_unname_90('12:34:56:78:90:AB'::MACADDR);

select pltest.PLPERLoutpara_unname_90(NULL);

select * from pltest.PLPERLoutpara_unname_90('12:34:56:78:90:AB'::MACADDR);

select * from pltest.PLPERLoutpara_unname_90(NULL);

DROP function pltest.PLPERLoutpara_unname_90 ( IN  MACADDR , OUT  MACADDR );
----Case 90 end----


----Case 91 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_91 ( IN  POINT , OUT  POINT ) RETURNS POINT as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLoutpara_unname_91('(1,2)'::POINT);

select pltest.PLPERLoutpara_unname_91(NULL);

select * from pltest.PLPERLoutpara_unname_91('(1,2)'::POINT);

select * from pltest.PLPERLoutpara_unname_91(NULL);

DROP function pltest.PLPERLoutpara_unname_91 ( IN  POINT , OUT  POINT );
----Case 91 end----


----Case 92 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_92 ( IN  INTERVAL , OUT  INTERVAL ) RETURNS INTERVAL as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_92(pltest.tableINTERVAL.key) AS ID from pltest.tableINTERVAL ORDER BY ID;

create table pltest.tableINTERVAL1 as select pltest.PLPERLoutpara_unname_92(pltest.tableINTERVAL.key) AS ID from pltest.tableINTERVAL DISTRIBUTED randomly;

select * from pltest.tableINTERVAL1 ORDER BY ID;

drop table pltest.tableINTERVAL1;

select pltest.PLPERLoutpara_unname_92('1 DAY 12 HOURS 59 MIN 10 SEC'::INTERVAL);

select pltest.PLPERLoutpara_unname_92('1 12:59:10'::INTERVAL);

select pltest.PLPERLoutpara_unname_92(NULL);

select * from pltest.PLPERLoutpara_unname_92('1 DAY 12 HOURS 59 MIN 10 SEC'::INTERVAL);

select * from pltest.PLPERLoutpara_unname_92('1 12:59:10'::INTERVAL);

select * from pltest.PLPERLoutpara_unname_92(NULL);

DROP function pltest.PLPERLoutpara_unname_92 ( IN  INTERVAL , OUT  INTERVAL );
----Case 92 end----


----Case 93 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_93 ( IN  NUMERIC , OUT  NUMERIC ) RETURNS NUMERIC as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_93(pltest.tableNUMERIC.key) AS ID from pltest.tableNUMERIC ORDER BY ID;

create table pltest.tableNUMERIC1 as select pltest.PLPERLoutpara_unname_93(pltest.tableNUMERIC.key) AS ID from pltest.tableNUMERIC DISTRIBUTED randomly;

select * from pltest.tableNUMERIC1 ORDER BY ID;

drop table pltest.tableNUMERIC1;

select pltest.PLPERLoutpara_unname_93(1234567890.0987654321::NUMERIC);

select pltest.PLPERLoutpara_unname_93('NAN'::NUMERIC);

select pltest.PLPERLoutpara_unname_93(2::NUMERIC^128 / 3::NUMERIC^129);

select pltest.PLPERLoutpara_unname_93(2::NUMERIC^128);

select pltest.PLPERLoutpara_unname_93(0.5::NUMERIC);

select pltest.PLPERLoutpara_unname_93(NULL);

select * from pltest.PLPERLoutpara_unname_93(1234567890.0987654321::NUMERIC);

select * from pltest.PLPERLoutpara_unname_93('NAN'::NUMERIC);

select * from pltest.PLPERLoutpara_unname_93(2::NUMERIC^128 / 3::NUMERIC^129);

select * from pltest.PLPERLoutpara_unname_93(2::NUMERIC^128);

select * from pltest.PLPERLoutpara_unname_93(0.5::NUMERIC);

select * from pltest.PLPERLoutpara_unname_93(NULL);

DROP function pltest.PLPERLoutpara_unname_93 ( IN  NUMERIC , OUT  NUMERIC );
----Case 93 end----


----Case 94 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_94 ( IN  INT4 , OUT  INT4 ) RETURNS INT4 as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_94((-2147483648)::INT4);

select pltest.PLPERLoutpara_unname_94(2147483647::INT4);

select pltest.PLPERLoutpara_unname_94(1234);

select pltest.PLPERLoutpara_unname_94(NULL);

select * from pltest.PLPERLoutpara_unname_94((-2147483648)::INT4);

select * from pltest.PLPERLoutpara_unname_94(2147483647::INT4);

select * from pltest.PLPERLoutpara_unname_94(1234);

select * from pltest.PLPERLoutpara_unname_94(NULL);

DROP function pltest.PLPERLoutpara_unname_94 ( IN  INT4 , OUT  INT4 );
----Case 94 end----


----Case 95 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_95 ( IN  CHAR(4) , OUT  CHAR(4) ) RETURNS CHAR(4) as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_95(pltest.tableCHAR4.key) AS ID from pltest.tableCHAR4 ORDER BY ID;

create table pltest.tableCHAR41 as select pltest.PLPERLoutpara_unname_95(pltest.tableCHAR4.key) AS ID from pltest.tableCHAR4 DISTRIBUTED randomly;

select * from pltest.tableCHAR41 ORDER BY ID;

drop table pltest.tableCHAR41;

select pltest.PLPERLoutpara_unname_95('SSSS'::CHAR(4));

select pltest.PLPERLoutpara_unname_95(NULL);

select * from pltest.PLPERLoutpara_unname_95('SSSS'::CHAR(4));

select * from pltest.PLPERLoutpara_unname_95(NULL);

DROP function pltest.PLPERLoutpara_unname_95 ( IN  CHAR(4) , OUT  CHAR(4) );
----Case 95 end----


----Case 96 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_96 ( IN  TEXT , OUT  TEXT ) RETURNS TEXT as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_96(pltest.tableTEXT.key) AS ID from pltest.tableTEXT ORDER BY ID;

create table pltest.tableTEXT1 as select pltest.PLPERLoutpara_unname_96(pltest.tableTEXT.key) AS ID from pltest.tableTEXT DISTRIBUTED randomly;

select * from pltest.tableTEXT1 ORDER BY ID;

drop table pltest.tableTEXT1;

select pltest.PLPERLoutpara_unname_96('QAZWSXEDCRFVTGB'::TEXT);

select pltest.PLPERLoutpara_unname_96(NULL);

select * from pltest.PLPERLoutpara_unname_96('QAZWSXEDCRFVTGB'::TEXT);

select * from pltest.PLPERLoutpara_unname_96(NULL);

DROP function pltest.PLPERLoutpara_unname_96 ( IN  TEXT , OUT  TEXT );
----Case 96 end----


----Case 97 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_97 ( IN  INET , OUT  INET ) RETURNS INET as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_97(pltest.tableINET.key) AS ID from pltest.tableINET ORDER BY ID;

create table pltest.tableINET1 as select pltest.PLPERLoutpara_unname_97(pltest.tableINET.key) AS ID from pltest.tableINET DISTRIBUTED randomly;

select * from pltest.tableINET1 ORDER BY ID;

drop table pltest.tableINET1;

select pltest.PLPERLoutpara_unname_97('192.168.0.1'::INET);

select pltest.PLPERLoutpara_unname_97(NULL);

select * from pltest.PLPERLoutpara_unname_97('192.168.0.1'::INET);

select * from pltest.PLPERLoutpara_unname_97(NULL);

DROP function pltest.PLPERLoutpara_unname_97 ( IN  INET , OUT  INET );
----Case 97 end----


----Case 98 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_98 ( IN  POLYGON , OUT  POLYGON ) RETURNS POLYGON as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLoutpara_unname_98('(0,0),(1,1),(2,0)'::POLYGON);

select pltest.PLPERLoutpara_unname_98(NULL);

select * from pltest.PLPERLoutpara_unname_98('(0,0),(1,1),(2,0)'::POLYGON);

select * from pltest.PLPERLoutpara_unname_98(NULL);

DROP function pltest.PLPERLoutpara_unname_98 ( IN  POLYGON , OUT  POLYGON );
----Case 98 end----


----Case 99 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 	
\echo '--ERROR:  PL/Perl functions cannot return type cstring'	
CREATE or REPLACE function pltest.PLPERLoutpara_unname_99 ( IN  CSTRING , OUT  CSTRING ) RETURNS CSTRING as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

----Case 99 end----


----Case 100 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_100 ( IN  LSEG , OUT  LSEG ) RETURNS LSEG as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLoutpara_unname_100('((1,1),(2,2))'::LSEG);

select pltest.PLPERLoutpara_unname_100(NULL);

select * from pltest.PLPERLoutpara_unname_100('((1,1),(2,2))'::LSEG);

select * from pltest.PLPERLoutpara_unname_100(NULL);

DROP function pltest.PLPERLoutpara_unname_100 ( IN  LSEG , OUT  LSEG );
----Case 100 end----


----Case 101 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
\echo '--ERROR:  PL/Perl functions cannot return type anyelement'
CREATE or REPLACE function pltest.PLPERLoutpara_unname_101 ( IN  ANYELEMENT , OUT  ANYELEMENT ) RETURNS ANYELEMENT as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

----Case 101 end----


----Case 102 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_102 ( IN  CIDR , OUT  CIDR ) RETURNS CIDR as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_102(pltest.tableCIDR.key) AS ID from pltest.tableCIDR ORDER BY ID;

create table pltest.tableCIDR1 as select pltest.PLPERLoutpara_unname_102(pltest.tableCIDR.key) AS ID from pltest.tableCIDR DISTRIBUTED randomly;

select * from pltest.tableCIDR1 ORDER BY ID;

drop table pltest.tableCIDR1;

select pltest.PLPERLoutpara_unname_102('192.168.0.1/32'::CIDR);

select pltest.PLPERLoutpara_unname_102(NULL);

select * from pltest.PLPERLoutpara_unname_102('192.168.0.1/32'::CIDR);

select * from pltest.PLPERLoutpara_unname_102(NULL);

DROP function pltest.PLPERLoutpara_unname_102 ( IN  CIDR , OUT  CIDR );
----Case 102 end----


----Case 103 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_103 ( IN  BYTEA , OUT  BYTEA ) RETURNS BYTEA as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_103(pltest.tableBYTEA.key) AS ID from pltest.tableBYTEA ORDER BY ID;

create table pltest.tableBYTEA1 as select pltest.PLPERLoutpara_unname_103(pltest.tableBYTEA.key) AS ID from pltest.tableBYTEA DISTRIBUTED randomly;

select * from pltest.tableBYTEA1 ORDER BY ID;

drop table pltest.tableBYTEA1;

select pltest.PLPERLoutpara_unname_103(E'\\000'::BYTEA);

select pltest.PLPERLoutpara_unname_103(NULL);

select * from pltest.PLPERLoutpara_unname_103(E'\\000'::BYTEA);

select * from pltest.PLPERLoutpara_unname_103(NULL);

DROP function pltest.PLPERLoutpara_unname_103 ( IN  BYTEA , OUT  BYTEA );
----Case 103 end----


----Case 104 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_104 ( IN  MONEY , OUT  MONEY ) RETURNS MONEY as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_104(pltest.tableMONEY.key) AS ID from pltest.tableMONEY ORDER BY ID;

create table pltest.tableMONEY1 as select pltest.PLPERLoutpara_unname_104(pltest.tableMONEY.key) AS ID from pltest.tableMONEY DISTRIBUTED randomly;

select * from pltest.tableMONEY1 ORDER BY ID;

drop table pltest.tableMONEY1;

select pltest.PLPERLoutpara_unname_104('$-21474836.48'::MONEY);

select pltest.PLPERLoutpara_unname_104('$21474836.47'::MONEY);

select pltest.PLPERLoutpara_unname_104('$1234'::MONEY);

select pltest.PLPERLoutpara_unname_104(NULL);

select * from pltest.PLPERLoutpara_unname_104('$-21474836.48'::MONEY);

select * from pltest.PLPERLoutpara_unname_104('$21474836.47'::MONEY);

select * from pltest.PLPERLoutpara_unname_104('$1234'::MONEY);

select * from pltest.PLPERLoutpara_unname_104(NULL);

DROP function pltest.PLPERLoutpara_unname_104 ( IN  MONEY , OUT  MONEY );
----Case 104 end----


----Case 105 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_105 ( IN  TIMESTAMP , OUT  TIMESTAMP ) RETURNS TIMESTAMP as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_105(pltest.tableTIMESTAMP.key) AS ID from pltest.tableTIMESTAMP ORDER BY ID;

create table pltest.tableTIMESTAMP1 as select pltest.PLPERLoutpara_unname_105(pltest.tableTIMESTAMP.key) AS ID from pltest.tableTIMESTAMP DISTRIBUTED randomly;

select * from pltest.tableTIMESTAMP1 ORDER BY ID;

drop table pltest.tableTIMESTAMP1;

select pltest.PLPERLoutpara_unname_105('2011-08-12 10:00:52.14'::TIMESTAMP);

select pltest.PLPERLoutpara_unname_105(NULL);

select * from pltest.PLPERLoutpara_unname_105('2011-08-12 10:00:52.14'::TIMESTAMP);

select * from pltest.PLPERLoutpara_unname_105(NULL);

DROP function pltest.PLPERLoutpara_unname_105 ( IN  TIMESTAMP , OUT  TIMESTAMP );
----Case 105 end----


----Case 106 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_106 ( IN  BOOLEAN , OUT  BOOLEAN ) RETURNS BOOLEAN as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_106(pltest.tableBOOLEAN.key) AS ID from pltest.tableBOOLEAN ORDER BY ID;

create table pltest.tableBOOLEAN1 as select pltest.PLPERLoutpara_unname_106(pltest.tableBOOLEAN.key) AS ID from pltest.tableBOOLEAN DISTRIBUTED randomly;

select * from pltest.tableBOOLEAN1 ORDER BY ID;

drop table pltest.tableBOOLEAN1;

select pltest.PLPERLoutpara_unname_106(FALSE::BOOLEAN);

select pltest.PLPERLoutpara_unname_106(TRUE::BOOLEAN);

select pltest.PLPERLoutpara_unname_106(NULL);

select * from pltest.PLPERLoutpara_unname_106(FALSE::BOOLEAN);

select * from pltest.PLPERLoutpara_unname_106(TRUE::BOOLEAN);

select * from pltest.PLPERLoutpara_unname_106(NULL);

DROP function pltest.PLPERLoutpara_unname_106 ( IN  BOOLEAN , OUT  BOOLEAN );
----Case 106 end----


----Case 107 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_107 ( IN  FLOAT8 , OUT  FLOAT8 ) RETURNS FLOAT8 as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_107(pltest.tableFLOAT8.key) AS ID from pltest.tableFLOAT8 ORDER BY ID;

create table pltest.tableFLOAT81 as select pltest.PLPERLoutpara_unname_107(pltest.tableFLOAT8.key) AS ID from pltest.tableFLOAT8 DISTRIBUTED randomly;

select * from pltest.tableFLOAT81 ORDER BY ID;

drop table pltest.tableFLOAT81;

select pltest.PLPERLoutpara_unname_107(123456789.98765432::FLOAT8);

select pltest.PLPERLoutpara_unname_107('NAN'::FLOAT8);

select pltest.PLPERLoutpara_unname_107('-INFINITY'::FLOAT8);

select pltest.PLPERLoutpara_unname_107('+INFINITY'::FLOAT8);

select pltest.PLPERLoutpara_unname_107(NULL);

select * from pltest.PLPERLoutpara_unname_107(123456789.98765432::FLOAT8);

select * from pltest.PLPERLoutpara_unname_107('NAN'::FLOAT8);

select * from pltest.PLPERLoutpara_unname_107('-INFINITY'::FLOAT8);

select * from pltest.PLPERLoutpara_unname_107('+INFINITY'::FLOAT8);

select * from pltest.PLPERLoutpara_unname_107(NULL);

DROP function pltest.PLPERLoutpara_unname_107 ( IN  FLOAT8 , OUT  FLOAT8 );
----Case 107 end----


----Case 108 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_108 ( IN  TIME , OUT  TIME ) RETURNS TIME as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_108(pltest.tableTIME.key) AS ID from pltest.tableTIME ORDER BY ID;

create table pltest.tableTIME1 as select pltest.PLPERLoutpara_unname_108(pltest.tableTIME.key) AS ID from pltest.tableTIME DISTRIBUTED randomly;

select * from pltest.tableTIME1 ORDER BY ID;

drop table pltest.tableTIME1;

select pltest.PLPERLoutpara_unname_108('10:00:52.14'::TIME);

select pltest.PLPERLoutpara_unname_108('ALLBALLS'::TIME);

select pltest.PLPERLoutpara_unname_108(NULL);

select * from pltest.PLPERLoutpara_unname_108('10:00:52.14'::TIME);

select * from pltest.PLPERLoutpara_unname_108('ALLBALLS'::TIME);

select * from pltest.PLPERLoutpara_unname_108(NULL);

DROP function pltest.PLPERLoutpara_unname_108 ( IN  TIME , OUT  TIME );
----Case 108 end----


----Case 109 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_109 ( IN  CIRCLE , OUT  CIRCLE ) RETURNS CIRCLE as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLoutpara_unname_109('<(1,1),2>'::CIRCLE);

select pltest.PLPERLoutpara_unname_109(NULL);

select * from pltest.PLPERLoutpara_unname_109('<(1,1),2>'::CIRCLE);

select * from pltest.PLPERLoutpara_unname_109(NULL);

DROP function pltest.PLPERLoutpara_unname_109 ( IN  CIRCLE , OUT  CIRCLE );
----Case 109 end----


----Case 110 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_110 ( IN  TIMETZ , OUT  TIMETZ ) RETURNS TIMETZ as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_110(pltest.tableTIMETZ.key) AS ID from pltest.tableTIMETZ ORDER BY ID;

create table pltest.tableTIMETZ1 as select pltest.PLPERLoutpara_unname_110(pltest.tableTIMETZ.key) AS ID from pltest.tableTIMETZ DISTRIBUTED randomly;

select * from pltest.tableTIMETZ1 ORDER BY ID;

drop table pltest.tableTIMETZ1;

select pltest.PLPERLoutpara_unname_110('2011-08-12 10:00:52.14'::TIMETZ);

select pltest.PLPERLoutpara_unname_110(NULL);

select * from pltest.PLPERLoutpara_unname_110('2011-08-12 10:00:52.14'::TIMETZ);

select * from pltest.PLPERLoutpara_unname_110(NULL);

DROP function pltest.PLPERLoutpara_unname_110 ( IN  TIMETZ , OUT  TIMETZ );
----Case 110 end----


----Case 111 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_111 ( IN  BIT(8) , OUT  BIT(8) ) RETURNS BIT(8) as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_111(pltest.tableBIT8.key) AS ID from pltest.tableBIT8 ORDER BY ID;

create table pltest.tableBIT81 as select pltest.PLPERLoutpara_unname_111(pltest.tableBIT8.key) AS ID from pltest.tableBIT8 DISTRIBUTED randomly;

select * from pltest.tableBIT81 ORDER BY ID;

drop table pltest.tableBIT81;

select pltest.PLPERLoutpara_unname_111(B'11001111'::BIT(8));

select pltest.PLPERLoutpara_unname_111(NULL);

select * from pltest.PLPERLoutpara_unname_111(B'11001111'::BIT(8));

select * from pltest.PLPERLoutpara_unname_111(NULL);

DROP function pltest.PLPERLoutpara_unname_111 ( IN  BIT(8) , OUT  BIT(8) );
----Case 111 end----


----Case 112 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
--
\echo '--ERROR:  syntax error at or near "ANY"' 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_112 ( IN  ANY , OUT  ANY ) RETURNS ANY as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

----Case 112 end----


----Case 113 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_113 ( IN  BOOLEAN[] , OUT  BOOLEAN[] ) RETURNS BOOLEAN[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_113(pltest.tableBOOLEANarray.key) AS ID from pltest.tableBOOLEANarray ORDER BY ID;

create table pltest.tableBOOLEANarray1 as select pltest.PLPERLoutpara_unname_113(pltest.tableBOOLEANarray.key) AS ID from pltest.tableBOOLEANarray DISTRIBUTED randomly;

select * from pltest.tableBOOLEANarray1 ORDER BY ID;

drop table pltest.tableBOOLEANarray1;

select pltest.PLPERLoutpara_unname_113(array[FALSE::BOOLEAN,TRUE::BOOLEAN,NULL]::BOOLEAN[]);

select * from pltest.PLPERLoutpara_unname_113(array[FALSE::BOOLEAN,TRUE::BOOLEAN,NULL]::BOOLEAN[]);

DROP function pltest.PLPERLoutpara_unname_113 ( IN  BOOLEAN[] , OUT  BOOLEAN[] );
----Case 113 end----


----Case 114 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_114 ( IN  INTERVAL[] , OUT  INTERVAL[] ) RETURNS INTERVAL[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_114(pltest.tableINTERVALarray.key) AS ID from pltest.tableINTERVALarray ORDER BY ID;

create table pltest.tableINTERVALarray1 as select pltest.PLPERLoutpara_unname_114(pltest.tableINTERVALarray.key) AS ID from pltest.tableINTERVALarray DISTRIBUTED randomly;

select * from pltest.tableINTERVALarray1 ORDER BY ID;

drop table pltest.tableINTERVALarray1;

select pltest.PLPERLoutpara_unname_114(array['1 DAY 12 HOURS 59 MIN 10 SEC'::INTERVAL,'1 12:59:10'::INTERVAL,NULL]::INTERVAL[]);

select * from pltest.PLPERLoutpara_unname_114(array['1 DAY 12 HOURS 59 MIN 10 SEC'::INTERVAL,'1 12:59:10'::INTERVAL,NULL]::INTERVAL[]);

DROP function pltest.PLPERLoutpara_unname_114 ( IN  INTERVAL[] , OUT  INTERVAL[] );
----Case 114 end----


----Case 115 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_115 ( IN  BIT(8)[] , OUT  BIT(8)[] ) RETURNS BIT(8)[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_115(pltest.tableBIT8array.key) AS ID from pltest.tableBIT8array ORDER BY ID;

create table pltest.tableBIT8array1 as select pltest.PLPERLoutpara_unname_115(pltest.tableBIT8array.key) AS ID from pltest.tableBIT8array DISTRIBUTED randomly;

select * from pltest.tableBIT8array1 ORDER BY ID;

drop table pltest.tableBIT8array1;

select pltest.PLPERLoutpara_unname_115(array[B'11001111'::BIT(8),NULL]::BIT(8)[]);

select * from pltest.PLPERLoutpara_unname_115(array[B'11001111'::BIT(8),NULL]::BIT(8)[]);

DROP function pltest.PLPERLoutpara_unname_115 ( IN  BIT(8)[] , OUT  BIT(8)[] );
----Case 115 end----


----Case 116 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_116 ( IN  TIMETZ[] , OUT  TIMETZ[] ) RETURNS TIMETZ[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_116(pltest.tableTIMETZarray.key) AS ID from pltest.tableTIMETZarray ORDER BY ID;

create table pltest.tableTIMETZarray1 as select pltest.PLPERLoutpara_unname_116(pltest.tableTIMETZarray.key) AS ID from pltest.tableTIMETZarray DISTRIBUTED randomly;

select * from pltest.tableTIMETZarray1 ORDER BY ID;

drop table pltest.tableTIMETZarray1;

select pltest.PLPERLoutpara_unname_116(array['2011-08-12 10:00:52.14'::TIMETZ,NULL]::TIMETZ[]);

select * from pltest.PLPERLoutpara_unname_116(array['2011-08-12 10:00:52.14'::TIMETZ,NULL]::TIMETZ[]);

DROP function pltest.PLPERLoutpara_unname_116 ( IN  TIMETZ[] , OUT  TIMETZ[] );
----Case 116 end----


----Case 117 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_117 ( IN  CIRCLE[] , OUT  CIRCLE[] ) RETURNS CIRCLE[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLoutpara_unname_117(array['<(1,1),2>'::CIRCLE,NULL]::CIRCLE[]);

select * from pltest.PLPERLoutpara_unname_117(array['<(1,1),2>'::CIRCLE,NULL]::CIRCLE[]);

DROP function pltest.PLPERLoutpara_unname_117 ( IN  CIRCLE[] , OUT  CIRCLE[] );
----Case 117 end----


----Case 118 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_118 ( IN  BYTEA[] , OUT  BYTEA[] ) RETURNS BYTEA[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_118(pltest.tableBYTEAarray.key) AS ID from pltest.tableBYTEAarray ORDER BY ID;

create table pltest.tableBYTEAarray1 as select pltest.PLPERLoutpara_unname_118(pltest.tableBYTEAarray.key) AS ID from pltest.tableBYTEAarray DISTRIBUTED randomly;

select * from pltest.tableBYTEAarray1 ORDER BY ID;

drop table pltest.tableBYTEAarray1;

select pltest.PLPERLoutpara_unname_118(array[E'\\000'::BYTEA,NULL]::BYTEA[]);

select * from pltest.PLPERLoutpara_unname_118(array[E'\\000'::BYTEA,NULL]::BYTEA[]);

DROP function pltest.PLPERLoutpara_unname_118 ( IN  BYTEA[] , OUT  BYTEA[] );
----Case 118 end----


----Case 119 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_119 ( IN  TIME[] , OUT  TIME[] ) RETURNS TIME[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_119(pltest.tableTIMEarray.key) AS ID from pltest.tableTIMEarray ORDER BY ID;

create table pltest.tableTIMEarray1 as select pltest.PLPERLoutpara_unname_119(pltest.tableTIMEarray.key) AS ID from pltest.tableTIMEarray DISTRIBUTED randomly;

select * from pltest.tableTIMEarray1 ORDER BY ID;

drop table pltest.tableTIMEarray1;

select pltest.PLPERLoutpara_unname_119(array['10:00:52.14'::TIME,'ALLBALLS'::TIME,NULL]::TIME[]);

select * from pltest.PLPERLoutpara_unname_119(array['10:00:52.14'::TIME,'ALLBALLS'::TIME,NULL]::TIME[]);

DROP function pltest.PLPERLoutpara_unname_119 ( IN  TIME[] , OUT  TIME[] );
----Case 119 end----


----Case 120 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_120 ( IN  REAL[] , OUT  REAL[] ) RETURNS REAL[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_120(pltest.tableREALarray.key) AS ID from pltest.tableREALarray ORDER BY ID;

create table pltest.tableREALarray1 as select pltest.PLPERLoutpara_unname_120(pltest.tableREALarray.key) AS ID from pltest.tableREALarray DISTRIBUTED randomly;

select * from pltest.tableREALarray1 ORDER BY ID;

drop table pltest.tableREALarray1;

select pltest.PLPERLoutpara_unname_120(array[39.333::REAL,'NAN'::REAL,'-INFINITY'::REAL,'+INFINITY'::REAL,NULL]::REAL[]);

select * from pltest.PLPERLoutpara_unname_120(array[39.333::REAL,'NAN'::REAL,'-INFINITY'::REAL,'+INFINITY'::REAL,NULL]::REAL[]);

DROP function pltest.PLPERLoutpara_unname_120 ( IN  REAL[] , OUT  REAL[] );
----Case 120 end----




----Case 122 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_122 ( IN  MONEY[] , OUT  MONEY[] ) RETURNS MONEY[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_122(pltest.tableMONEYarray.key) AS ID from pltest.tableMONEYarray ORDER BY ID;

create table pltest.tableMONEYarray1 as select pltest.PLPERLoutpara_unname_122(pltest.tableMONEYarray.key) AS ID from pltest.tableMONEYarray DISTRIBUTED randomly;

select * from pltest.tableMONEYarray1 ORDER BY ID;

drop table pltest.tableMONEYarray1;

select pltest.PLPERLoutpara_unname_122(array['$-21474836.48'::MONEY,'$21474836.47'::MONEY,'$1234'::MONEY,NULL]::MONEY[]);

select * from pltest.PLPERLoutpara_unname_122(array['$-21474836.48'::MONEY,'$21474836.47'::MONEY,'$1234'::MONEY,NULL]::MONEY[]);

DROP function pltest.PLPERLoutpara_unname_122 ( IN  MONEY[] , OUT  MONEY[] );
----Case 122 end----


----Case 123 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_123 ( IN  POINT[] , OUT  POINT[] ) RETURNS POINT[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLoutpara_unname_123(array['(1,2)'::POINT,NULL]::POINT[]);

select * from pltest.PLPERLoutpara_unname_123(array['(1,2)'::POINT,NULL]::POINT[]);

DROP function pltest.PLPERLoutpara_unname_123 ( IN  POINT[] , OUT  POINT[] );
----Case 123 end----


----Case 124 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_124 ( IN  CHAR[] , OUT  CHAR[] ) RETURNS CHAR[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_124(pltest.tableCHARarray.key) AS ID from pltest.tableCHARarray ORDER BY ID;

create table pltest.tableCHARarray1 as select pltest.PLPERLoutpara_unname_124(pltest.tableCHARarray.key) AS ID from pltest.tableCHARarray DISTRIBUTED randomly;

select * from pltest.tableCHARarray1 ORDER BY ID;

drop table pltest.tableCHARarray1;

select pltest.PLPERLoutpara_unname_124(array['B'::CHAR,NULL::CHAR]::CHAR[]);

select * from pltest.PLPERLoutpara_unname_124(array['B'::CHAR,NULL::CHAR]::CHAR[]);

DROP function pltest.PLPERLoutpara_unname_124 ( IN  CHAR[] , OUT  CHAR[] );
----Case 124 end----


----Case 125 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_125 ( IN  MACADDR[] , OUT  MACADDR[] ) RETURNS MACADDR[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_125(pltest.tableMACADDRarray.key) AS ID from pltest.tableMACADDRarray ORDER BY ID;

create table pltest.tableMACADDRarray1 as select pltest.PLPERLoutpara_unname_125(pltest.tableMACADDRarray.key) AS ID from pltest.tableMACADDRarray DISTRIBUTED randomly;

select * from pltest.tableMACADDRarray1 ORDER BY ID;

drop table pltest.tableMACADDRarray1;

select pltest.PLPERLoutpara_unname_125(array['12:34:56:78:90:AB'::MACADDR,NULL]::MACADDR[]);

select * from pltest.PLPERLoutpara_unname_125(array['12:34:56:78:90:AB'::MACADDR,NULL]::MACADDR[]);

DROP function pltest.PLPERLoutpara_unname_125 ( IN  MACADDR[] , OUT  MACADDR[] );
----Case 125 end----


----Case 126 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_126 ( IN  FLOAT8[] , OUT  FLOAT8[] ) RETURNS FLOAT8[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_126(pltest.tableFLOAT8array.key) AS ID from pltest.tableFLOAT8array ORDER BY ID;

create table pltest.tableFLOAT8array1 as select pltest.PLPERLoutpara_unname_126(pltest.tableFLOAT8array.key) AS ID from pltest.tableFLOAT8array DISTRIBUTED randomly;

select * from pltest.tableFLOAT8array1 ORDER BY ID;

drop table pltest.tableFLOAT8array1;

select pltest.PLPERLoutpara_unname_126(array[123456789.98765432::FLOAT8,'NAN'::FLOAT8,'-INFINITY'::FLOAT8,'+INFINITY'::FLOAT8,NULL]::FLOAT8[]);

select * from pltest.PLPERLoutpara_unname_126(array[123456789.98765432::FLOAT8,'NAN'::FLOAT8,'-INFINITY'::FLOAT8,'+INFINITY'::FLOAT8,NULL]::FLOAT8[]);

DROP function pltest.PLPERLoutpara_unname_126 ( IN  FLOAT8[] , OUT  FLOAT8[] );
----Case 126 end----


----Case 127 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_127 ( IN  LSEG[] , OUT  LSEG[] ) RETURNS LSEG[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLoutpara_unname_127(array['((1,1),(2,2))'::LSEG,NULL]::LSEG[]);

select * from pltest.PLPERLoutpara_unname_127(array['((1,1),(2,2))'::LSEG,NULL]::LSEG[]);

DROP function pltest.PLPERLoutpara_unname_127 ( IN  LSEG[] , OUT  LSEG[] );
----Case 127 end----


----Case 128 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_128 ( IN  DATE[] , OUT  DATE[] ) RETURNS DATE[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_128(pltest.tableDATEarray.key) AS ID from pltest.tableDATEarray ORDER BY ID;

create table pltest.tableDATEarray1 as select pltest.PLPERLoutpara_unname_128(pltest.tableDATEarray.key) AS ID from pltest.tableDATEarray DISTRIBUTED randomly;

select * from pltest.tableDATEarray1 ORDER BY ID;

drop table pltest.tableDATEarray1;

select pltest.PLPERLoutpara_unname_128(array['2011-08-12'::DATE,'EPOCH'::DATE,NULL]::DATE[]);

select * from pltest.PLPERLoutpara_unname_128(array['2011-08-12'::DATE,'EPOCH'::DATE,NULL]::DATE[]);

DROP function pltest.PLPERLoutpara_unname_128 ( IN  DATE[] , OUT  DATE[] );
----Case 128 end----


----Case 129 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_129 ( IN  BIGINT[] , OUT  BIGINT[] ) RETURNS BIGINT[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_129(pltest.tableBIGINTarray.key) AS ID from pltest.tableBIGINTarray ORDER BY ID;

create table pltest.tableBIGINTarray1 as select pltest.PLPERLoutpara_unname_129(pltest.tableBIGINTarray.key) AS ID from pltest.tableBIGINTarray DISTRIBUTED randomly;

select * from pltest.tableBIGINTarray1 ORDER BY ID;

drop table pltest.tableBIGINTarray1;

select pltest.PLPERLoutpara_unname_129(array[(-9223372036854775808)::BIGINT,9223372036854775807::BIGINT,123456789::BIGINT,NULL]::BIGINT[]);

select * from pltest.PLPERLoutpara_unname_129(array[(-9223372036854775808)::BIGINT,9223372036854775807::BIGINT,123456789::BIGINT,NULL]::BIGINT[]);

DROP function pltest.PLPERLoutpara_unname_129 ( IN  BIGINT[] , OUT  BIGINT[] );
----Case 129 end----




----Case 131 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_131 ( IN  PATH[] , OUT  PATH[] ) RETURNS PATH[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLoutpara_unname_131(array['(1,1),(2,2)'::PATH,NULL]::PATH[]);

select * from pltest.PLPERLoutpara_unname_131(array['(1,1),(2,2)'::PATH,NULL]::PATH[]);

DROP function pltest.PLPERLoutpara_unname_131 ( IN  PATH[] , OUT  PATH[] );
----Case 131 end----


----Case 132 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_132 ( IN  POLYGON[] , OUT  POLYGON[] ) RETURNS POLYGON[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;



select pltest.PLPERLoutpara_unname_132(array['(0,0),(1,1),(2,0)'::POLYGON,NULL]::POLYGON[]);

select * from pltest.PLPERLoutpara_unname_132(array['(0,0),(1,1),(2,0)'::POLYGON,NULL]::POLYGON[]);

DROP function pltest.PLPERLoutpara_unname_132 ( IN  POLYGON[] , OUT  POLYGON[] );
----Case 132 end----


----Case 133 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_133 ( IN  INET[] , OUT  INET[] ) RETURNS INET[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_133(pltest.tableINETarray.key) AS ID from pltest.tableINETarray ORDER BY ID;

create table pltest.tableINETarray1 as select pltest.PLPERLoutpara_unname_133(pltest.tableINETarray.key) AS ID from pltest.tableINETarray DISTRIBUTED randomly;

select * from pltest.tableINETarray1 ORDER BY ID;

drop table pltest.tableINETarray1;

select pltest.PLPERLoutpara_unname_133(array['192.168.0.1'::INET,NULL]::INET[]);

select * from pltest.PLPERLoutpara_unname_133(array['192.168.0.1'::INET,NULL]::INET[]);

DROP function pltest.PLPERLoutpara_unname_133 ( IN  INET[] , OUT  INET[] );
----Case 133 end----




----Case 135 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_135 ( IN  TIMESTAMPTZ[] , OUT  TIMESTAMPTZ[] ) RETURNS TIMESTAMPTZ[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_135(pltest.tableTIMESTAMPTZarray.key) AS ID from pltest.tableTIMESTAMPTZarray ORDER BY ID;

create table pltest.tableTIMESTAMPTZarray1 as select pltest.PLPERLoutpara_unname_135(pltest.tableTIMESTAMPTZarray.key) AS ID from pltest.tableTIMESTAMPTZarray DISTRIBUTED randomly;

select * from pltest.tableTIMESTAMPTZarray1 ORDER BY ID;

drop table pltest.tableTIMESTAMPTZarray1;

select pltest.PLPERLoutpara_unname_135(array['2011-08-12 10:00:52.14'::TIMESTAMPTZ,NULL]::TIMESTAMPTZ[]);

select * from pltest.PLPERLoutpara_unname_135(array['2011-08-12 10:00:52.14'::TIMESTAMPTZ,NULL]::TIMESTAMPTZ[]);

DROP function pltest.PLPERLoutpara_unname_135 ( IN  TIMESTAMPTZ[] , OUT  TIMESTAMPTZ[] );
----Case 135 end----


----Case 136 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_136 ( IN  CHAR(4)[] , OUT  CHAR(4)[] ) RETURNS CHAR(4)[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_136(pltest.tableCHAR4array.key) AS ID from pltest.tableCHAR4array ORDER BY ID;

create table pltest.tableCHAR4array1 as select pltest.PLPERLoutpara_unname_136(pltest.tableCHAR4array.key) AS ID from pltest.tableCHAR4array DISTRIBUTED randomly;

select * from pltest.tableCHAR4array1 ORDER BY ID;

drop table pltest.tableCHAR4array1;

select pltest.PLPERLoutpara_unname_136(array['SSSS'::CHAR(4),NULL]::CHAR(4)[]);

select * from pltest.PLPERLoutpara_unname_136(array['SSSS'::CHAR(4),NULL]::CHAR(4)[]);

DROP function pltest.PLPERLoutpara_unname_136 ( IN  CHAR(4)[] , OUT  CHAR(4)[] );
----Case 136 end----


----Case 137 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_137 ( IN  VARCHAR(16)[] , OUT  VARCHAR(16)[] ) RETURNS VARCHAR(16)[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_137(pltest.tableVARCHAR16array.key) AS ID from pltest.tableVARCHAR16array ORDER BY ID;

create table pltest.tableVARCHAR16array1 as select pltest.PLPERLoutpara_unname_137(pltest.tableVARCHAR16array.key) AS ID from pltest.tableVARCHAR16array DISTRIBUTED randomly;

select * from pltest.tableVARCHAR16array1 ORDER BY ID;

drop table pltest.tableVARCHAR16array1;

select pltest.PLPERLoutpara_unname_137(array['ASDFGHJKL'::VARCHAR(16),NULL]::VARCHAR(16)[]);

select * from pltest.PLPERLoutpara_unname_137(array['ASDFGHJKL'::VARCHAR(16),NULL]::VARCHAR(16)[]);

DROP function pltest.PLPERLoutpara_unname_137 ( IN  VARCHAR(16)[] , OUT  VARCHAR(16)[] );
----Case 137 end----


----Case 138 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_138 ( IN  INT4[] , OUT  INT4[] ) RETURNS INT4[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_138(pltest.tableINT4array.key) AS ID from pltest.tableINT4array ORDER BY ID;

create table pltest.tableINT4array1 as select pltest.PLPERLoutpara_unname_138(pltest.tableINT4array.key) AS ID from pltest.tableINT4array DISTRIBUTED randomly;

select * from pltest.tableINT4array1 ORDER BY ID;

drop table pltest.tableINT4array1;

select pltest.PLPERLoutpara_unname_138(array[(-2147483648)::INT4,2147483647::INT4,1234,NULL]::INT4[]);

select * from pltest.PLPERLoutpara_unname_138(array[(-2147483648)::INT4,2147483647::INT4,1234,NULL]::INT4[]);

DROP function pltest.PLPERLoutpara_unname_138 ( IN  INT4[] , OUT  INT4[] );
----Case 138 end----


----Case 139 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_139 ( IN  CIDR[] , OUT  CIDR[] ) RETURNS CIDR[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_139(pltest.tableCIDRarray.key) AS ID from pltest.tableCIDRarray ORDER BY ID;

create table pltest.tableCIDRarray1 as select pltest.PLPERLoutpara_unname_139(pltest.tableCIDRarray.key) AS ID from pltest.tableCIDRarray DISTRIBUTED randomly;

select * from pltest.tableCIDRarray1 ORDER BY ID;

drop table pltest.tableCIDRarray1;

select pltest.PLPERLoutpara_unname_139(array['192.168.0.1/32'::CIDR,NULL]::CIDR[]);

select * from pltest.PLPERLoutpara_unname_139(array['192.168.0.1/32'::CIDR,NULL]::CIDR[]);

DROP function pltest.PLPERLoutpara_unname_139 ( IN  CIDR[] , OUT  CIDR[] );
----Case 139 end----


----Case 140 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_140 ( IN  "char"[] , OUT  "char"[] ) RETURNS "char"[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_140(pltest.tableQUTOCHARarray.key) AS ID from pltest.tableQUTOCHARarray ORDER BY ID;

create table pltest.tableQUTOCHARarray1 as select pltest.PLPERLoutpara_unname_140(pltest.tableQUTOCHARarray.key) AS ID from pltest.tableQUTOCHARarray DISTRIBUTED randomly;

select * from pltest.tableQUTOCHARarray1 ORDER BY ID;

drop table pltest.tableQUTOCHARarray1;

select pltest.PLPERLoutpara_unname_140(array['a'::"char",null::"char"]::"char"[]);

select * from pltest.PLPERLoutpara_unname_140(array['a'::"char",null::"char"]::"char"[]);

DROP function pltest.PLPERLoutpara_unname_140 ( IN  "char"[] , OUT  "char"[] );
----Case 140 end----


----Case 141 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_141 ( IN  TIMESTAMP[] , OUT  TIMESTAMP[] ) RETURNS TIMESTAMP[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_141(pltest.tableTIMESTAMParray.key) AS ID from pltest.tableTIMESTAMParray ORDER BY ID;

create table pltest.tableTIMESTAMParray1 as select pltest.PLPERLoutpara_unname_141(pltest.tableTIMESTAMParray.key) AS ID from pltest.tableTIMESTAMParray DISTRIBUTED randomly;

select * from pltest.tableTIMESTAMParray1 ORDER BY ID;

drop table pltest.tableTIMESTAMParray1;

select pltest.PLPERLoutpara_unname_141(array['2011-08-12 10:00:52.14'::TIMESTAMP,NULL]::TIMESTAMP[]);

select * from pltest.PLPERLoutpara_unname_141(array['2011-08-12 10:00:52.14'::TIMESTAMP,NULL]::TIMESTAMP[]);

DROP function pltest.PLPERLoutpara_unname_141 ( IN  TIMESTAMP[] , OUT  TIMESTAMP[] );
----Case 141 end----


----Case 142 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_142 ( IN  BOX[] , OUT  BOX[] ) RETURNS BOX[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLoutpara_unname_142(array['((1,2),(3,4))'::BOX,NULL]::BOX[]);

select * from pltest.PLPERLoutpara_unname_142(array['((1,2),(3,4))'::BOX,NULL]::BOX[]);

DROP function pltest.PLPERLoutpara_unname_142 ( IN  BOX[] , OUT  BOX[] );
----Case 142 end----


----Case 143 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_143 ( IN  TEXT[] , OUT  TEXT[] ) RETURNS TEXT[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_143(pltest.tableTEXTarray.key) AS ID from pltest.tableTEXTarray ORDER BY ID;

create table pltest.tableTEXTarray1 as select pltest.PLPERLoutpara_unname_143(pltest.tableTEXTarray.key) AS ID from pltest.tableTEXTarray DISTRIBUTED randomly;

select * from pltest.tableTEXTarray1 ORDER BY ID;

drop table pltest.tableTEXTarray1;

select pltest.PLPERLoutpara_unname_143(array['QAZWSXEDCRFVTGB'::TEXT,NULL]::TEXT[]);

select * from pltest.PLPERLoutpara_unname_143(array['QAZWSXEDCRFVTGB'::TEXT,NULL]::TEXT[]);

DROP function pltest.PLPERLoutpara_unname_143 ( IN  TEXT[] , OUT  TEXT[] );
----Case 143 end----


----Case 144 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_144 ( IN  BIT VARYING(10)[] , OUT  BIT VARYING(10)[] ) RETURNS BIT VARYING(10)[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_144(pltest.tableBITVARYING10array.key) AS ID from pltest.tableBITVARYING10array ORDER BY ID;

create table pltest.tableBITVARYING10array1 as select pltest.PLPERLoutpara_unname_144(pltest.tableBITVARYING10array.key) AS ID from pltest.tableBITVARYING10array DISTRIBUTED randomly;

select * from pltest.tableBITVARYING10array1 ORDER BY ID;

drop table pltest.tableBITVARYING10array1;

select pltest.PLPERLoutpara_unname_144(array[B'11001111'::BIT VARYING(10),NULL]::BIT VARYING(10)[]);

select * from pltest.PLPERLoutpara_unname_144(array[B'11001111'::BIT VARYING(10),NULL]::BIT VARYING(10)[]);

DROP function pltest.PLPERLoutpara_unname_144 ( IN  BIT VARYING(10)[] , OUT  BIT VARYING(10)[] );
----Case 144 end----


----Case 145 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_145 ( IN  NUMERIC[] , OUT  NUMERIC[] ) RETURNS NUMERIC[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_145(pltest.tableNUMERICarray.key) AS ID from pltest.tableNUMERICarray ORDER BY ID;

create table pltest.tableNUMERICarray1 as select pltest.PLPERLoutpara_unname_145(pltest.tableNUMERICarray.key) AS ID from pltest.tableNUMERICarray DISTRIBUTED randomly;

select * from pltest.tableNUMERICarray1 ORDER BY ID;

drop table pltest.tableNUMERICarray1;

select pltest.PLPERLoutpara_unname_145(array[1234567890.0987654321::NUMERIC,'NAN'::NUMERIC,2::NUMERIC^128 / 3::NUMERIC^129,2::NUMERIC^128,0.5::NUMERIC,NULL]::NUMERIC[]);

select * from pltest.PLPERLoutpara_unname_145(array[1234567890.0987654321::NUMERIC,'NAN'::NUMERIC,2::NUMERIC^128 / 3::NUMERIC^129,2::NUMERIC^128,0.5::NUMERIC,NULL]::NUMERIC[]);

DROP function pltest.PLPERLoutpara_unname_145 ( IN  NUMERIC[] , OUT  NUMERIC[] );
----Case 145 end----


----Case 146 b
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 	
\echo '--ERROR:  syntax error at or near "ANY"'	
CREATE or REPLACE function pltest.PLPERLoutpara_unname_146 ( IN  ANY[] , OUT  ANY[] ) RETURNS ANY[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;
----Case 146 end----


----Case 147 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_147 ( IN  SMALLINT[] , OUT  SMALLINT[] ) RETURNS SMALLINT[] as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLoutpara_unname_147(pltest.tableSMALLINTarray.key) AS ID from pltest.tableSMALLINTarray ORDER BY ID;

create table pltest.tableSMALLINTarray1 as select pltest.PLPERLoutpara_unname_147(pltest.tableSMALLINTarray.key) AS ID from pltest.tableSMALLINTarray DISTRIBUTED randomly;

select * from pltest.tableSMALLINTarray1 ORDER BY ID;

drop table pltest.tableSMALLINTarray1;

select pltest.PLPERLoutpara_unname_147(array[(-32768)::SMALLINT,32767::SMALLINT,1234::SMALLINT,NULL]::SMALLINT[]);

select * from pltest.PLPERLoutpara_unname_147(array[(-32768)::SMALLINT,32767::SMALLINT,1234::SMALLINT,NULL]::SMALLINT[]);

DROP function pltest.PLPERLoutpara_unname_147 ( IN  SMALLINT[] , OUT  SMALLINT[] );
----Case 147 end----


----Case 148 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_148 ( IN  pltest.COMPOSITE_INTERNET , OUT  pltest.COMPOSITE_INTERNET ) RETURNS pltest.COMPOSITE_INTERNET as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLoutpara_unname_148(((-32768)::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET);

select pltest.PLPERLoutpara_unname_148((32767::SMALLINT, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET);

select pltest.PLPERLoutpara_unname_148((1234::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET);

select pltest.PLPERLoutpara_unname_148((NULL, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET);

select (pltest.PLPERLoutpara_unname_148(((-32768)::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET)).*;

select (pltest.PLPERLoutpara_unname_148((32767::SMALLINT, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET)).*;

select (pltest.PLPERLoutpara_unname_148((1234::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET)).*;

select (pltest.PLPERLoutpara_unname_148((NULL, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET)).*;

select * from pltest.PLPERLoutpara_unname_148(((-32768)::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET);

select * from pltest.PLPERLoutpara_unname_148((32767::SMALLINT, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET);

select * from pltest.PLPERLoutpara_unname_148((1234::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET);

select * from pltest.PLPERLoutpara_unname_148((NULL, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET);

DROP function pltest.PLPERLoutpara_unname_148 ( IN  pltest.COMPOSITE_INTERNET , OUT  pltest.COMPOSITE_INTERNET );
----Case 148 end----


----Case 149 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_149 ( IN  pltest.COMPOSITE_CHAR , OUT  pltest.COMPOSITE_CHAR ) RETURNS pltest.COMPOSITE_CHAR as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;



select pltest.PLPERLoutpara_unname_149(('SSSS'::CHAR(4), 'ASDFGHJKL'::VARCHAR(16), 'QAZWSXEDCRFVTGB'::TEXT)::pltest.COMPOSITE_CHAR);

select pltest.PLPERLoutpara_unname_149((NULL, NULL, NULL)::pltest.COMPOSITE_CHAR);

select (pltest.PLPERLoutpara_unname_149(('SSSS'::CHAR(4), 'ASDFGHJKL'::VARCHAR(16), 'QAZWSXEDCRFVTGB'::TEXT)::pltest.COMPOSITE_CHAR)).*;

select (pltest.PLPERLoutpara_unname_149((NULL, NULL, NULL)::pltest.COMPOSITE_CHAR)).*;

select * from pltest.PLPERLoutpara_unname_149(('SSSS'::CHAR(4), 'ASDFGHJKL'::VARCHAR(16), 'QAZWSXEDCRFVTGB'::TEXT)::pltest.COMPOSITE_CHAR);

select * from pltest.PLPERLoutpara_unname_149((NULL, NULL, NULL)::pltest.COMPOSITE_CHAR);

DROP function pltest.PLPERLoutpara_unname_149 ( IN  pltest.COMPOSITE_CHAR , OUT  pltest.COMPOSITE_CHAR );
----Case 149 end----


----Case 150 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_150 ( IN  pltest.COMPOSITE_BIT , OUT  pltest.COMPOSITE_BIT ) RETURNS pltest.COMPOSITE_BIT as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLoutpara_unname_150(((-32768)::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), FALSE::BOOLEAN, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT);

select pltest.PLPERLoutpara_unname_150((32767::SMALLINT, NULL, NULL, TRUE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT);

select pltest.PLPERLoutpara_unname_150((1234::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), NULL, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT);

select pltest.PLPERLoutpara_unname_150((NULL, NULL, NULL, FALSE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT);

select (pltest.PLPERLoutpara_unname_150(((-32768)::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), FALSE::BOOLEAN, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT)).*;

select (pltest.PLPERLoutpara_unname_150((32767::SMALLINT, NULL, NULL, TRUE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT)).*;

select (pltest.PLPERLoutpara_unname_150((1234::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), NULL, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT)).*;

select (pltest.PLPERLoutpara_unname_150((NULL, NULL, NULL, FALSE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT)).*;

select * from pltest.PLPERLoutpara_unname_150(((-32768)::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), FALSE::BOOLEAN, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT);

select * from pltest.PLPERLoutpara_unname_150((32767::SMALLINT, NULL, NULL, TRUE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT);

select * from pltest.PLPERLoutpara_unname_150((1234::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), NULL, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT);

select * from pltest.PLPERLoutpara_unname_150((NULL, NULL, NULL, FALSE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT);

DROP function pltest.PLPERLoutpara_unname_150 ( IN  pltest.COMPOSITE_BIT , OUT  pltest.COMPOSITE_BIT );
----Case 150 end----


----Case 151 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_151 ( IN  pltest.COMPOSITE_GEOMETRIC , OUT  pltest.COMPOSITE_GEOMETRIC ) RETURNS pltest.COMPOSITE_GEOMETRIC as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLoutpara_unname_151(((-32768)::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC);

select pltest.PLPERLoutpara_unname_151((32767::SMALLINT, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC);

select pltest.PLPERLoutpara_unname_151((1234::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC);

select pltest.PLPERLoutpara_unname_151((NULL, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC);

select (pltest.PLPERLoutpara_unname_151(((-32768)::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC)).*;

select (pltest.PLPERLoutpara_unname_151((32767::SMALLINT, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC)).*;

select (pltest.PLPERLoutpara_unname_151((1234::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC)).*;

select (pltest.PLPERLoutpara_unname_151((NULL, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC)).*;

select * from pltest.PLPERLoutpara_unname_151(((-32768)::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC);

select * from pltest.PLPERLoutpara_unname_151((32767::SMALLINT, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC);

select * from pltest.PLPERLoutpara_unname_151((1234::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC);

select * from pltest.PLPERLoutpara_unname_151((NULL, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC);

DROP function pltest.PLPERLoutpara_unname_151 ( IN  pltest.COMPOSITE_GEOMETRIC , OUT  pltest.COMPOSITE_GEOMETRIC );
----Case 151 end----


----Case 152 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_152 ( IN  pltest.COMPOSITE_DATE , OUT  pltest.COMPOSITE_DATE ) RETURNS pltest.COMPOSITE_DATE as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLoutpara_unname_152(('2011-08-12'::DATE, '10:00:52.14'::TIME, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE);

select pltest.PLPERLoutpara_unname_152(('EPOCH'::DATE, 'ALLBALLS'::TIME, NULL, NULL, NULL)::pltest.COMPOSITE_DATE);

select pltest.PLPERLoutpara_unname_152((NULL, NULL, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE);

select (pltest.PLPERLoutpara_unname_152(('2011-08-12'::DATE, '10:00:52.14'::TIME, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE)).*;

select (pltest.PLPERLoutpara_unname_152(('EPOCH'::DATE, 'ALLBALLS'::TIME, NULL, NULL, NULL)::pltest.COMPOSITE_DATE)).*;

select (pltest.PLPERLoutpara_unname_152((NULL, NULL, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE)).*;

select * from pltest.PLPERLoutpara_unname_152(('2011-08-12'::DATE, '10:00:52.14'::TIME, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE);

select * from pltest.PLPERLoutpara_unname_152(('EPOCH'::DATE, 'ALLBALLS'::TIME, NULL, NULL, NULL)::pltest.COMPOSITE_DATE);

select * from pltest.PLPERLoutpara_unname_152((NULL, NULL, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE);

DROP function pltest.PLPERLoutpara_unname_152 ( IN  pltest.COMPOSITE_DATE , OUT  pltest.COMPOSITE_DATE );
----Case 152 end----


----Case 153 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(IN b/c/b[]/c[], OUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLoutpara_unname_153 ( IN  pltest.COMPOSITE_NUMERIC , OUT  pltest.COMPOSITE_NUMERIC ) RETURNS pltest.COMPOSITE_NUMERIC as $$    	 
	my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLoutpara_unname_153(((-9223372036854775808)::BIGINT, 1234567890.0987654321::NUMERIC, 123456789.98765432::FLOAT8, (-2147483648)::INT4, '$-21474836.48'::MONEY, 39.333::REAL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select pltest.PLPERLoutpara_unname_153((9223372036854775807::BIGINT, 'NAN'::NUMERIC, 'NAN'::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 'NAN'::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select pltest.PLPERLoutpara_unname_153((123456789::BIGINT, 2::NUMERIC^128 / 3::NUMERIC^129, '-INFINITY'::FLOAT8, 1234, '$1234'::MONEY, '-INFINITY'::REAL, 1234::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select pltest.PLPERLoutpara_unname_153((NULL, 2::NUMERIC^128, '+INFINITY'::FLOAT8, NULL, NULL, '+INFINITY'::REAL, NULL)::pltest.COMPOSITE_NUMERIC);

select pltest.PLPERLoutpara_unname_153(((-9223372036854775808)::BIGINT, 0.5::NUMERIC, NULL, (-2147483648)::INT4, '$-21474836.48'::MONEY, NULL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select pltest.PLPERLoutpara_unname_153((9223372036854775807::BIGINT, NULL, 123456789.98765432::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 39.333::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select (pltest.PLPERLoutpara_unname_153(((-9223372036854775808)::BIGINT, 1234567890.0987654321::NUMERIC, 123456789.98765432::FLOAT8, (-2147483648)::INT4, '$-21474836.48'::MONEY, 39.333::REAL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC)).*;

select (pltest.PLPERLoutpara_unname_153((9223372036854775807::BIGINT, 'NAN'::NUMERIC, 'NAN'::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 'NAN'::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC)).*;

select (pltest.PLPERLoutpara_unname_153((123456789::BIGINT, 2::NUMERIC^128 / 3::NUMERIC^129, '-INFINITY'::FLOAT8, 1234, '$1234'::MONEY, '-INFINITY'::REAL, 1234::SMALLINT)::pltest.COMPOSITE_NUMERIC)).*;

select (pltest.PLPERLoutpara_unname_153((NULL, 2::NUMERIC^128, '+INFINITY'::FLOAT8, NULL, NULL, '+INFINITY'::REAL, NULL)::pltest.COMPOSITE_NUMERIC)).*;

select (pltest.PLPERLoutpara_unname_153(((-9223372036854775808)::BIGINT, 0.5::NUMERIC, NULL, (-2147483648)::INT4, '$-21474836.48'::MONEY, NULL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC)).*;

select (pltest.PLPERLoutpara_unname_153((9223372036854775807::BIGINT, NULL, 123456789.98765432::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 39.333::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC)).*;

select * from pltest.PLPERLoutpara_unname_153(((-9223372036854775808)::BIGINT, 1234567890.0987654321::NUMERIC, 123456789.98765432::FLOAT8, (-2147483648)::INT4, '$-21474836.48'::MONEY, 39.333::REAL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select * from pltest.PLPERLoutpara_unname_153((9223372036854775807::BIGINT, 'NAN'::NUMERIC, 'NAN'::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 'NAN'::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select * from pltest.PLPERLoutpara_unname_153((123456789::BIGINT, 2::NUMERIC^128 / 3::NUMERIC^129, '-INFINITY'::FLOAT8, 1234, '$1234'::MONEY, '-INFINITY'::REAL, 1234::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select * from pltest.PLPERLoutpara_unname_153((NULL, 2::NUMERIC^128, '+INFINITY'::FLOAT8, NULL, NULL, '+INFINITY'::REAL, NULL)::pltest.COMPOSITE_NUMERIC);

select * from pltest.PLPERLoutpara_unname_153(((-9223372036854775808)::BIGINT, 0.5::NUMERIC, NULL, (-2147483648)::INT4, '$-21474836.48'::MONEY, NULL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select * from pltest.PLPERLoutpara_unname_153((9223372036854775807::BIGINT, NULL, 123456789.98765432::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 39.333::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC);

DROP function pltest.PLPERLoutpara_unname_153 ( IN  pltest.COMPOSITE_NUMERIC , OUT  pltest.COMPOSITE_NUMERIC );
----Case 153 end----


