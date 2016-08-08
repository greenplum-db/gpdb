CREATE or REPLACE function pltest.PLPERLmulcall ( INOUT  REAL ) RETURNS REAL as $$
                        my $key = shift ; $key = $key + 1;  return $key;
$$ language PLPERL;

select key, 
       pltest.PLPERLmulcall(pltest.tableREAL.key), 
       key,
       pltest.PLPERLmulcall(pltest.tableREAL.key) 
from pltest.tableREAL ORDER BY key;

DROP function pltest.PLPERLmulcall(INOUT  REAL);

----Case 231 begin----
-- CREATE OR REPLACE FUNCTION functionname(table) RETURN TEXT.
CREATE or REPLACE function pltest.PLPERLtablein_231 ( IN  pltest.tupleCOMPOSITE_BIT ) RETURNS TEXT as $$  
			my ($emp) = @_;
			my $result = '';
			for my $key ( keys %$emp ) {
				$result = $result . ',' . $emp->{$key}
			}
		
			return substr($result, 1, length($result));
$$ language PLPERL;

select pltest.PLPERLtablein_231( pltest.tupleCOMPOSITE_BIT.* ) from pltest.tupleCOMPOSITE_BIT order by plperltablein_231;


DROP function pltest.PLPERLtablein_231 ( IN  pltest.tupleCOMPOSITE_BIT );
----Case 231 end----


----Case 232 begin----
-- CREATE OR REPLACE FUNCTION functionname(table) RETURN TEXT.
CREATE or REPLACE function pltest.PLPERLtablein_232 ( IN  pltest.tupleCOMPOSITE_INTERNET ) RETURNS TEXT as $$  
			my ($emp) = @_;
			my $result = '';
			for my $key ( keys %$emp ) {
				$result = $result . ',' . $emp->{$key}
			}
		
			return substr($result, 1, length($result));
$$ language PLPERL;

select pltest.PLPERLtablein_232( pltest.tupleCOMPOSITE_INTERNET.* ) 
from pltest.tupleCOMPOSITE_INTERNET
order by PLPERLtablein_232;
DROP function pltest.PLPERLtablein_232 ( IN  pltest.tupleCOMPOSITE_INTERNET );
----Case 232 end----


----Case 233 begin----
-- CREATE OR REPLACE FUNCTION functionname(table) RETURN TEXT.
CREATE or REPLACE function pltest.PLPERLtablein_233 ( IN  pltest.tupleCOMPOSITE_DATE ) RETURNS TEXT as $$  
			my ($emp) = @_;
			my $result = '';
			for my $key ( keys %$emp ) {
				$result = $result . ',' . $emp->{$key}
			}
		
			return substr($result, 1, length($result));
$$ language PLPERL;

select pltest.PLPERLtablein_233( pltest.tupleCOMPOSITE_DATE.* ) 
from pltest.tupleCOMPOSITE_DATE
order by PLPERLtablein_233;


DROP function pltest.PLPERLtablein_233 ( IN  pltest.tupleCOMPOSITE_DATE );
----Case 233 end----


----Case 234 begin----
-- CREATE OR REPLACE FUNCTION functionname(table) RETURN TEXT.
CREATE or REPLACE function pltest.PLPERLtablein_234 ( IN  pltest.tupleCOMPOSITE_GEOMETRIC ) RETURNS TEXT as $$  
			my ($emp) = @_;
			my $result = '';
			for my $key ( keys %$emp ) {
				$result = $result . ',' . $emp->{$key}
			}
		
			return substr($result, 1, length($result));
$$ language PLPERL;

select pltest.PLPERLtablein_234( pltest.tupleCOMPOSITE_GEOMETRIC.* ) 
from pltest.tupleCOMPOSITE_GEOMETRIC
order by PLPERLtablein_234;


DROP function pltest.PLPERLtablein_234 ( IN  pltest.tupleCOMPOSITE_GEOMETRIC );
----Case 234 end----


----Case 235 begin----
-- CREATE OR REPLACE FUNCTION functionname(table) RETURN TEXT.
CREATE or REPLACE function pltest.PLPERLtablein_235 ( IN  pltest.tupleCOMPOSITE_CHAR ) RETURNS TEXT as $$  
			my ($emp) = @_;
			my $result = '';
			for my $key ( keys %$emp ) {
				$result = $result . ',' . $emp->{$key}
			}
		
			return substr($result, 1, length($result));
$$ language PLPERL;

select pltest.PLPERLtablein_235( pltest.tupleCOMPOSITE_CHAR.* ) 
from pltest.tupleCOMPOSITE_CHAR
order by PLPERLtablein_235;

DROP function pltest.PLPERLtablein_235 ( IN  pltest.tupleCOMPOSITE_CHAR );
----Case 235 end----


----Case 236 begin----
-- CREATE OR REPLACE FUNCTION functionname(table) RETURN TEXT.
CREATE or REPLACE function pltest.PLPERLtablein_236 ( IN  pltest.tupleCOMPOSITE_NUMERIC ) RETURNS TEXT as $$  
			my ($emp) = @_;
			my $result = '';
			for my $key ( keys %$emp ) {
				$result = $result . ',' . $emp->{$key}
			}
		
			return substr($result, 1, length($result));
