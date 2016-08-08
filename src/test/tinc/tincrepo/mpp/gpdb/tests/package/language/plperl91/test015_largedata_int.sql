CREATE or REPLACE function pltest.lineitem_orderkey ( ) RETURNS SETOF bigint as $$
my $rv = spi_exec_query('select l_orderkey from pltest.lineitem;');
my $status = $rv->{status};
my $nrows = $rv->{processed};
foreach my $rn (0 .. $nrows - 1) {
my $row = $rv->{rows}[$rn] -> {l_orderkey};
return_next($row);
}
return undef;
$$ language PLPERL;

SELECT count(*) FROM pltest.lineitem_orderkey();


DROP FUNCTION pltest.lineitem_orderkey();

-------------------

CREATE TABLE pltest.table10000 AS SELECT * from generate_series(1,10000) DISTRIBUTED RANDOMLY;
CREATE OR REPLACE FUNCTION pltest.setof_int() RETURNS SETOF INTEGER AS $$ 
   my $range = 20000; 
   foreach (1..$range) 
   { 
      return_next(1); 
   }
   return undef; 
$$ LANGUAGE plperl; 

CREATE TABLE pltest.setofIntRes AS SELECT pltest.setof_int() from pltest.table10000 DISTRIBUTED RANDOMLY;
DROP TABLE pltest.setofIntRes;
DROP FUNCTION pltest.setof_int();
DROP TABLE pltest.table10000;


