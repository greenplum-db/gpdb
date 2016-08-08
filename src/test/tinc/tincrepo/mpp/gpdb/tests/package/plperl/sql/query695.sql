-- Test: plperl 58
CREATE OR REPLACE FUNCTION perl_warn(TEXT) RETURNS VOID AS $$
							  my $msg = shift;
							  warn($msg);
							  $$ LANGUAGE plperl;
							  
SELECT perl_warn('implicit elog via warn');
							  

