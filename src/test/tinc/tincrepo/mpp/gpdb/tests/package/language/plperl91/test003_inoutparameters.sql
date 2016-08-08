----Case 154 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_154 ( INOUT  REAL ) RETURNS REAL as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_154(pltest.tableREAL.key) AS ID from pltest.tableREAL ORDER BY ID;

create table pltest.tableREAL1 as select pltest.PLPERLinoutpara_unname_154(pltest.tableREAL.key) AS ID from pltest.tableREAL DISTRIBUTED randomly;

select * from pltest.tableREAL1 ORDER BY ID;

drop table pltest.tableREAL1;

select pltest.PLPERLinoutpara_unname_154(39.333::REAL);

select pltest.PLPERLinoutpara_unname_154('NAN'::REAL);

select pltest.PLPERLinoutpara_unname_154('-INFINITY'::REAL);

select pltest.PLPERLinoutpara_unname_154('+INFINITY'::REAL);

select pltest.PLPERLinoutpara_unname_154(NULL);


select * from pltest.PLPERLinoutpara_unname_154(39.333::REAL);

select * from pltest.PLPERLinoutpara_unname_154('NAN'::REAL);

select * from pltest.PLPERLinoutpara_unname_154('-INFINITY'::REAL);

select * from pltest.PLPERLinoutpara_unname_154('+INFINITY'::REAL);

select * from pltest.PLPERLinoutpara_unname_154(NULL);


DROP function pltest.PLPERLinoutpara_unname_154 ( INOUT  REAL );
----Case 154 end----


----Case 155 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_155 ( INOUT  BOX ) RETURNS BOX as $$     
			my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinoutpara_unname_155('((1,2),(3,4))'::BOX);

select pltest.PLPERLinoutpara_unname_155(NULL);


select * from pltest.PLPERLinoutpara_unname_155('((1,2),(3,4))'::BOX);

select * from pltest.PLPERLinoutpara_unname_155(NULL);


DROP function pltest.PLPERLinoutpara_unname_155 ( INOUT  BOX );
----Case 155 end----


----Case 156 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_156 ( INOUT  VARCHAR(16) ) RETURNS VARCHAR(16) as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_156(pltest.tableVARCHAR16.key) AS ID from pltest.tableVARCHAR16 ORDER BY ID;

create table pltest.tableVARCHAR161 as select pltest.PLPERLinoutpara_unname_156(pltest.tableVARCHAR16.key) AS ID from pltest.tableVARCHAR16 DISTRIBUTED randomly;

select * from pltest.tableVARCHAR161 ORDER BY ID;

drop table pltest.tableVARCHAR161;

select pltest.PLPERLinoutpara_unname_156('ASDFGHJKL'::VARCHAR(16));

select pltest.PLPERLinoutpara_unname_156(NULL);


select * from pltest.PLPERLinoutpara_unname_156('ASDFGHJKL'::VARCHAR(16));

select * from pltest.PLPERLinoutpara_unname_156(NULL);


DROP function pltest.PLPERLinoutpara_unname_156 ( INOUT  VARCHAR(16) );
----Case 156 end----


----Case 157 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
--
\echo '-- ERROR:  PL/Perl functions cannot return type anyarray' 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_157 ( INOUT  ANYARRAY ) RETURNS ANYARRAY as $$     
			my $key = shift; return $key;
$$ language PLPERL;

----Case 157 end----


----Case 158 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_158 ( INOUT  UNKNOWN ) RETURNS UNKNOWN as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_158('QAZWSXEDCRFV'::UNKNOWN);

select pltest.PLPERLinoutpara_unname_158(NULL::UNKNOWN);


select * from pltest.PLPERLinoutpara_unname_158('QAZWSXEDCRFV'::UNKNOWN);

select * from pltest.PLPERLinoutpara_unname_158(NULL::UNKNOWN);


DROP function pltest.PLPERLinoutpara_unname_158 ( INOUT  UNKNOWN );
----Case 158 end----


----Case 159 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_159 ( INOUT  BIT VARYING(10) ) RETURNS BIT VARYING(10) as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_159(pltest.tableBITVARYING10.key) AS ID from pltest.tableBITVARYING10 ORDER BY ID;

create table pltest.tableBITVARYING101 as select pltest.PLPERLinoutpara_unname_159(pltest.tableBITVARYING10.key) AS ID from pltest.tableBITVARYING10 DISTRIBUTED randomly;

select * from pltest.tableBITVARYING101 ORDER BY ID;

drop table pltest.tableBITVARYING101;

select pltest.PLPERLinoutpara_unname_159(B'11001111'::BIT VARYING(10));

select pltest.PLPERLinoutpara_unname_159(NULL);


select * from pltest.PLPERLinoutpara_unname_159(B'11001111'::BIT VARYING(10));

select * from pltest.PLPERLinoutpara_unname_159(NULL);


DROP function pltest.PLPERLinoutpara_unname_159 ( INOUT  BIT VARYING(10) );
----Case 159 end----


----Case 160 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_160 ( INOUT  TIMESTAMPTZ ) RETURNS TIMESTAMPTZ as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_160(pltest.tableTIMESTAMPTZ.key) AS ID from pltest.tableTIMESTAMPTZ ORDER BY ID;

create table pltest.tableTIMESTAMPTZ1 as select pltest.PLPERLinoutpara_unname_160(pltest.tableTIMESTAMPTZ.key) AS ID from pltest.tableTIMESTAMPTZ DISTRIBUTED randomly;

select * from pltest.tableTIMESTAMPTZ1 ORDER BY ID;

drop table pltest.tableTIMESTAMPTZ1;

select pltest.PLPERLinoutpara_unname_160('2011-08-12 10:00:52.14'::TIMESTAMPTZ);

select pltest.PLPERLinoutpara_unname_160(NULL);


select * from pltest.PLPERLinoutpara_unname_160('2011-08-12 10:00:52.14'::TIMESTAMPTZ);

select * from pltest.PLPERLinoutpara_unname_160(NULL);


DROP function pltest.PLPERLinoutpara_unname_160 ( INOUT  TIMESTAMPTZ );
----Case 160 end----


----Case 161 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_161 ( INOUT  "char" ) RETURNS "char" as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_161(pltest.tableQUTOCHAR.key) AS ID from pltest.tableQUTOCHAR ORDER BY ID;

create table pltest.tableQUTOCHAR1 as select pltest.PLPERLinoutpara_unname_161(pltest.tableQUTOCHAR.key) AS ID from pltest.tableQUTOCHAR DISTRIBUTED randomly;

select * from pltest.tableQUTOCHAR1 ORDER BY ID;

drop table pltest.tableQUTOCHAR1;

select pltest.PLPERLinoutpara_unname_161('a'::"char");

select pltest.PLPERLinoutpara_unname_161(null::"char");


select * from pltest.PLPERLinoutpara_unname_161('a'::"char");

select * from pltest.PLPERLinoutpara_unname_161(null::"char");


DROP function pltest.PLPERLinoutpara_unname_161 ( INOUT  "char" );
----Case 161 end----


----Case 162 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_162 ( INOUT  CHAR ) RETURNS CHAR as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_162(pltest.tableCHAR.key) AS ID from pltest.tableCHAR ORDER BY ID;

create table pltest.tableCHAR1 as select pltest.PLPERLinoutpara_unname_162(pltest.tableCHAR.key) AS ID from pltest.tableCHAR DISTRIBUTED randomly;

select * from pltest.tableCHAR1 ORDER BY ID;

drop table pltest.tableCHAR1;

select pltest.PLPERLinoutpara_unname_162('B'::CHAR);

select pltest.PLPERLinoutpara_unname_162(NULL::CHAR);


select * from pltest.PLPERLinoutpara_unname_162('B'::CHAR);

select * from pltest.PLPERLinoutpara_unname_162(NULL::CHAR);


DROP function pltest.PLPERLinoutpara_unname_162 ( INOUT  CHAR );
----Case 162 end----


----Case 163 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_163 ( INOUT  BIGINT ) RETURNS BIGINT as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_163(pltest.tableBIGINT.key) AS ID from pltest.tableBIGINT ORDER BY ID;

create table pltest.tableBIGINT1 as select pltest.PLPERLinoutpara_unname_163(pltest.tableBIGINT.key) AS ID from pltest.tableBIGINT DISTRIBUTED randomly;

select * from pltest.tableBIGINT1 ORDER BY ID;

drop table pltest.tableBIGINT1;

select pltest.PLPERLinoutpara_unname_163((-9223372036854775808)::BIGINT);

select pltest.PLPERLinoutpara_unname_163(9223372036854775807::BIGINT);

select pltest.PLPERLinoutpara_unname_163(123456789::BIGINT);

select pltest.PLPERLinoutpara_unname_163(NULL);


select * from pltest.PLPERLinoutpara_unname_163((-9223372036854775808)::BIGINT);

select * from pltest.PLPERLinoutpara_unname_163(9223372036854775807::BIGINT);

select * from pltest.PLPERLinoutpara_unname_163(123456789::BIGINT);

select * from pltest.PLPERLinoutpara_unname_163(NULL);


