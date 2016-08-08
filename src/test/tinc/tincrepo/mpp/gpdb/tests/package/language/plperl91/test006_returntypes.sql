----Case 0 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_0 (  ) RETURNS SETOF REAL as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEREAL;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_0() order by PLPERLsetoftype_0;

select * from pltest.PLPERLsetoftype_0() order by PLPERLsetoftype_0;

DROP function pltest.PLPERLsetoftype_0 (  );
----Case 0 end----




----Case 2 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_2 (  ) RETURNS SETOF VARCHAR(16) as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEVARCHAR16;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_2() order by PLPERLsetoftype_2;

select * from pltest.PLPERLsetoftype_2() order by PLPERLsetoftype_2;

DROP function pltest.PLPERLsetoftype_2 (  );
----Case 2 end----


----Case 3 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
\echo '--ERROR:  cannot determine result data type'
CREATE or REPLACE function pltest.PLPERLsetoftype_3 (  ) RETURNS SETOF ANYARRAY as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEANYARRAY;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;

----Case 3 end----


----Case 4 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_4 (  ) RETURNS SETOF UNKNOWN as $$  
			my $rv = spi_exec_query('select * from pltest.TABLECHAR;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;




select * from pltest.PLPERLsetoftype_4() order by PLPERLsetoftype_4;

DROP function pltest.PLPERLsetoftype_4 (  );
----Case 4 end----


----Case 5 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_5 (  ) RETURNS SETOF BIT VARYING(10) as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEBITVARYING10;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_5() order by PLPERLsetoftype_5;

select * from pltest.PLPERLsetoftype_5() order by PLPERLsetoftype_5;

DROP function pltest.PLPERLsetoftype_5 (  );
----Case 5 end----


----Case 6 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_6 (  ) RETURNS SETOF TIMESTAMPTZ as $$  
			my $rv = spi_exec_query('select * from pltest.TABLETIMESTAMPTZ;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_6() order by PLPERLsetoftype_6;

select * from pltest.PLPERLsetoftype_6() order by PLPERLsetoftype_6;

DROP function pltest.PLPERLsetoftype_6 (  );
----Case 6 end----


----Case 7 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_7 (  ) RETURNS SETOF "char" as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEchar;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_7() order by PLPERLsetoftype_7;

select * from pltest.PLPERLsetoftype_7() order by PLPERLsetoftype_7;

DROP function pltest.PLPERLsetoftype_7 (  );
----Case 7 end----


----Case 8 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_8 (  ) RETURNS SETOF CHAR as $$  
			my $rv = spi_exec_query('select * from pltest.TABLECHAR;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_8() order by PLPERLsetoftype_8;

select * from pltest.PLPERLsetoftype_8() order by PLPERLsetoftype_8;

DROP function pltest.PLPERLsetoftype_8 (  );
----Case 8 end----


----Case 9 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_9 (  ) RETURNS SETOF BIGINT as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEBIGINT;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_9() order by PLPERLsetoftype_9;

select * from pltest.PLPERLsetoftype_9() order by PLPERLsetoftype_9;

DROP function pltest.PLPERLsetoftype_9 (  );
----Case 9 end----


----Case 10 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_10 (  ) RETURNS SETOF DATE as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEDATE;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_10() order by PLPERLsetoftype_10;

select * from pltest.PLPERLsetoftype_10() order by PLPERLsetoftype_10;

DROP function pltest.PLPERLsetoftype_10 (  );
----Case 10 end----



----Case 12 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_12 (  ) RETURNS SETOF SMALLINT as $$  
			my $rv = spi_exec_query('select * from pltest.TABLESMALLINT;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_12() order by PLPERLsetoftype_12;

select * from pltest.PLPERLsetoftype_12() order by PLPERLsetoftype_12;

DROP function pltest.PLPERLsetoftype_12 (  );
----Case 12 end----


----Case 13 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_13 (  ) RETURNS SETOF MACADDR as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEMACADDR;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_13() order by PLPERLsetoftype_13;

select * from pltest.PLPERLsetoftype_13() order by PLPERLsetoftype_13;

DROP function pltest.PLPERLsetoftype_13 (  );
----Case 13 end----




----Case 15 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_15 (  ) RETURNS SETOF INTERVAL as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEINTERVAL;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_15() order by PLPERLsetoftype_15;

select * from pltest.PLPERLsetoftype_15() order by PLPERLsetoftype_15;

DROP function pltest.PLPERLsetoftype_15 (  );
----Case 15 end----


----Case 16 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_16 (  ) RETURNS SETOF NUMERIC as $$  
			my $rv = spi_exec_query('select * from pltest.TABLENUMERIC;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_16() order by PLPERLsetoftype_16;

select * from pltest.PLPERLsetoftype_16() order by PLPERLsetoftype_16;

DROP function pltest.PLPERLsetoftype_16 (  );
----Case 16 end----


----Case 17 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_17 (  ) RETURNS SETOF INT4 as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEINT4;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_17() order by PLPERLsetoftype_17;

select * from pltest.PLPERLsetoftype_17() order by PLPERLsetoftype_17;

DROP function pltest.PLPERLsetoftype_17 (  );
----Case 17 end----


----Case 18 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_18 (  ) RETURNS SETOF CHAR(4) as $$  
			my $rv = spi_exec_query('select * from pltest.TABLECHAR4;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_18() order by PLPERLsetoftype_18;

select * from pltest.PLPERLsetoftype_18() order by PLPERLsetoftype_18;

DROP function pltest.PLPERLsetoftype_18 (  );
----Case 18 end----


----Case 19 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_19 (  ) RETURNS SETOF TEXT as $$  
			my $rv = spi_exec_query('select * from pltest.TABLETEXT;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_19() order by PLPERLsetoftype_19;

select * from pltest.PLPERLsetoftype_19() order by PLPERLsetoftype_19;

DROP function pltest.PLPERLsetoftype_19 (  );
----Case 19 end----


----Case 20 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_20 (  ) RETURNS SETOF INET as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEINET;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_20()  order by PLPERLsetoftype_20;

select * from pltest.PLPERLsetoftype_20()  order by PLPERLsetoftype_20;

DROP function pltest.PLPERLsetoftype_20 (  );
----Case 20 end----

----Case 22 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
\echo '--ERROR:  PL/Perl functions cannot return type cstring'
CREATE or REPLACE function pltest.PLPERLsetoftype_22 (  ) RETURNS SETOF CSTRING as $$  
			my $rv = spi_exec_query('select * from pltest.TABLECSTRING;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;

----Case 22 end----



----Case 24 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
\echo '--ERROR:  cannot determine result data type'
CREATE or REPLACE function pltest.PLPERLsetoftype_24 (  ) RETURNS SETOF ANYELEMENT as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEANYELEMENT;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



----Case 24 end----


----Case 25 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_25 (  ) RETURNS SETOF CIDR as $$  
			my $rv = spi_exec_query('select * from pltest.TABLECIDR;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_25()  order by plperlsetoftype_25;

select * from pltest.PLPERLsetoftype_25()  order by plperlsetoftype_25;

DROP function pltest.PLPERLsetoftype_25 (  );
----Case 25 end----


----Case 26 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_26 (  ) RETURNS SETOF BYTEA as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEBYTEA;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_26() order by plperlsetoftype_26;

select * from pltest.PLPERLsetoftype_26() order by plperlsetoftype_26;

DROP function pltest.PLPERLsetoftype_26 (  );
----Case 26 end----


----Case 27 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_27 (  ) RETURNS SETOF MONEY as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEMONEY;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_27() order by plperlsetoftype_27;

select * from pltest.PLPERLsetoftype_27()  order by plperlsetoftype_27;

DROP function pltest.PLPERLsetoftype_27 (  );
----Case 27 end----


----Case 28 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_28 (  ) RETURNS SETOF TIMESTAMP as $$  
			my $rv = spi_exec_query('select * from pltest.TABLETIMESTAMP;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_28()  order by plperlsetoftype_28;

select * from pltest.PLPERLsetoftype_28() order by plperlsetoftype_28;

DROP function pltest.PLPERLsetoftype_28 (  );
----Case 28 end----


----Case 29 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_29 (  ) RETURNS SETOF BOOLEAN as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEBOOLEAN;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_29() order by plperlsetoftype_29;

select * from pltest.PLPERLsetoftype_29() order by plperlsetoftype_29;

DROP function pltest.PLPERLsetoftype_29 (  );
----Case 29 end----


----Case 30 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_30 (  ) RETURNS SETOF FLOAT8 as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEFLOAT8;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_30()  order by plperlsetoftype_30;

select * from pltest.PLPERLsetoftype_30()  order by plperlsetoftype_30;

DROP function pltest.PLPERLsetoftype_30 (  );
----Case 30 end----


----Case 31 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_31 (  ) RETURNS SETOF TIME as $$  
			my $rv = spi_exec_query('select * from pltest.TABLETIME;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_31()  order by plperlsetoftype_31;

select * from pltest.PLPERLsetoftype_31()  order by plperlsetoftype_31;

DROP function pltest.PLPERLsetoftype_31 (  );
----Case 31 end----



----Case 33 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_33 (  ) RETURNS SETOF TIMETZ as $$  
			my $rv = spi_exec_query('select * from pltest.TABLETIMETZ;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_33()  order by plperlsetoftype_33;

select * from pltest.PLPERLsetoftype_33()  order by plperlsetoftype_33;

DROP function pltest.PLPERLsetoftype_33 (  );
----Case 33 end----


----Case 34 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_34 (  ) RETURNS SETOF BIT(8) as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEBIT8;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;



select pltest.PLPERLsetoftype_34()  order by plperlsetoftype_34;

select * from pltest.PLPERLsetoftype_34()  order by plperlsetoftype_34;

DROP function pltest.PLPERLsetoftype_34 (  );
----Case 34 end----


----Case 35 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
\echo '--ERROR:  syntax error at or near "ANY"'
CREATE or REPLACE function pltest.PLPERLsetoftype_35 (  ) RETURNS SETOF ANY as $$  
			my $rv = spi_exec_query('select * from pltest.TABLEANY;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
			my $row = $rv->{rows}[$rn] -> {key};
				return_next($row);
			}
			return undef;
$$ language PLPERL;


----Case 35 end----

----Case 1 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_1 (  ) RETURNS SETOF BOX as $$
                        my $rv = spi_exec_query('select * from pltest.TABLEBOX;');
                        my $status = $rv->{status};
                        my $nrows = $rv->{processed};
                        foreach my $rn (0 .. $nrows - 1) {
                        my $row = $rv->{rows}[$rn] -> {key};
                                return_next($row);
                        }
                        return undef;
$$ language PLPERL;

DROP function pltest.PLPERLsetoftype_1 (  );
----Case 1 end----

----Case 11 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_11 (  ) RETURNS SETOF PATH as $$
                        my $rv = spi_exec_query('select * from pltest.TABLEPATH;');
                        my $status = $rv->{status};
                        my $nrows = $rv->{processed};
                        foreach my $rn (0 .. $nrows - 1) {
                        my $row = $rv->{rows}[$rn] -> {key};
                                return_next($row);
                        }
                        return undef;
$$ language PLPERL;


DROP function pltest.PLPERLsetoftype_11 (  );
----Case 11 end----

----Case 14 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_14 (  ) RETURNS SETOF POINT as $$
                        my $rv = spi_exec_query('select * from pltest.TABLEPOINT;');
                        my $status = $rv->{status};
                        my $nrows = $rv->{processed};
                        foreach my $rn (0 .. $nrows - 1) {
                        my $row = $rv->{rows}[$rn] -> {key};
                                return_next($row);
                        }
                        return undef;
$$ language PLPERL;

DROP function pltest.PLPERLsetoftype_14 (  );
----Case 14 end----


----Case 21 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_21 (  ) RETURNS SETOF POLYGON as $$
                        my $rv = spi_exec_query('select * from pltest.TABLEPOLYGON;');
                        my $status = $rv->{status};
                        my $nrows = $rv->{processed};
                        foreach my $rn (0 .. $nrows - 1) {
                        my $row = $rv->{rows}[$rn] -> {key};
                                return_next($row);
                        }
                        return undef;
$$ language PLPERL;


DROP function pltest.PLPERLsetoftype_21 (  );
----Case 21 end----



----Case 23 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_23 (  ) RETURNS SETOF LSEG as $$
                        my $rv = spi_exec_query('select * from pltest.TABLELSEG;');
                        my $status = $rv->{status};
                        my $nrows = $rv->{processed};
                        foreach my $rn (0 .. $nrows - 1) {
                        my $row = $rv->{rows}[$rn] -> {key};
                                return_next($row);
                        }
                        return undef;
$$ language PLPERL;


DROP function pltest.PLPERLsetoftype_23 (  );
----Case 23 end----


----Case 32 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN SETOF b/c.
CREATE or REPLACE function pltest.PLPERLsetoftype_32 (  ) RETURNS SETOF CIRCLE as $$
                        my $rv = spi_exec_query('select * from pltest.TABLECIRCLE;');
                        my $status = $rv->{status};
                        my $nrows = $rv->{processed};
                        foreach my $rn (0 .. $nrows - 1) {
                        my $row = $rv->{rows}[$rn] -> {key};
                                return_next($row);
                        }
                        return undef;
$$ language PLPERL;



DROP function pltest.PLPERLsetoftype_32 (  );
----Case 32 end----

----Case 77 begin----
-- CREATE OR REPLACE FUNCTION functionname() RETURN VOID.
CREATE or REPLACE function pltest.PLPERLreturnvoid_77 (  ) RETURNS VOID as $$   
	my ($emp) = @_;
	my $result = '';
	for my $key ( keys %$emp ) {
		$result = $result . ',' . $emp->{$key}
	}

	return undef;
$$ language PLPERL;



select pltest.PLPERLreturnvoid_77();

select * from pltest.PLPERLreturnvoid_77();

DROP function pltest.PLPERLreturnvoid_77 (  );
----Case 77 end----


