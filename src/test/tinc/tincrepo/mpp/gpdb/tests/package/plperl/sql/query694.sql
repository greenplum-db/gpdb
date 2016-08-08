-- Test: plperl 57
CREATE OR REPLACE FUNCTION perl_elog(TEXT) RETURNS VOID AS $$
							  my $msg = shift;
							  elog(NOTICE,$msg);
							  $$ LANGUAGE plperl;
							  
SELECT perl_elog('explicit elog');
							  

