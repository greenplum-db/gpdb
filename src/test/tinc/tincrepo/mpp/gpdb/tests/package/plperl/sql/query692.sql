-- Test: plperl 55
CREATE OR REPLACE FUNCTION perl_spi_prepared(INTEGER) RETURNS INTEGER AS $$
							  my $x = spi_prepare('select $1 AS a', 'INT4');
							  my $q = spi_exec_prepared( $x, $_[0] + 1);
							  spi_freeplan($x);
							  return $q->{rows}->[0]->{a};
							  $$ LANGUAGE plperl;
							  
SELECT * from perl_spi_prepared(42);
							  

