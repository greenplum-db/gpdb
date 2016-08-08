-- Test: plperl 51
CREATE OR REPLACE FUNCTION perl_spi_func2() RETURNS INTEGER AS $$
							  my $x = spi_query("select 1 as a union select 2 as a");
							  spi_cursor_close( $x);
							  return 0;
							  $$ LANGUAGE plperl;
							  
SELECT * from perl_spi_func2();
							  