DROP function pltest.PLPERLinoutpara_unname_163 ( INOUT  BIGINT );
----Case 163 end----


----Case 164 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_164 ( INOUT  DATE ) RETURNS DATE as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_164(pltest.tableDATE.key) AS ID from pltest.tableDATE ORDER BY ID;

create table pltest.tableDATE1 as select pltest.PLPERLinoutpara_unname_164(pltest.tableDATE.key) AS ID from pltest.tableDATE DISTRIBUTED randomly;

select * from pltest.tableDATE1 ORDER BY ID;

drop table pltest.tableDATE1;

select pltest.PLPERLinoutpara_unname_164('2011-08-12'::DATE);

select pltest.PLPERLinoutpara_unname_164('EPOCH'::DATE);

select pltest.PLPERLinoutpara_unname_164(NULL);


select * from pltest.PLPERLinoutpara_unname_164('2011-08-12'::DATE);

select * from pltest.PLPERLinoutpara_unname_164('EPOCH'::DATE);

select * from pltest.PLPERLinoutpara_unname_164(NULL);


DROP function pltest.PLPERLinoutpara_unname_164 ( INOUT  DATE );
----Case 164 end----


----Case 165 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_165 ( INOUT  PATH ) RETURNS PATH as $$     
			my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinoutpara_unname_165('(1,1),(2,2)'::PATH);

select pltest.PLPERLinoutpara_unname_165(NULL);


select * from pltest.PLPERLinoutpara_unname_165('(1,1),(2,2)'::PATH);

select * from pltest.PLPERLinoutpara_unname_165(NULL);


DROP function pltest.PLPERLinoutpara_unname_165 ( INOUT  PATH );
----Case 165 end----


----Case 166 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_166 ( INOUT  SMALLINT ) RETURNS SMALLINT as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_166(pltest.tableSMALLINT.key) AS ID from pltest.tableSMALLINT ORDER BY ID;

create table pltest.tableSMALLINT1 as select pltest.PLPERLinoutpara_unname_166(pltest.tableSMALLINT.key) AS ID from pltest.tableSMALLINT DISTRIBUTED randomly;

select * from pltest.tableSMALLINT1 ORDER BY ID;

drop table pltest.tableSMALLINT1;

select pltest.PLPERLinoutpara_unname_166((-32768)::SMALLINT);

select pltest.PLPERLinoutpara_unname_166(32767::SMALLINT);

select pltest.PLPERLinoutpara_unname_166(1234::SMALLINT);

select pltest.PLPERLinoutpara_unname_166(NULL);


select * from pltest.PLPERLinoutpara_unname_166((-32768)::SMALLINT);

select * from pltest.PLPERLinoutpara_unname_166(32767::SMALLINT);

select * from pltest.PLPERLinoutpara_unname_166(1234::SMALLINT);

select * from pltest.PLPERLinoutpara_unname_166(NULL);


DROP function pltest.PLPERLinoutpara_unname_166 ( INOUT  SMALLINT );
----Case 166 end----


----Case 167 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_167 ( INOUT  MACADDR ) RETURNS MACADDR as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_167(pltest.tableMACADDR.key) AS ID from pltest.tableMACADDR ORDER BY ID;

create table pltest.tableMACADDR1 as select pltest.PLPERLinoutpara_unname_167(pltest.tableMACADDR.key) AS ID from pltest.tableMACADDR DISTRIBUTED randomly;

select * from pltest.tableMACADDR1 ORDER BY ID;

drop table pltest.tableMACADDR1;

select pltest.PLPERLinoutpara_unname_167('12:34:56:78:90:AB'::MACADDR);

select pltest.PLPERLinoutpara_unname_167(NULL);


select * from pltest.PLPERLinoutpara_unname_167('12:34:56:78:90:AB'::MACADDR);

select * from pltest.PLPERLinoutpara_unname_167(NULL);


DROP function pltest.PLPERLinoutpara_unname_167 ( INOUT  MACADDR );
----Case 167 end----


----Case 168 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_168 ( INOUT  POINT ) RETURNS POINT as $$     
			my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinoutpara_unname_168('(1,2)'::POINT);

select pltest.PLPERLinoutpara_unname_168(NULL);


select * from pltest.PLPERLinoutpara_unname_168('(1,2)'::POINT);

select * from pltest.PLPERLinoutpara_unname_168(NULL);


DROP function pltest.PLPERLinoutpara_unname_168 ( INOUT  POINT );
----Case 168 end----


----Case 169 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_169 ( INOUT  INTERVAL ) RETURNS INTERVAL as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_169(pltest.tableINTERVAL.key) AS ID from pltest.tableINTERVAL ORDER BY ID;

create table pltest.tableINTERVAL1 as select pltest.PLPERLinoutpara_unname_169(pltest.tableINTERVAL.key) AS ID from pltest.tableINTERVAL DISTRIBUTED randomly;

select * from pltest.tableINTERVAL1 ORDER BY ID;

drop table pltest.tableINTERVAL1;

select pltest.PLPERLinoutpara_unname_169('1 DAY 12 HOURS 59 MIN 10 SEC'::INTERVAL);

select pltest.PLPERLinoutpara_unname_169('1 12:59:10'::INTERVAL);

select pltest.PLPERLinoutpara_unname_169(NULL);


select * from pltest.PLPERLinoutpara_unname_169('1 DAY 12 HOURS 59 MIN 10 SEC'::INTERVAL);

select * from pltest.PLPERLinoutpara_unname_169('1 12:59:10'::INTERVAL);

select * from pltest.PLPERLinoutpara_unname_169(NULL);


DROP function pltest.PLPERLinoutpara_unname_169 ( INOUT  INTERVAL );
----Case 169 end----


----Case 170 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_170 ( INOUT  NUMERIC ) RETURNS NUMERIC as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_170(pltest.tableNUMERIC.key) AS ID from pltest.tableNUMERIC ORDER BY ID;

create table pltest.tableNUMERIC1 as select pltest.PLPERLinoutpara_unname_170(pltest.tableNUMERIC.key) AS ID from pltest.tableNUMERIC DISTRIBUTED randomly;

select * from pltest.tableNUMERIC1 ORDER BY ID;

drop table pltest.tableNUMERIC1;

select pltest.PLPERLinoutpara_unname_170(1234567890.0987654321::NUMERIC);

select pltest.PLPERLinoutpara_unname_170('NAN'::NUMERIC);

select pltest.PLPERLinoutpara_unname_170(2::NUMERIC^128 / 3::NUMERIC^129);

select pltest.PLPERLinoutpara_unname_170(2::NUMERIC^128);

select pltest.PLPERLinoutpara_unname_170(0.5::NUMERIC);

select pltest.PLPERLinoutpara_unname_170(NULL);


select * from pltest.PLPERLinoutpara_unname_170(1234567890.0987654321::NUMERIC);

select * from pltest.PLPERLinoutpara_unname_170('NAN'::NUMERIC);

select * from pltest.PLPERLinoutpara_unname_170(2::NUMERIC^128 / 3::NUMERIC^129);

select * from pltest.PLPERLinoutpara_unname_170(2::NUMERIC^128);

select * from pltest.PLPERLinoutpara_unname_170(0.5::NUMERIC);

select * from pltest.PLPERLinoutpara_unname_170(NULL);


DROP function pltest.PLPERLinoutpara_unname_170 ( INOUT  NUMERIC );
----Case 170 end----


----Case 171 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_171 ( INOUT  INT4 ) RETURNS INT4 as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_171(pltest.tableINT4.key) AS ID from pltest.tableINT4 ORDER BY ID;

create table pltest.tableINT41 as select pltest.PLPERLinoutpara_unname_171(pltest.tableINT4.key) AS ID from pltest.tableINT4 DISTRIBUTED randomly;

select * from pltest.tableINT41 ORDER BY ID;

drop table pltest.tableINT41;

select pltest.PLPERLinoutpara_unname_171((-2147483648)::INT4);

select pltest.PLPERLinoutpara_unname_171(2147483647::INT4);

select pltest.PLPERLinoutpara_unname_171(1234);

select pltest.PLPERLinoutpara_unname_171(NULL);


select * from pltest.PLPERLinoutpara_unname_171((-2147483648)::INT4);

select * from pltest.PLPERLinoutpara_unname_171(2147483647::INT4);

select * from pltest.PLPERLinoutpara_unname_171(1234);

select * from pltest.PLPERLinoutpara_unname_171(NULL);


DROP function pltest.PLPERLinoutpara_unname_171 ( INOUT  INT4 );
----Case 171 end----


----Case 172 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_172 ( INOUT  CHAR(4) ) RETURNS CHAR(4) as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_172(pltest.tableCHAR4.key) AS ID from pltest.tableCHAR4 ORDER BY ID;

create table pltest.tableCHAR41 as select pltest.PLPERLinoutpara_unname_172(pltest.tableCHAR4.key) AS ID from pltest.tableCHAR4 DISTRIBUTED randomly;

select * from pltest.tableCHAR41 ORDER BY ID;

drop table pltest.tableCHAR41;

select pltest.PLPERLinoutpara_unname_172('SSSS'::CHAR(4));

select pltest.PLPERLinoutpara_unname_172(NULL);


