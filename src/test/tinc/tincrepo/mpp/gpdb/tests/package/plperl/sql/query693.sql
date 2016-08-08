-- Test: plperl 56
CREATE OR REPLACE FUNCTION perl_spi_prepared_set(INTEGER, INTEGER) RETURNS SETOF
							  INTEGER AS $$
							  my $x = spi_prepare('SELECT $1 AS a union select $2 as a', 'INT4', 'INT4');
							  my $q = spi_query_prepared( $x, 1+$_[0], 2+$_[1]);
							  while (defined (my $y = spi_fetchrow($q))) {
								  return_next $y->{a};
							  }
							  spi_freeplan($x);
							  return;
							  $$ LANGUAGE plperl;
							  
SELECT * from perl_spi_prepared_set(1,2);
							  

