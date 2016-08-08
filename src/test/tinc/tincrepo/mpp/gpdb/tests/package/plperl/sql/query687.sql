-- Test: plperl 50
CREATE OR REPLACE FUNCTION perl_spi_func() RETURNS SETOF INTEGER AS $$
							  my $x = spi_query("select 1 as a union select 2 as a");
							  while (defined (my $y = spi_fetchrow($x))) {
							      return_next($y->{a});
							  }
							  return;
							  $$ LANGUAGE plperl;
							  
SELECT * from perl_spi_func();
							  