select * from pltest.PLPERLinoutpara_unname_172('SSSS'::CHAR(4));

select * from pltest.PLPERLinoutpara_unname_172(NULL);


DROP function pltest.PLPERLinoutpara_unname_172 ( INOUT  CHAR(4) );
----Case 172 end----


----Case 173 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_173 ( INOUT  TEXT ) RETURNS TEXT as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_173(pltest.tableTEXT.key) AS ID from pltest.tableTEXT ORDER BY ID;

create table pltest.tableTEXT1 as select pltest.PLPERLinoutpara_unname_173(pltest.tableTEXT.key) AS ID from pltest.tableTEXT DISTRIBUTED randomly;

select * from pltest.tableTEXT1 ORDER BY ID;

drop table pltest.tableTEXT1;

select pltest.PLPERLinoutpara_unname_173('QAZWSXEDCRFVTGB'::TEXT);

select pltest.PLPERLinoutpara_unname_173(NULL);


select * from pltest.PLPERLinoutpara_unname_173('QAZWSXEDCRFVTGB'::TEXT);

select * from pltest.PLPERLinoutpara_unname_173(NULL);


DROP function pltest.PLPERLinoutpara_unname_173 ( INOUT  TEXT );
----Case 173 end----


----Case 174 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_174 ( INOUT  INET ) RETURNS INET as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_174(pltest.tableINET.key) AS ID from pltest.tableINET ORDER BY ID;

create table pltest.tableINET1 as select pltest.PLPERLinoutpara_unname_174(pltest.tableINET.key) AS ID from pltest.tableINET DISTRIBUTED randomly;

select * from pltest.tableINET1 ORDER BY ID;

drop table pltest.tableINET1;

select pltest.PLPERLinoutpara_unname_174('192.168.0.1'::INET);

select pltest.PLPERLinoutpara_unname_174(NULL);


select * from pltest.PLPERLinoutpara_unname_174('192.168.0.1'::INET);

select * from pltest.PLPERLinoutpara_unname_174(NULL);


DROP function pltest.PLPERLinoutpara_unname_174 ( INOUT  INET );
----Case 174 end----


----Case 175 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_175 ( INOUT  POLYGON ) RETURNS POLYGON as $$     
			my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinoutpara_unname_175('(0,0),(1,1),(2,0)'::POLYGON);

select pltest.PLPERLinoutpara_unname_175(NULL);


select * from pltest.PLPERLinoutpara_unname_175('(0,0),(1,1),(2,0)'::POLYGON);

select * from pltest.PLPERLinoutpara_unname_175(NULL);


DROP function pltest.PLPERLinoutpara_unname_175 ( INOUT  POLYGON );
----Case 175 end----


----Case 176 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
--
\echo '--ERROR:  PL/Perl functions cannot return type cstring' 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_176 ( INOUT  CSTRING ) RETURNS CSTRING as $$     
			my $key = shift; return $key;
$$ language PLPERL;

----Case 176 end----


----Case 177 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_177 ( INOUT  LSEG ) RETURNS LSEG as $$     
			my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinoutpara_unname_177('((1,1),(2,2))'::LSEG);

select pltest.PLPERLinoutpara_unname_177(NULL);


select * from pltest.PLPERLinoutpara_unname_177('((1,1),(2,2))'::LSEG);

select * from pltest.PLPERLinoutpara_unname_177(NULL);


DROP function pltest.PLPERLinoutpara_unname_177 ( INOUT  LSEG );
----Case 177 end----


----Case 178 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
\echo '--ERROR:  PL/Perl functions cannot return type anyelement'
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_178 ( INOUT  ANYELEMENT ) RETURNS ANYELEMENT as $$     
			my $key = shift; return $key;
$$ language PLPERL;

----Case 178 end----


----Case 179 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_179 ( INOUT  CIDR ) RETURNS CIDR as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_179(pltest.tableCIDR.key) AS ID from pltest.tableCIDR ORDER BY ID;

create table pltest.tableCIDR1 as select pltest.PLPERLinoutpara_unname_179(pltest.tableCIDR.key) AS ID from pltest.tableCIDR DISTRIBUTED randomly;

select * from pltest.tableCIDR1 ORDER BY ID;

drop table pltest.tableCIDR1;

select pltest.PLPERLinoutpara_unname_179('192.168.0.1/32'::CIDR);

select pltest.PLPERLinoutpara_unname_179(NULL);


select * from pltest.PLPERLinoutpara_unname_179('192.168.0.1/32'::CIDR);

select * from pltest.PLPERLinoutpara_unname_179(NULL);


DROP function pltest.PLPERLinoutpara_unname_179 ( INOUT  CIDR );
----Case 179 end----


----Case 180 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_180 ( INOUT  BYTEA ) RETURNS BYTEA as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_180(pltest.tableBYTEA.key) AS ID from pltest.tableBYTEA ORDER BY ID;

create table pltest.tableBYTEA1 as select pltest.PLPERLinoutpara_unname_180(pltest.tableBYTEA.key) AS ID from pltest.tableBYTEA DISTRIBUTED randomly;

select * from pltest.tableBYTEA1 ORDER BY ID;

drop table pltest.tableBYTEA1;

select pltest.PLPERLinoutpara_unname_180(E'\\000'::BYTEA);

select pltest.PLPERLinoutpara_unname_180(NULL);


select * from pltest.PLPERLinoutpara_unname_180(E'\\000'::BYTEA);

select * from pltest.PLPERLinoutpara_unname_180(NULL);


DROP function pltest.PLPERLinoutpara_unname_180 ( INOUT  BYTEA );
----Case 180 end----


----Case 181 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_181 ( INOUT  MONEY ) RETURNS MONEY as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_181(pltest.tableMONEY.key) AS ID from pltest.tableMONEY ORDER BY ID;

create table pltest.tableMONEY1 as select pltest.PLPERLinoutpara_unname_181(pltest.tableMONEY.key) AS ID from pltest.tableMONEY DISTRIBUTED randomly;

select * from pltest.tableMONEY1 ORDER BY ID;

drop table pltest.tableMONEY1;

select pltest.PLPERLinoutpara_unname_181('$-21474836.48'::MONEY);

select pltest.PLPERLinoutpara_unname_181('$21474836.47'::MONEY);

select pltest.PLPERLinoutpara_unname_181('$1234'::MONEY);

select pltest.PLPERLinoutpara_unname_181(NULL);


select * from pltest.PLPERLinoutpara_unname_181('$-21474836.48'::MONEY);

select * from pltest.PLPERLinoutpara_unname_181('$21474836.47'::MONEY);

select * from pltest.PLPERLinoutpara_unname_181('$1234'::MONEY);

select * from pltest.PLPERLinoutpara_unname_181(NULL);


DROP function pltest.PLPERLinoutpara_unname_181 ( INOUT  MONEY );
----Case 181 end----


----Case 182 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_182 ( INOUT  TIMESTAMP ) RETURNS TIMESTAMP as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_182(pltest.tableTIMESTAMP.key) AS ID from pltest.tableTIMESTAMP ORDER BY ID;

create table pltest.tableTIMESTAMP1 as select pltest.PLPERLinoutpara_unname_182(pltest.tableTIMESTAMP.key) AS ID from pltest.tableTIMESTAMP DISTRIBUTED randomly;

select * from pltest.tableTIMESTAMP1 ORDER BY ID;

drop table pltest.tableTIMESTAMP1;

select pltest.PLPERLinoutpara_unname_182('2011-08-12 10:00:52.14'::TIMESTAMP);

select pltest.PLPERLinoutpara_unname_182(NULL);


select * from pltest.PLPERLinoutpara_unname_182('2011-08-12 10:00:52.14'::TIMESTAMP);

select * from pltest.PLPERLinoutpara_unname_182(NULL);


DROP function pltest.PLPERLinoutpara_unname_182 ( INOUT  TIMESTAMP );
----Case 182 end----


----Case 183 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_183 ( INOUT  BOOLEAN ) RETURNS BOOLEAN as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_183(pltest.tableBOOLEAN.key) AS ID from pltest.tableBOOLEAN ORDER BY ID;

create table pltest.tableBOOLEAN1 as select pltest.PLPERLinoutpara_unname_183(pltest.tableBOOLEAN.key) AS ID from pltest.tableBOOLEAN DISTRIBUTED randomly;

select * from pltest.tableBOOLEAN1 ORDER BY ID;

drop table pltest.tableBOOLEAN1;

select pltest.PLPERLinoutpara_unname_183(FALSE::BOOLEAN);

select pltest.PLPERLinoutpara_unname_183(TRUE::BOOLEAN);

select pltest.PLPERLinoutpara_unname_183(NULL);


select * from pltest.PLPERLinoutpara_unname_183(FALSE::BOOLEAN);

select * from pltest.PLPERLinoutpara_unname_183(TRUE::BOOLEAN);

select * from pltest.PLPERLinoutpara_unname_183(NULL);


DROP function pltest.PLPERLinoutpara_unname_183 ( INOUT  BOOLEAN );
----Case 183 end----


