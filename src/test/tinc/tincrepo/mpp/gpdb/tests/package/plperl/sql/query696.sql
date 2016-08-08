-- Test: plperl 62
CREATE OR REPLACE FUNCTION setme(key TEXT, val TEXT) RETURNS VOID AS $$
							  my $key = shift;
							  my $val = shift;
							  $_SHARED{$key}= $val;
							  $$ LANGUAGE plperl;
							  
CREATE OR REPLACE FUNCTION getme(key TEXT) RETURNS TEXT AS $$
							  my $key = shift;
							  return $_SHARED{$key};
							  $$ LANGUAGE plperl;
							  
SELECT setme('ourkey','ourval');
							  