$$ language PLPERL;

select pltest.PLPERLtablein_236( pltest.tupleCOMPOSITE_NUMERIC.* ) 
from pltest.tupleCOMPOSITE_NUMERIC
order by PLPERLtablein_236;


DROP function pltest.PLPERLtablein_236 ( IN  pltest.tupleCOMPOSITE_NUMERIC );
----Case 236 end----


----Case 237 begin----
-- multiple parameter
CREATE or REPLACE function pltest.PLPERLmultiplepara_inet_237 ( IN  INET , IN  CIDR , IN  MACADDR ) RETURNS TEXT as $$  
		    my $var1 = shift;
		    my $var2 = shift;
		    my $var3 = shift;
		    return $var1 . "," . $var2 . "," . $var3;
$$ language PLPERL;

select pltest.PLPERLmultiplepara_inet_237(t0.key, t1.key, t2.key) from pltest.tableINET as t0, pltest.tableCIDR as t1, pltest.tableMACADDR as t2
order by plperlmultiplepara_inet_237;

select pltest.PLPERLmultiplepara_inet_237('192.168.0.1'::INET,'192.168.0.1/32'::CIDR,'12:34:56:78:90:AB'::MACADDR);

select pltest.PLPERLmultiplepara_inet_237(NULL,NULL,NULL);

select * from pltest.PLPERLmultiplepara_inet_237('192.168.0.1'::INET,'192.168.0.1/32'::CIDR,'12:34:56:78:90:AB'::MACADDR);

select * from pltest.PLPERLmultiplepara_inet_237(NULL,NULL,NULL);

DROP function pltest.PLPERLmultiplepara_inet_237 ( IN  INET , IN  CIDR , IN  MACADDR );
----Case 237 end----


----Case 238 begin----
--  multiple parameter
CREATE or REPLACE function pltest.PLPERLmultiplepara_num_238 ( IN  INT4 , IN  FLOAT8 , IN  NUMERIC , IN  BIGINT ) RETURNS TEXT as $$  
		    my $var1 = shift;
		    my $var2 = shift;
		    my $var3 = shift;
		    my $var4 = shift;
		    return $var1 . "," . $var2 . "," . $var3 . "," . $var4
$$ language PLPERL;

select pltest.PLPERLmultiplepara_num_238(t0.key, t1.key, t2.key, t3.key) 
from pltest.tableINT4 as t0, pltest.tableFLOAT8 as t1, pltest.tableNUMERIC as t2, pltest.tableBIGINT as t3
order by PLPERLmultiplepara_num_238;

select pltest.PLPERLmultiplepara_num_238((-2147483648)::INT4,123456789.98765432::FLOAT8,1234567890.0987654321::NUMERIC,(-9223372036854775808)::BIGINT);

select pltest.PLPERLmultiplepara_num_238(2147483647::INT4,'NAN'::FLOAT8,'NAN'::NUMERIC,9223372036854775807::BIGINT);

select pltest.PLPERLmultiplepara_num_238(1234,'-INFINITY'::FLOAT8,2::NUMERIC^128 / 3::NUMERIC^129,123456789::BIGINT);

select pltest.PLPERLmultiplepara_num_238(NULL,'+INFINITY'::FLOAT8,2::NUMERIC^128,NULL);

select pltest.PLPERLmultiplepara_num_238((-2147483648)::INT4,NULL,0.5::NUMERIC,(-9223372036854775808)::BIGINT);

select pltest.PLPERLmultiplepara_num_238(2147483647::INT4,123456789.98765432::FLOAT8,NULL,9223372036854775807::BIGINT);

select * from pltest.PLPERLmultiplepara_num_238((-2147483648)::INT4,123456789.98765432::FLOAT8,1234567890.0987654321::NUMERIC,(-9223372036854775808)::BIGINT);

select * from pltest.PLPERLmultiplepara_num_238(2147483647::INT4,'NAN'::FLOAT8,'NAN'::NUMERIC,9223372036854775807::BIGINT);

select * from pltest.PLPERLmultiplepara_num_238(1234,'-INFINITY'::FLOAT8,2::NUMERIC^128 / 3::NUMERIC^129,123456789::BIGINT);

select * from pltest.PLPERLmultiplepara_num_238(NULL,'+INFINITY'::FLOAT8,2::NUMERIC^128,NULL);

select * from pltest.PLPERLmultiplepara_num_238((-2147483648)::INT4,NULL,0.5::NUMERIC,(-9223372036854775808)::BIGINT);

select * from pltest.PLPERLmultiplepara_num_238(2147483647::INT4,123456789.98765432::FLOAT8,NULL,9223372036854775807::BIGINT);

DROP function pltest.PLPERLmultiplepara_num_238 ( IN  INT4 , IN  FLOAT8 , IN  NUMERIC , IN  BIGINT );
----Case 238 end----