----Case 184 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_184 ( INOUT  FLOAT8 ) RETURNS FLOAT8 as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_184(pltest.tableFLOAT8.key) AS ID from pltest.tableFLOAT8 ORDER BY ID;

create table pltest.tableFLOAT81 as select pltest.PLPERLinoutpara_unname_184(pltest.tableFLOAT8.key) AS ID from pltest.tableFLOAT8 DISTRIBUTED randomly;

select * from pltest.tableFLOAT81 ORDER BY ID;

drop table pltest.tableFLOAT81;

select pltest.PLPERLinoutpara_unname_184(123456789.98765432::FLOAT8);

select pltest.PLPERLinoutpara_unname_184('NAN'::FLOAT8);

select pltest.PLPERLinoutpara_unname_184('-INFINITY'::FLOAT8);

select pltest.PLPERLinoutpara_unname_184('+INFINITY'::FLOAT8);

select pltest.PLPERLinoutpara_unname_184(NULL);


select * from pltest.PLPERLinoutpara_unname_184(123456789.98765432::FLOAT8);

select * from pltest.PLPERLinoutpara_unname_184('NAN'::FLOAT8);

select * from pltest.PLPERLinoutpara_unname_184('-INFINITY'::FLOAT8);

select * from pltest.PLPERLinoutpara_unname_184('+INFINITY'::FLOAT8);

select * from pltest.PLPERLinoutpara_unname_184(NULL);


DROP function pltest.PLPERLinoutpara_unname_184 ( INOUT  FLOAT8 );
----Case 184 end----


----Case 185 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_185 ( INOUT  TIME ) RETURNS TIME as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_185(pltest.tableTIME.key) AS ID from pltest.tableTIME ORDER BY ID;

create table pltest.tableTIME1 as select pltest.PLPERLinoutpara_unname_185(pltest.tableTIME.key) AS ID from pltest.tableTIME DISTRIBUTED randomly;

select * from pltest.tableTIME1 ORDER BY ID;

drop table pltest.tableTIME1;

select pltest.PLPERLinoutpara_unname_185('10:00:52.14'::TIME);

select pltest.PLPERLinoutpara_unname_185('ALLBALLS'::TIME);

select pltest.PLPERLinoutpara_unname_185(NULL);


select * from pltest.PLPERLinoutpara_unname_185('10:00:52.14'::TIME);

select * from pltest.PLPERLinoutpara_unname_185('ALLBALLS'::TIME);

select * from pltest.PLPERLinoutpara_unname_185(NULL);


DROP function pltest.PLPERLinoutpara_unname_185 ( INOUT  TIME );
----Case 185 end----

----Case 186 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_186 ( INOUT  CIRCLE ) RETURNS CIRCLE as $$     
			my $key = shift; return $key;
$$ language PLPERL;



select pltest.PLPERLinoutpara_unname_186('<(1,1),2>'::CIRCLE);

select pltest.PLPERLinoutpara_unname_186(NULL);


select * from pltest.PLPERLinoutpara_unname_186('<(1,1),2>'::CIRCLE);

select * from pltest.PLPERLinoutpara_unname_186(NULL);


DROP function pltest.PLPERLinoutpara_unname_186 ( INOUT  CIRCLE );
----Case 186 end----


----Case 187 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_187 ( INOUT  TIMETZ ) RETURNS TIMETZ as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_187(pltest.tableTIMETZ.key) AS ID from pltest.tableTIMETZ ORDER BY ID;

create table pltest.tableTIMETZ1 as select pltest.PLPERLinoutpara_unname_187(pltest.tableTIMETZ.key) AS ID from pltest.tableTIMETZ DISTRIBUTED randomly;

select * from pltest.tableTIMETZ1 ORDER BY ID;

drop table pltest.tableTIMETZ1;

select pltest.PLPERLinoutpara_unname_187('2011-08-12 10:00:52.14'::TIMETZ);

select pltest.PLPERLinoutpara_unname_187(NULL);


select * from pltest.PLPERLinoutpara_unname_187('2011-08-12 10:00:52.14'::TIMETZ);

select * from pltest.PLPERLinoutpara_unname_187(NULL);


DROP function pltest.PLPERLinoutpara_unname_187 ( INOUT  TIMETZ );
----Case 187 end----


----Case 188 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_188 ( INOUT  BIT(8) ) RETURNS BIT(8) as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_188(pltest.tableBIT8.key) AS ID from pltest.tableBIT8 ORDER BY ID;

create table pltest.tableBIT81 as select pltest.PLPERLinoutpara_unname_188(pltest.tableBIT8.key) AS ID from pltest.tableBIT8 DISTRIBUTED randomly;

select * from pltest.tableBIT81 ORDER BY ID;

drop table pltest.tableBIT81;

select pltest.PLPERLinoutpara_unname_188(B'11001111'::BIT(8));

select pltest.PLPERLinoutpara_unname_188(NULL);


select * from pltest.PLPERLinoutpara_unname_188(B'11001111'::BIT(8));

select * from pltest.PLPERLinoutpara_unname_188(NULL);


DROP function pltest.PLPERLinoutpara_unname_188 ( INOUT  BIT(8) );
----Case 188 end----


----Case 189 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
--
\echo '--ERROR:  syntax error at or near "ANY"' 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_189 ( INOUT  ANY ) RETURNS ANY as $$     
			my $key = shift; return $key;
$$ language PLPERL;

----Case 189 end----


