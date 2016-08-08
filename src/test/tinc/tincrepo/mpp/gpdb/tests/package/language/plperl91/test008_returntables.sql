----Case 0 begin----
-- 
-- 			CREATE OR REPLACE FUNCTION functionname() RETURN TABLE(...);
-- 			CREATE OR REPLACE FUNCTION functionname() RETURN SETOF table;
-- 			CREATE OR REPLACE FUNCTION functionname() RETURN SETOF RECORD;
-- 			CREATE OR REPLACE FUNCTION functionname(OUT column1 b, OUT column2 b, ...) RETURN SETOF RECORD.
-- 		
CREATE or REPLACE function pltest.PLPERLsetoftuple_0 (  ) RETURNS TABLE( id1 SMALLINT,id2 BIT(8),id3 BIT VARYING(10),id4 BOOLEAN,id5 BYTEA ) as $$  
			my $rv = spi_exec_query('select * from pltest.tupleCOMPOSITE_BIT ORDER BY ID1;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
				my $row = $rv->{rows}[$rn];
				return_next($row);
			}
			return undef;
$$ language PLPERL;

-- BUG select pltest.PLPERLsetoftuple_0() from pltest.tableTEXT;

select pltest.PLPERLsetoftuple_0();

select (pltest.PLPERLsetoftuple_0()).* ORDER BY ID1;

select * from pltest.PLPERLsetoftuple_0() ORDER BY ID1;

DROP function pltest.PLPERLsetoftuple_0 (  );
----Case 0 end----


----Case 1 begin----
-- 
-- 			CREATE OR REPLACE FUNCTION functionname() RETURN TABLE(...);
-- 			CREATE OR REPLACE FUNCTION functionname() RETURN SETOF table;
-- 			CREATE OR REPLACE FUNCTION functionname() RETURN SETOF RECORD;
-- 			CREATE OR REPLACE FUNCTION functionname(OUT column1 b, OUT column2 b, ...) RETURN SETOF RECORD.
-- 		
CREATE or REPLACE function pltest.PLPERLsetoftuple_1 (  ) RETURNS SETOF pltest.tupleCOMPOSITE_BIT as $$  
			my $rv = spi_exec_query('select * from pltest.tupleCOMPOSITE_BIT ORDER BY ID1;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
				my $row = $rv->{rows}[$rn];
				return_next($row);
			}
			return undef;
$$ language PLPERL;

-- Limitaion select pltest.PLPERLsetoftuple_1() from pltest.tableTEXT;

select pltest.PLPERLsetoftuple_1();

select (pltest.PLPERLsetoftuple_1()).* ORDER BY ID1;

select * from pltest.PLPERLsetoftuple_1() ORDER BY ID1;

DROP function pltest.PLPERLsetoftuple_1 (  );
----Case 1 end----


----Case 2 begin----
-- 
-- 			CREATE OR REPLACE FUNCTION functionname() RETURN TABLE(...);
-- 			CREATE OR REPLACE FUNCTION functionname() RETURN SETOF table;
-- 			CREATE OR REPLACE FUNCTION functionname() RETURN SETOF RECORD;
-- 			CREATE OR REPLACE FUNCTION functionname(OUT column1 b, OUT column2 b, ...) RETURN SETOF RECORD.
-- 		
CREATE or REPLACE function pltest.PLPERLsetoftuple_2 (  ) RETURNS SETOF RECORD as $$  
			my $rv = spi_exec_query('select * from pltest.tupleCOMPOSITE_BIT ORDER BY ID1;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
				my $row = $rv->{rows}[$rn];
				return_next($row);
			}
			return undef;
$$ language PLPERL;

-- BUG select pltest.PLPERLsetoftuple_2() from pltest.tableTEXT;

select * from pltest.PLPERLsetoftuple_2() f(id1 SMALLINT,id2 BIT(8),id3 BIT VARYING(10),id4 BOOLEAN,id5 BYTEA);

DROP function pltest.PLPERLsetoftuple_2 (  );
----Case 2 end----


----Case 3 begin----
-- 
-- 			CREATE OR REPLACE FUNCTION functionname() RETURN TABLE(...);
-- 			CREATE OR REPLACE FUNCTION functionname() RETURN SETOF table;
-- 			CREATE OR REPLACE FUNCTION functionname() RETURN SETOF RECORD;
-- 			CREATE OR REPLACE FUNCTION functionname(OUT column1 b, OUT column2 b, ...) RETURN SETOF RECORD.
-- 		
CREATE or REPLACE function pltest.PLPERLsetoftuple_3 ( OUT id1 SMALLINT , OUT id2 BIT(8) , OUT id3 BIT VARYING(10) , OUT id4 BOOLEAN , OUT id5 BYTEA ) RETURNS SETOF RECORD as $$  
			my $rv = spi_exec_query('select * from pltest.tupleCOMPOSITE_BIT ORDER BY ID1;');
			my $status = $rv->{status};
			my $nrows = $rv->{processed};
			foreach my $rn (0 .. $nrows - 1) {
				my $row = $rv->{rows}[$rn];
				return_next($row);
			}
			return undef;
$$ language PLPERL;

-- BUG select pltest.PLPERLsetoftuple_3() from pltest.tableTEXT;

select pltest.PLPERLsetoftuple_3();

select (pltest.PLPERLsetoftuple_3()).* ORDER BY ID1;

select * from pltest.PLPERLsetoftuple_3() ORDER BY ID1;

DROP function pltest.PLPERLsetoftuple_3 ( OUT id1 SMALLINT , OUT id2 BIT(8) , OUT id3 BIT VARYING(10) , OUT id4 BOOLEAN , OUT id5 BYTEA );
----Case 3 end----