----Case 239 begin----
-- Value marshing
CREATE or REPLACE function pltest.PLPERLvaluemarshing_num_239 ( IN  INT4 , IN  FLOAT8 , IN  NUMERIC , IN  BIGINT ) RETURNS TEXT as $$  
		    my $var1 = shift ;
		    my $var2 = shift ;
		    my $var3 = shift;
		    my $var4 = shift;
		    
		    $var1 = $var1 + 1;
		    $var2 = $var2 + 1;
		    $var3 = $var3 + 1;
		    $var4 = $var4 + 1;
		    return $var1 . "," . $var2 . "," . $var3 . "," . $var4
$$ language PLPERL;

select pltest.PLPERLvaluemarshing_num_239(t0.key, t1.key, t2.key, t3.key) 
from pltest.tableINT4 as t0, pltest.tableFLOAT8 as t1, pltest.tableNUMERIC as t2, pltest.tableBIGINT as t3
order by PLPERLvaluemarshing_num_239;

select pltest.PLPERLvaluemarshing_num_239((-2147483648)::INT4,123456789.98765432::FLOAT8,1234567890.0987654321::NUMERIC,(-9223372036854775808)::BIGINT);

select pltest.PLPERLvaluemarshing_num_239(2147483647::INT4,'NAN'::FLOAT8,'NAN'::NUMERIC,9223372036854775807::BIGINT);

select pltest.PLPERLvaluemarshing_num_239(1234,'-INFINITY'::FLOAT8,2::NUMERIC^128 / 3::NUMERIC^129,123456789::BIGINT);

select pltest.PLPERLvaluemarshing_num_239(NULL,'+INFINITY'::FLOAT8,2::NUMERIC^128,NULL);

select pltest.PLPERLvaluemarshing_num_239((-2147483648)::INT4,NULL,0.5::NUMERIC,(-9223372036854775808)::BIGINT);

select pltest.PLPERLvaluemarshing_num_239(2147483647::INT4,123456789.98765432::FLOAT8,NULL,9223372036854775807::BIGINT);

select * from pltest.PLPERLvaluemarshing_num_239((-2147483648)::INT4,123456789.98765432::FLOAT8,1234567890.0987654321::NUMERIC,(-9223372036854775808)::BIGINT);

select * from pltest.PLPERLvaluemarshing_num_239(2147483647::INT4,'NAN'::FLOAT8,'NAN'::NUMERIC,9223372036854775807::BIGINT);

select * from pltest.PLPERLvaluemarshing_num_239(1234,'-INFINITY'::FLOAT8,2::NUMERIC^128 / 3::NUMERIC^129,123456789::BIGINT);

select * from pltest.PLPERLvaluemarshing_num_239(NULL,'+INFINITY'::FLOAT8,2::NUMERIC^128,NULL);

select * from pltest.PLPERLvaluemarshing_num_239((-2147483648)::INT4,NULL,0.5::NUMERIC,(-9223372036854775808)::BIGINT);

select * from pltest.PLPERLvaluemarshing_num_239(2147483647::INT4,123456789.98765432::FLOAT8,NULL,9223372036854775807::BIGINT);

DROP function pltest.PLPERLvaluemarshing_num_239 ( IN  INT4 , IN  FLOAT8 , IN  NUMERIC , IN  BIGINT );
----Case 239 end----


CREATE or REPLACE function pltest.PLPERLrecordin_240 ( IN  RECORD ) RETURNS TEXT as $$
        my ($emp) = @_;
        my $result = '';
        for my $key ( keys %$emp ) {
                $result = $result . ',' . $emp->{$key}
        }

        return substr($result, 1, length($result));
$$ language PLPERL;

select pltest.PLPERLrecordin_240(pltest.tableCOMPOSITE_INTERNET.key) AS ID from pltest.tableCOMPOSITE_INTERNET ORDER BY ID;

create table pltest.tableCOMPOSITE_INTERNET1 as select pltest.PLPERLrecordin_240(pltest.tableCOMPOSITE_INTERNET.key) AS ID from pltest.tableCOMPOSITE_INTERNET DISTRIBUTED randomly;

select * from pltest.tableCOMPOSITE_INTERNET1 ORDER BY ID;

drop table pltest.tableCOMPOSITE_INTERNET1;

select pltest.PLPERLrecordin_240(((-32768)::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET);

select pltest.PLPERLrecordin_240((32767::SMALLINT, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET);

select pltest.PLPERLrecordin_240((1234::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET);

select pltest.PLPERLrecordin_240((NULL, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET);

select * from pltest.PLPERLrecordin_240(((-32768)::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET);

select * from pltest.PLPERLrecordin_240((32767::SMALLINT, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET);

select * from pltest.PLPERLrecordin_240((1234::SMALLINT, '192.168.0.1'::INET, '192.168.0.1/32'::CIDR, '12:34:56:78:90:AB'::MACADDR)::pltest.COMPOSITE_INTERNET);

select * from pltest.PLPERLrecordin_240((NULL, NULL, NULL, NULL)::pltest.COMPOSITE_INTERNET);

DROP function pltest.PLPERLrecordin_240 ( IN  RECORD );