----Case 190 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_190 ( INOUT  BOOLEAN[] ) RETURNS BOOLEAN[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_190(pltest.tableBOOLEANarray.key) AS ID from pltest.tableBOOLEANarray ORDER BY ID;

create table pltest.tableBOOLEANarray1 as select pltest.PLPERLinoutpara_unname_190(pltest.tableBOOLEANarray.key) AS ID from pltest.tableBOOLEANarray DISTRIBUTED randomly;

select * from pltest.tableBOOLEANarray1 ORDER BY ID;

drop table pltest.tableBOOLEANarray1;

select pltest.PLPERLinoutpara_unname_190(array[FALSE::BOOLEAN,TRUE::BOOLEAN,NULL]::BOOLEAN[]);


select * from pltest.PLPERLinoutpara_unname_190(array[FALSE::BOOLEAN,TRUE::BOOLEAN,NULL]::BOOLEAN[]);


DROP function pltest.PLPERLinoutpara_unname_190 ( INOUT  BOOLEAN[] );
----Case 190 end----


----Case 191 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_191 ( INOUT  INTERVAL[] ) RETURNS INTERVAL[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_191(pltest.tableINTERVALarray.key) AS ID from pltest.tableINTERVALarray ORDER BY ID;

create table pltest.tableINTERVALarray1 as select pltest.PLPERLinoutpara_unname_191(pltest.tableINTERVALarray.key) AS ID from pltest.tableINTERVALarray DISTRIBUTED randomly;

select * from pltest.tableINTERVALarray1 ORDER BY ID;

drop table pltest.tableINTERVALarray1;

select pltest.PLPERLinoutpara_unname_191(array['1 DAY 12 HOURS 59 MIN 10 SEC'::INTERVAL,'1 12:59:10'::INTERVAL,NULL]::INTERVAL[]);


select * from pltest.PLPERLinoutpara_unname_191(array['1 DAY 12 HOURS 59 MIN 10 SEC'::INTERVAL,'1 12:59:10'::INTERVAL,NULL]::INTERVAL[]);


DROP function pltest.PLPERLinoutpara_unname_191 ( INOUT  INTERVAL[] );
----Case 191 end----


----Case 192 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_192 ( INOUT  BIT(8)[] ) RETURNS BIT(8)[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_192(pltest.tableBIT8array.key) AS ID from pltest.tableBIT8array ORDER BY ID;

create table pltest.tableBIT8array1 as select pltest.PLPERLinoutpara_unname_192(pltest.tableBIT8array.key) AS ID from pltest.tableBIT8array DISTRIBUTED randomly;

select * from pltest.tableBIT8array1 ORDER BY ID;

drop table pltest.tableBIT8array1;

select pltest.PLPERLinoutpara_unname_192(array[B'11001111'::BIT(8),NULL]::BIT(8)[]);




DROP function pltest.PLPERLinoutpara_unname_192 ( INOUT  BIT(8)[] );
----Case 192 end----


----Case 193 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_193 ( INOUT  TIMETZ[] ) RETURNS TIMETZ[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_193(pltest.tableTIMETZarray.key) AS ID from pltest.tableTIMETZarray ORDER BY ID;

create table pltest.tableTIMETZarray1 as select pltest.PLPERLinoutpara_unname_193(pltest.tableTIMETZarray.key) AS ID from pltest.tableTIMETZarray DISTRIBUTED randomly;

select * from pltest.tableTIMETZarray1 ORDER BY ID;

drop table pltest.tableTIMETZarray1;

select pltest.PLPERLinoutpara_unname_193(array['2011-08-12 10:00:52.14'::TIMETZ,NULL]::TIMETZ[]);


select * from pltest.PLPERLinoutpara_unname_193(array['2011-08-12 10:00:52.14'::TIMETZ,NULL]::TIMETZ[]);


DROP function pltest.PLPERLinoutpara_unname_193 ( INOUT  TIMETZ[] );
----Case 193 end----


----Case 194 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_194 ( INOUT  CIRCLE[] ) RETURNS CIRCLE[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;



select pltest.PLPERLinoutpara_unname_194(array['<(1,1),2>'::CIRCLE,NULL]::CIRCLE[]);


select * from pltest.PLPERLinoutpara_unname_194(array['<(1,1),2>'::CIRCLE,NULL]::CIRCLE[]);


DROP function pltest.PLPERLinoutpara_unname_194 ( INOUT  CIRCLE[] );
----Case 194 end----


----Case 195 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_195 ( INOUT  BYTEA[] ) RETURNS BYTEA[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_195(pltest.tableBYTEAarray.key) AS ID from pltest.tableBYTEAarray ORDER BY ID;

create table pltest.tableBYTEAarray1 as select pltest.PLPERLinoutpara_unname_195(pltest.tableBYTEAarray.key) AS ID from pltest.tableBYTEAarray DISTRIBUTED randomly;

select * from pltest.tableBYTEAarray1 ORDER BY ID;

drop table pltest.tableBYTEAarray1;

select pltest.PLPERLinoutpara_unname_195(array[E'\\000'::BYTEA,NULL]::BYTEA[]);


select * from pltest.PLPERLinoutpara_unname_195(array[E'\\000'::BYTEA,NULL]::BYTEA[]);


DROP function pltest.PLPERLinoutpara_unname_195 ( INOUT  BYTEA[] );
----Case 195 end----


----Case 196 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_196 ( INOUT  TIME[] ) RETURNS TIME[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_196(pltest.tableTIMEarray.key) AS ID from pltest.tableTIMEarray ORDER BY ID;

create table pltest.tableTIMEarray1 as select pltest.PLPERLinoutpara_unname_196(pltest.tableTIMEarray.key) AS ID from pltest.tableTIMEarray DISTRIBUTED randomly;

select * from pltest.tableTIMEarray1 ORDER BY ID;

drop table pltest.tableTIMEarray1;

select pltest.PLPERLinoutpara_unname_196(array['10:00:52.14'::TIME,'ALLBALLS'::TIME,NULL]::TIME[]);


select * from pltest.PLPERLinoutpara_unname_196(array['10:00:52.14'::TIME,'ALLBALLS'::TIME,NULL]::TIME[]);


DROP function pltest.PLPERLinoutpara_unname_196 ( INOUT  TIME[] );
----Case 196 end----


----Case 197 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_197 ( INOUT  REAL[] ) RETURNS REAL[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_197(pltest.tableREALarray.key) AS ID from pltest.tableREALarray ORDER BY ID;

create table pltest.tableREALarray1 as select pltest.PLPERLinoutpara_unname_197(pltest.tableREALarray.key) AS ID from pltest.tableREALarray DISTRIBUTED randomly;

select * from pltest.tableREALarray1 ORDER BY ID;

drop table pltest.tableREALarray1;

select pltest.PLPERLinoutpara_unname_197(array[39.333::REAL,'NAN'::REAL,'-INFINITY'::REAL,'+INFINITY'::REAL,NULL]::REAL[]);


select * from pltest.PLPERLinoutpara_unname_197(array[39.333::REAL,'NAN'::REAL,'-INFINITY'::REAL,'+INFINITY'::REAL,NULL]::REAL[]);


DROP function pltest.PLPERLinoutpara_unname_197 ( INOUT  REAL[] );
----Case 197 end----




----Case 199 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_199 ( INOUT  MONEY[] ) RETURNS MONEY[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_199(pltest.tableMONEYarray.key) AS ID from pltest.tableMONEYarray ORDER BY ID;

create table pltest.tableMONEYarray1 as select pltest.PLPERLinoutpara_unname_199(pltest.tableMONEYarray.key) AS ID from pltest.tableMONEYarray DISTRIBUTED randomly;

select * from pltest.tableMONEYarray1 ORDER BY ID;

drop table pltest.tableMONEYarray1;

select pltest.PLPERLinoutpara_unname_199(array['$-21474836.48'::MONEY,'$21474836.47'::MONEY,'$1234'::MONEY,NULL]::MONEY[]);


select * from pltest.PLPERLinoutpara_unname_199(array['$-21474836.48'::MONEY,'$21474836.47'::MONEY,'$1234'::MONEY,NULL]::MONEY[]);


DROP function pltest.PLPERLinoutpara_unname_199 ( INOUT  MONEY[] );
----Case 199 end----


----Case 200 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_200 ( INOUT  POINT[] ) RETURNS POINT[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinoutpara_unname_200(array['(1,2)'::POINT,NULL]::POINT[]);


select * from pltest.PLPERLinoutpara_unname_200(array['(1,2)'::POINT,NULL]::POINT[]);


DROP function pltest.PLPERLinoutpara_unname_200 ( INOUT  POINT[] );
----Case 200 end----


----Case 201 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_201 ( INOUT  CHAR[] ) RETURNS CHAR[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_201(pltest.tableCHARarray.key) AS ID from pltest.tableCHARarray ORDER BY ID;

create table pltest.tableCHARarray1 as select pltest.PLPERLinoutpara_unname_201(pltest.tableCHARarray.key) AS ID from pltest.tableCHARarray DISTRIBUTED randomly;

select * from pltest.tableCHARarray1 ORDER BY ID;

drop table pltest.tableCHARarray1;

select pltest.PLPERLinoutpara_unname_201(array['B'::CHAR,NULL::CHAR]::CHAR[]);


select * from pltest.PLPERLinoutpara_unname_201(array['B'::CHAR,NULL::CHAR]::CHAR[]);


DROP function pltest.PLPERLinoutpara_unname_201 ( INOUT  CHAR[] );
----Case 201 end----


----Case 202 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_202 ( INOUT  MACADDR[] ) RETURNS MACADDR[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_202(pltest.tableMACADDRarray.key) AS ID from pltest.tableMACADDRarray ORDER BY ID;

create table pltest.tableMACADDRarray1 as select pltest.PLPERLinoutpara_unname_202(pltest.tableMACADDRarray.key) AS ID from pltest.tableMACADDRarray DISTRIBUTED randomly;

select * from pltest.tableMACADDRarray1 ORDER BY ID;

drop table pltest.tableMACADDRarray1;

select pltest.PLPERLinoutpara_unname_202(array['12:34:56:78:90:AB'::MACADDR,NULL]::MACADDR[]);


select * from pltest.PLPERLinoutpara_unname_202(array['12:34:56:78:90:AB'::MACADDR,NULL]::MACADDR[]);


DROP function pltest.PLPERLinoutpara_unname_202 ( INOUT  MACADDR[] );
----Case 202 end----


----Case 203 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_203 ( INOUT  FLOAT8[] ) RETURNS FLOAT8[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_203(pltest.tableFLOAT8array.key) AS ID from pltest.tableFLOAT8array ORDER BY ID;

create table pltest.tableFLOAT8array1 as select pltest.PLPERLinoutpara_unname_203(pltest.tableFLOAT8array.key) AS ID from pltest.tableFLOAT8array DISTRIBUTED randomly;

select * from pltest.tableFLOAT8array1 ORDER BY ID;

drop table pltest.tableFLOAT8array1;

select pltest.PLPERLinoutpara_unname_203(array[123456789.98765432::FLOAT8,'NAN'::FLOAT8,'-INFINITY'::FLOAT8,'+INFINITY'::FLOAT8,NULL]::FLOAT8[]);


select * from pltest.PLPERLinoutpara_unname_203(array[123456789.98765432::FLOAT8,'NAN'::FLOAT8,'-INFINITY'::FLOAT8,'+INFINITY'::FLOAT8,NULL]::FLOAT8[]);


DROP function pltest.PLPERLinoutpara_unname_203 ( INOUT  FLOAT8[] );
----Case 203 end----


----Case 204 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_204 ( INOUT  LSEG[] ) RETURNS LSEG[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinoutpara_unname_204(array['((1,1),(2,2))'::LSEG,NULL]::LSEG[]);


select * from pltest.PLPERLinoutpara_unname_204(array['((1,1),(2,2))'::LSEG,NULL]::LSEG[]);


DROP function pltest.PLPERLinoutpara_unname_204 ( INOUT  LSEG[] );
----Case 204 end----


----Case 205 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_205 ( INOUT  DATE[] ) RETURNS DATE[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_205(pltest.tableDATEarray.key) AS ID from pltest.tableDATEarray ORDER BY ID;

create table pltest.tableDATEarray1 as select pltest.PLPERLinoutpara_unname_205(pltest.tableDATEarray.key) AS ID from pltest.tableDATEarray DISTRIBUTED randomly;

select * from pltest.tableDATEarray1 ORDER BY ID;

drop table pltest.tableDATEarray1;

select pltest.PLPERLinoutpara_unname_205(array['2011-08-12'::DATE,'EPOCH'::DATE,NULL]::DATE[]);


select * from pltest.PLPERLinoutpara_unname_205(array['2011-08-12'::DATE,'EPOCH'::DATE,NULL]::DATE[]);


DROP function pltest.PLPERLinoutpara_unname_205 ( INOUT  DATE[] );
----Case 205 end----


----Case 206 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_206 ( INOUT  BIGINT[] ) RETURNS BIGINT[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_206(pltest.tableBIGINTarray.key) AS ID from pltest.tableBIGINTarray ORDER BY ID;

create table pltest.tableBIGINTarray1 as select pltest.PLPERLinoutpara_unname_206(pltest.tableBIGINTarray.key) AS ID from pltest.tableBIGINTarray DISTRIBUTED randomly;

select * from pltest.tableBIGINTarray1 ORDER BY ID;

drop table pltest.tableBIGINTarray1;

select pltest.PLPERLinoutpara_unname_206(array[(-9223372036854775808)::BIGINT,9223372036854775807::BIGINT,123456789::BIGINT,NULL]::BIGINT[]);


select * from pltest.PLPERLinoutpara_unname_206(array[(-9223372036854775808)::BIGINT,9223372036854775807::BIGINT,123456789::BIGINT,NULL]::BIGINT[]);


DROP function pltest.PLPERLinoutpara_unname_206 ( INOUT  BIGINT[] );
----Case 206 end----

----Case 208 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_208 ( INOUT  PATH[] ) RETURNS PATH[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinoutpara_unname_208(array['(1,1),(2,2)'::PATH,NULL]::PATH[]);


select * from pltest.PLPERLinoutpara_unname_208(array['(1,1),(2,2)'::PATH,NULL]::PATH[]);


DROP function pltest.PLPERLinoutpara_unname_208 ( INOUT  PATH[] );
----Case 208 end----


----Case 209 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_209 ( INOUT  POLYGON[] ) RETURNS POLYGON[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinoutpara_unname_209(array['(0,0),(1,1),(2,0)'::POLYGON,NULL]::POLYGON[]);


select * from pltest.PLPERLinoutpara_unname_209(array['(0,0),(1,1),(2,0)'::POLYGON,NULL]::POLYGON[]);


DROP function pltest.PLPERLinoutpara_unname_209 ( INOUT  POLYGON[] );
----Case 209 end----


----Case 210 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_210 ( INOUT  INET[] ) RETURNS INET[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_210(pltest.tableINETarray.key) AS ID from pltest.tableINETarray ORDER BY ID;

create table pltest.tableINETarray1 as select pltest.PLPERLinoutpara_unname_210(pltest.tableINETarray.key) AS ID from pltest.tableINETarray DISTRIBUTED randomly;

select * from pltest.tableINETarray1 ORDER BY ID;

drop table pltest.tableINETarray1;

select pltest.PLPERLinoutpara_unname_210(array['192.168.0.1'::INET,NULL]::INET[]);


select * from pltest.PLPERLinoutpara_unname_210(array['192.168.0.1'::INET,NULL]::INET[]);


DROP function pltest.PLPERLinoutpara_unname_210 ( INOUT  INET[] );
----Case 210 end----




----Case 212 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_212 ( INOUT  TIMESTAMPTZ[] ) RETURNS TIMESTAMPTZ[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_212(pltest.tableTIMESTAMPTZarray.key) AS ID from pltest.tableTIMESTAMPTZarray ORDER BY ID;

create table pltest.tableTIMESTAMPTZarray1 as select pltest.PLPERLinoutpara_unname_212(pltest.tableTIMESTAMPTZarray.key) AS ID from pltest.tableTIMESTAMPTZarray DISTRIBUTED randomly;

select * from pltest.tableTIMESTAMPTZarray1 ORDER BY ID;

drop table pltest.tableTIMESTAMPTZarray1;

select pltest.PLPERLinoutpara_unname_212(array['2011-08-12 10:00:52.14'::TIMESTAMPTZ,NULL]::TIMESTAMPTZ[]);


select * from pltest.PLPERLinoutpara_unname_212(array['2011-08-12 10:00:52.14'::TIMESTAMPTZ,NULL]::TIMESTAMPTZ[]);


DROP function pltest.PLPERLinoutpara_unname_212 ( INOUT  TIMESTAMPTZ[] );
----Case 212 end----


----Case 213 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_213 ( INOUT  CHAR(4)[] ) RETURNS CHAR(4)[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_213(pltest.tableCHAR4array.key) AS ID from pltest.tableCHAR4array ORDER BY ID;

create table pltest.tableCHAR4array1 as select pltest.PLPERLinoutpara_unname_213(pltest.tableCHAR4array.key) AS ID from pltest.tableCHAR4array DISTRIBUTED randomly;

select * from pltest.tableCHAR4array1 ORDER BY ID;

drop table pltest.tableCHAR4array1;

select pltest.PLPERLinoutpara_unname_213(array['SSSS'::CHAR(4),NULL]::CHAR(4)[]);


select * from pltest.PLPERLinoutpara_unname_213(array['SSSS'::CHAR(4),NULL]::CHAR(4)[]);


DROP function pltest.PLPERLinoutpara_unname_213 ( INOUT  CHAR(4)[] );
----Case 213 end----


----Case 214 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_214 ( INOUT  VARCHAR(16)[] ) RETURNS VARCHAR(16)[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_214(pltest.tableVARCHAR16array.key) AS ID from pltest.tableVARCHAR16array ORDER BY ID;

create table pltest.tableVARCHAR16array1 as select pltest.PLPERLinoutpara_unname_214(pltest.tableVARCHAR16array.key) AS ID from pltest.tableVARCHAR16array DISTRIBUTED randomly;

select * from pltest.tableVARCHAR16array1 ORDER BY ID;

drop table pltest.tableVARCHAR16array1;

select pltest.PLPERLinoutpara_unname_214(array['ASDFGHJKL'::VARCHAR(16),NULL]::VARCHAR(16)[]);


select * from pltest.PLPERLinoutpara_unname_214(array['ASDFGHJKL'::VARCHAR(16),NULL]::VARCHAR(16)[]);


DROP function pltest.PLPERLinoutpara_unname_214 ( INOUT  VARCHAR(16)[] );
----Case 214 end----


----Case 215 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_215 ( INOUT  INT4[] ) RETURNS INT4[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_215(pltest.tableINT4array.key) AS ID from pltest.tableINT4array ORDER BY ID;

create table pltest.tableINT4array1 as select pltest.PLPERLinoutpara_unname_215(pltest.tableINT4array.key) AS ID from pltest.tableINT4array DISTRIBUTED randomly;

select * from pltest.tableINT4array1 ORDER BY ID;

drop table pltest.tableINT4array1;

select pltest.PLPERLinoutpara_unname_215(array[(-2147483648)::INT4,2147483647::INT4,1234,NULL]::INT4[]);


select * from pltest.PLPERLinoutpara_unname_215(array[(-2147483648)::INT4,2147483647::INT4,1234,NULL]::INT4[]);


DROP function pltest.PLPERLinoutpara_unname_215 ( INOUT  INT4[] );
----Case 215 end----


----Case 216 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_216 ( INOUT  CIDR[] ) RETURNS CIDR[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_216(pltest.tableCIDRarray.key) AS ID from pltest.tableCIDRarray ORDER BY ID;

create table pltest.tableCIDRarray1 as select pltest.PLPERLinoutpara_unname_216(pltest.tableCIDRarray.key) AS ID from pltest.tableCIDRarray DISTRIBUTED randomly;

select * from pltest.tableCIDRarray1 ORDER BY ID;

drop table pltest.tableCIDRarray1;

select pltest.PLPERLinoutpara_unname_216(array['192.168.0.1/32'::CIDR,NULL]::CIDR[]);


select * from pltest.PLPERLinoutpara_unname_216(array['192.168.0.1/32'::CIDR,NULL]::CIDR[]);


DROP function pltest.PLPERLinoutpara_unname_216 ( INOUT  CIDR[] );
----Case 216 end----


----Case 217 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_217 ( INOUT  "char"[] ) RETURNS "char"[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_217(pltest.tableQUTOCHARarray.key) AS ID from pltest.tableQUTOCHARarray ORDER BY ID;

create table pltest.tableQUTOCHARarray1 as select pltest.PLPERLinoutpara_unname_217(pltest.tableQUTOCHARarray.key) AS ID from pltest.tableQUTOCHARarray DISTRIBUTED randomly;

select * from pltest.tableQUTOCHARarray1 ORDER BY ID;

drop table pltest.tableQUTOCHARarray1;

select pltest.PLPERLinoutpara_unname_217(array['a'::"char",null::"char"]::"char"[]);


select * from pltest.PLPERLinoutpara_unname_217(array['a'::"char",null::"char"]::"char"[]);


DROP function pltest.PLPERLinoutpara_unname_217 ( INOUT  "char"[] );
----Case 217 end----


----Case 218 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_218 ( INOUT  TIMESTAMP[] ) RETURNS TIMESTAMP[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_218(pltest.tableTIMESTAMParray.key) AS ID from pltest.tableTIMESTAMParray ORDER BY ID;

create table pltest.tableTIMESTAMParray1 as select pltest.PLPERLinoutpara_unname_218(pltest.tableTIMESTAMParray.key) AS ID from pltest.tableTIMESTAMParray DISTRIBUTED randomly;

select * from pltest.tableTIMESTAMParray1 ORDER BY ID;

drop table pltest.tableTIMESTAMParray1;

select pltest.PLPERLinoutpara_unname_218(array['2011-08-12 10:00:52.14'::TIMESTAMP,NULL]::TIMESTAMP[]);


select * from pltest.PLPERLinoutpara_unname_218(array['2011-08-12 10:00:52.14'::TIMESTAMP,NULL]::TIMESTAMP[]);


DROP function pltest.PLPERLinoutpara_unname_218 ( INOUT  TIMESTAMP[] );
----Case 218 end----


----Case 219 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_219 ( INOUT  BOX[] ) RETURNS BOX[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinoutpara_unname_219(array['((1,2),(3,4))'::BOX,NULL]::BOX[]);


select * from pltest.PLPERLinoutpara_unname_219(array['((1,2),(3,4))'::BOX,NULL]::BOX[]);


DROP function pltest.PLPERLinoutpara_unname_219 ( INOUT  BOX[] );
----Case 219 end----


----Case 220 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_220 ( INOUT  TEXT[] ) RETURNS TEXT[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_220(pltest.tableTEXTarray.key) AS ID from pltest.tableTEXTarray ORDER BY ID;

create table pltest.tableTEXTarray1 as select pltest.PLPERLinoutpara_unname_220(pltest.tableTEXTarray.key) AS ID from pltest.tableTEXTarray DISTRIBUTED randomly;

select * from pltest.tableTEXTarray1 ORDER BY ID;

drop table pltest.tableTEXTarray1;

select pltest.PLPERLinoutpara_unname_220(array['QAZWSXEDCRFVTGB'::TEXT,NULL]::TEXT[]);


select * from pltest.PLPERLinoutpara_unname_220(array['QAZWSXEDCRFVTGB'::TEXT,NULL]::TEXT[]);


DROP function pltest.PLPERLinoutpara_unname_220 ( INOUT  TEXT[] );
----Case 220 end----


----Case 221 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_221 ( INOUT  BIT VARYING(10)[] ) RETURNS BIT VARYING(10)[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_221(pltest.tableBITVARYING10array.key) AS ID from pltest.tableBITVARYING10array ORDER BY ID;

create table pltest.tableBITVARYING10array1 as select pltest.PLPERLinoutpara_unname_221(pltest.tableBITVARYING10array.key) AS ID from pltest.tableBITVARYING10array DISTRIBUTED randomly;

select * from pltest.tableBITVARYING10array1 ORDER BY ID;

drop table pltest.tableBITVARYING10array1;

select pltest.PLPERLinoutpara_unname_221(array[B'11001111'::BIT VARYING(10),NULL]::BIT VARYING(10)[]);


select * from pltest.PLPERLinoutpara_unname_221(array[B'11001111'::BIT VARYING(10),NULL]::BIT VARYING(10)[]);


DROP function pltest.PLPERLinoutpara_unname_221 ( INOUT  BIT VARYING(10)[] );
----Case 221 end----


----Case 222 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_222 ( INOUT  NUMERIC[] ) RETURNS NUMERIC[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_222(pltest.tableNUMERICarray.key) AS ID from pltest.tableNUMERICarray ORDER BY ID;

create table pltest.tableNUMERICarray1 as select pltest.PLPERLinoutpara_unname_222(pltest.tableNUMERICarray.key) AS ID from pltest.tableNUMERICarray DISTRIBUTED randomly;

select * from pltest.tableNUMERICarray1 ORDER BY ID;

drop table pltest.tableNUMERICarray1;

select pltest.PLPERLinoutpara_unname_222(array[1234567890.0987654321::NUMERIC,'NAN'::NUMERIC,2::NUMERIC^128 / 3::NUMERIC^129,2::NUMERIC^128,0.5::NUMERIC,NULL]::NUMERIC[]);


select * from pltest.PLPERLinoutpara_unname_222(array[1234567890.0987654321::NUMERIC,'NAN'::NUMERIC,2::NUMERIC^128 / 3::NUMERIC^129,2::NUMERIC^128,0.5::NUMERIC,NULL]::NUMERIC[]);


DROP function pltest.PLPERLinoutpara_unname_222 ( INOUT  NUMERIC[] );
----Case 222 end----


----Case 224 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_224 ( INOUT  SMALLINT[] ) RETURNS SMALLINT[] as $$     
			my $key = shift; return $key;
$$ language PLPERL;

select pltest.PLPERLinoutpara_unname_224(pltest.tableSMALLINTarray.key) AS ID from pltest.tableSMALLINTarray ORDER BY ID;

create table pltest.tableSMALLINTarray1 as select pltest.PLPERLinoutpara_unname_224(pltest.tableSMALLINTarray.key) AS ID from pltest.tableSMALLINTarray DISTRIBUTED randomly;

select * from pltest.tableSMALLINTarray1 ORDER BY ID;

drop table pltest.tableSMALLINTarray1;

select pltest.PLPERLinoutpara_unname_224(array[(-32768)::SMALLINT,32767::SMALLINT,1234::SMALLINT,NULL]::SMALLINT[]);


select * from pltest.PLPERLinoutpara_unname_224(array[(-32768)::SMALLINT,32767::SMALLINT,1234::SMALLINT,NULL]::SMALLINT[]);


DROP function pltest.PLPERLinoutpara_unname_224 ( INOUT  SMALLINT[] );
----Case 224 end----


----Case 225 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_225 ( INOUT  pltest.COMPOSITE_INTERNET ) RETURNS pltest.COMPOSITE_INTERNET as $$     
			my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinoutpara_unname_225(((-32768)::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET);

select pltest.PLPERLinoutpara_unname_225((32767::SMALLINT, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET);

select pltest.PLPERLinoutpara_unname_225((1234::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET);

select pltest.PLPERLinoutpara_unname_225((NULL, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET);

select (pltest.PLPERLinoutpara_unname_225(((-32768)::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET)).*;

select (pltest.PLPERLinoutpara_unname_225((32767::SMALLINT, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET)).*;

select (pltest.PLPERLinoutpara_unname_225((1234::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET)).*;

select (pltest.PLPERLinoutpara_unname_225((NULL, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET)).*;

select * from pltest.PLPERLinoutpara_unname_225(((-32768)::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET);

select * from pltest.PLPERLinoutpara_unname_225((32767::SMALLINT, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET);

select * from pltest.PLPERLinoutpara_unname_225((1234::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET);

select * from pltest.PLPERLinoutpara_unname_225((NULL, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET);


DROP function pltest.PLPERLinoutpara_unname_225 ( INOUT  pltest.COMPOSITE_INTERNET );
----Case 225 end----


----Case 226 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_226 ( INOUT  pltest.COMPOSITE_CHAR ) RETURNS pltest.COMPOSITE_CHAR as $$     
			my $key = shift; return $key;
$$ language PLPERL;



select pltest.PLPERLinoutpara_unname_226(('SSSS'::CHAR(4), 'ASDFGHJKL'::VARCHAR(16), 'QAZWSXEDCRFVTGB'::TEXT)::pltest.COMPOSITE_CHAR);

select pltest.PLPERLinoutpara_unname_226((NULL, NULL, NULL)::pltest.COMPOSITE_CHAR);

select (pltest.PLPERLinoutpara_unname_226(('SSSS'::CHAR(4), 'ASDFGHJKL'::VARCHAR(16), 'QAZWSXEDCRFVTGB'::TEXT)::pltest.COMPOSITE_CHAR)).*;

select (pltest.PLPERLinoutpara_unname_226((NULL, NULL, NULL)::pltest.COMPOSITE_CHAR)).*;

select * from pltest.PLPERLinoutpara_unname_226(('SSSS'::CHAR(4), 'ASDFGHJKL'::VARCHAR(16), 'QAZWSXEDCRFVTGB'::TEXT)::pltest.COMPOSITE_CHAR);

select * from pltest.PLPERLinoutpara_unname_226((NULL, NULL, NULL)::pltest.COMPOSITE_CHAR);


DROP function pltest.PLPERLinoutpara_unname_226 ( INOUT  pltest.COMPOSITE_CHAR );
----Case 226 end----




----Case 227 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_227 ( INOUT  pltest.COMPOSITE_BIT ) RETURNS pltest.COMPOSITE_BIT as $$     
			my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinoutpara_unname_227(((-32768)::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), FALSE::BOOLEAN, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT);

select pltest.PLPERLinoutpara_unname_227((32767::SMALLINT, NULL, NULL, TRUE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT);

select pltest.PLPERLinoutpara_unname_227((1234::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), NULL, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT);

select pltest.PLPERLinoutpara_unname_227((NULL, NULL, NULL, FALSE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT);

select (pltest.PLPERLinoutpara_unname_227(((-32768)::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), FALSE::BOOLEAN, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT)).*;

select (pltest.PLPERLinoutpara_unname_227((32767::SMALLINT, NULL, NULL, TRUE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT)).*;

select (pltest.PLPERLinoutpara_unname_227((1234::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), NULL, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT)).*;

select (pltest.PLPERLinoutpara_unname_227((NULL, NULL, NULL, FALSE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT)).*;

select * from pltest.PLPERLinoutpara_unname_227(((-32768)::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), FALSE::BOOLEAN, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT);

select * from pltest.PLPERLinoutpara_unname_227((32767::SMALLINT, NULL, NULL, TRUE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT);

select * from pltest.PLPERLinoutpara_unname_227((1234::SMALLINT, B'11001111'::BIT(8), B'11001111'::BIT VARYING(10), NULL, E'\\000'::BYTEA)::pltest.COMPOSITE_BIT);

select * from pltest.PLPERLinoutpara_unname_227((NULL, NULL, NULL, FALSE::BOOLEAN, NULL)::pltest.COMPOSITE_BIT);


DROP function pltest.PLPERLinoutpara_unname_227 ( INOUT  pltest.COMPOSITE_BIT );
----Case 227 end----


----Case 228 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_228 ( INOUT  pltest.COMPOSITE_GEOMETRIC ) RETURNS pltest.COMPOSITE_GEOMETRIC as $$     
			my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinoutpara_unname_228(((-32768)::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC);

select pltest.PLPERLinoutpara_unname_228((32767::SMALLINT, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC);

select pltest.PLPERLinoutpara_unname_228((1234::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC);

select pltest.PLPERLinoutpara_unname_228((NULL, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC);

select (pltest.PLPERLinoutpara_unname_228(((-32768)::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC)).*;

select (pltest.PLPERLinoutpara_unname_228((32767::SMALLINT, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC)).*;

select (pltest.PLPERLinoutpara_unname_228((1234::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC)).*;

select (pltest.PLPERLinoutpara_unname_228((NULL, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC)).*;

select * from pltest.PLPERLinoutpara_unname_228(((-32768)::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC);

select * from pltest.PLPERLinoutpara_unname_228((32767::SMALLINT, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC);

select * from pltest.PLPERLinoutpara_unname_228((1234::SMALLINT, '(0,0),(1,1),(2,0)'::POLYGON, '(1,1),(2,2)'::PATH, '((1,1),(2,2))'::LSEG, '<(1,1),2>'::CIRCLE, '(1,2)'::POINT, '((1,2),(3,4))'::BOX)::pltest.COMPOSITE_GEOMETRIC);

select * from pltest.PLPERLinoutpara_unname_228((NULL, NULL, NULL, NULL, NULL, NULL, NULL)::pltest.COMPOSITE_GEOMETRIC);


DROP function pltest.PLPERLinoutpara_unname_228 ( INOUT  pltest.COMPOSITE_GEOMETRIC );
----Case 228 end----


----Case 229 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_229 ( INOUT  pltest.COMPOSITE_DATE ) RETURNS pltest.COMPOSITE_DATE as $$     
			my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinoutpara_unname_229(('2011-08-12'::DATE, '10:00:52.14'::TIME, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE);

select pltest.PLPERLinoutpara_unname_229(('EPOCH'::DATE, 'ALLBALLS'::TIME, NULL, NULL, NULL)::pltest.COMPOSITE_DATE);

select pltest.PLPERLinoutpara_unname_229((NULL, NULL, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE);

select (pltest.PLPERLinoutpara_unname_229(('2011-08-12'::DATE, '10:00:52.14'::TIME, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE)).*;

select (pltest.PLPERLinoutpara_unname_229(('EPOCH'::DATE, 'ALLBALLS'::TIME, NULL, NULL, NULL)::pltest.COMPOSITE_DATE)).*;

select (pltest.PLPERLinoutpara_unname_229((NULL, NULL, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE)).*;

select * from pltest.PLPERLinoutpara_unname_229(('2011-08-12'::DATE, '10:00:52.14'::TIME, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE);

select * from pltest.PLPERLinoutpara_unname_229(('EPOCH'::DATE, 'ALLBALLS'::TIME, NULL, NULL, NULL)::pltest.COMPOSITE_DATE);

select * from pltest.PLPERLinoutpara_unname_229((NULL, NULL, '2011-08-12 10:00:52.14'::TIMETZ, '2011-08-12 10:00:52.14'::TIMESTAMP, '2011-08-12 10:00:52.14'::TIMESTAMPTZ)::pltest.COMPOSITE_DATE);


DROP function pltest.PLPERLinoutpara_unname_229 ( INOUT  pltest.COMPOSITE_DATE );
----Case 229 end----


----Case 230 begin----
-- 
-- 		Unnamed parameter test;
-- 		CREATE OR REPLACE FUNCTION functionname(INOUT b/c/b[]/c[]) RETURN b/c/b[]/c[].
-- 		
CREATE or REPLACE function pltest.PLPERLinoutpara_unname_230 ( INOUT  pltest.COMPOSITE_NUMERIC ) RETURNS pltest.COMPOSITE_NUMERIC as $$     
			my $key = shift; return $key;
$$ language PLPERL;


select pltest.PLPERLinoutpara_unname_230(((-9223372036854775808)::BIGINT, 1234567890.0987654321::NUMERIC, 123456789.98765432::FLOAT8, (-2147483648)::INT4, '$-21474836.48'::MONEY, 39.333::REAL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select pltest.PLPERLinoutpara_unname_230((9223372036854775807::BIGINT, 'NAN'::NUMERIC, 'NAN'::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 'NAN'::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select pltest.PLPERLinoutpara_unname_230((123456789::BIGINT, 2::NUMERIC^128 / 3::NUMERIC^129, '-INFINITY'::FLOAT8, 1234, '$1234'::MONEY, '-INFINITY'::REAL, 1234::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select pltest.PLPERLinoutpara_unname_230((NULL, 2::NUMERIC^128, '+INFINITY'::FLOAT8, NULL, NULL, '+INFINITY'::REAL, NULL)::pltest.COMPOSITE_NUMERIC);

select pltest.PLPERLinoutpara_unname_230(((-9223372036854775808)::BIGINT, 0.5::NUMERIC, NULL, (-2147483648)::INT4, '$-21474836.48'::MONEY, NULL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select pltest.PLPERLinoutpara_unname_230((9223372036854775807::BIGINT, NULL, 123456789.98765432::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 39.333::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select (pltest.PLPERLinoutpara_unname_230(((-9223372036854775808)::BIGINT, 1234567890.0987654321::NUMERIC, 123456789.98765432::FLOAT8, (-2147483648)::INT4, '$-21474836.48'::MONEY, 39.333::REAL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC)).*;

select (pltest.PLPERLinoutpara_unname_230((9223372036854775807::BIGINT, 'NAN'::NUMERIC, 'NAN'::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 'NAN'::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC)).*;

select (pltest.PLPERLinoutpara_unname_230((123456789::BIGINT, 2::NUMERIC^128 / 3::NUMERIC^129, '-INFINITY'::FLOAT8, 1234, '$1234'::MONEY, '-INFINITY'::REAL, 1234::SMALLINT)::pltest.COMPOSITE_NUMERIC)).*;

select (pltest.PLPERLinoutpara_unname_230((NULL, 2::NUMERIC^128, '+INFINITY'::FLOAT8, NULL, NULL, '+INFINITY'::REAL, NULL)::pltest.COMPOSITE_NUMERIC)).*;

select (pltest.PLPERLinoutpara_unname_230(((-9223372036854775808)::BIGINT, 0.5::NUMERIC, NULL, (-2147483648)::INT4, '$-21474836.48'::MONEY, NULL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC)).*;

select (pltest.PLPERLinoutpara_unname_230((9223372036854775807::BIGINT, NULL, 123456789.98765432::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 39.333::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC)).*;

select * from pltest.PLPERLinoutpara_unname_230(((-9223372036854775808)::BIGINT, 1234567890.0987654321::NUMERIC, 123456789.98765432::FLOAT8, (-2147483648)::INT4, '$-21474836.48'::MONEY, 39.333::REAL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select * from pltest.PLPERLinoutpara_unname_230((9223372036854775807::BIGINT, 'NAN'::NUMERIC, 'NAN'::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 'NAN'::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select * from pltest.PLPERLinoutpara_unname_230((123456789::BIGINT, 2::NUMERIC^128 / 3::NUMERIC^129, '-INFINITY'::FLOAT8, 1234, '$1234'::MONEY, '-INFINITY'::REAL, 1234::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select * from pltest.PLPERLinoutpara_unname_230((NULL, 2::NUMERIC^128, '+INFINITY'::FLOAT8, NULL, NULL, '+INFINITY'::REAL, NULL)::pltest.COMPOSITE_NUMERIC);

select * from pltest.PLPERLinoutpara_unname_230(((-9223372036854775808)::BIGINT, 0.5::NUMERIC, NULL, (-2147483648)::INT4, '$-21474836.48'::MONEY, NULL, (-32768)::SMALLINT)::pltest.COMPOSITE_NUMERIC);

select * from pltest.PLPERLinoutpara_unname_230((9223372036854775807::BIGINT, NULL, 123456789.98765432::FLOAT8, 2147483647::INT4, '$21474836.47'::MONEY, 39.333::REAL, 32767::SMALLINT)::pltest.COMPOSITE_NUMERIC);


DROP function pltest.PLPERLinoutpara_unname_230 ( INOUT  pltest.COMPOSITE_NUMERIC );
----Case 230 end----


