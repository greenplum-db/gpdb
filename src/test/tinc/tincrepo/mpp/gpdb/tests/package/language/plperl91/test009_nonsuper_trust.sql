\echo '-- check that restricted operations are rejected in a plperl'
CREATE or REPLACE function pltest.restrict_op () RETURNS VOID AS $$
     use Safe;
     my $safeobj = new Safe;
     return 'okay';
$$ LANGUAGE plperl;

\echo '-- check that restricted operations are rejected in a plperl'
CREATE or REPLACE function pltest.restrict_op () RETURNS VOID AS $$ 
	system("/nonesuch");
$$ LANGUAGE plperl;  

\echo '-- check that restricted operations are rejected in a plperl'
CREATE or REPLACE function pltest.restrict_op () RETURNS VOID AS $$ 
   qx("/nonesuch");
$$ LANGUAGE plperl;

\echo '-- check that restricted operations are rejected in a plperl'
CREATE or REPLACE function pltest.restrict_op () RETURNS VOID AS $$ 
   open my $fh, "</nonesuch";
$$ LANGUAGE plperl;

\echo '-- check that eval is allowed and eval'd restricted ops are caught'
CREATE or REPLACE function pltest.restrict_op () RETURNS VOID AS $$ 
    eval q{chdir '.'}; warn "Caught: $@";
$$ LANGUAGE plperl;
SELECT pltest.restrict_op ();
DROP FUNCTION pltest.restrict_op ();
\echo '-- check that compiling do (dofile opcode) is allowed'
\echo '-- but that executing it for a file not already loaded (via require) dies'
CREATE or REPLACE function pltest.restrict_op () RETURNS VOID AS $$ 
   warn do "/dev/null";
$$ LANGUAGE plperl;
SELECT pltest.restrict_op ();
DROP FUNCTION pltest.restrict_op ();


\echo '-- check that we can't "use" a module that's not been loaded already'
\echo '- compile-time error: "Unable to load blib.pm into plperl"'
CREATE or REPLACE function pltest.restrict_op () RETURNS VOID AS $$ 
   use blib;
$$ LANGUAGE plperl;
/*
\echo '-- check that we can "use" a module that has already been loaded'
\echo '-- runtime error: "Can't use string ("foo") as a SCALAR ref while "strict refs" in use'
CREATE or REPLACE function pltest.restrict_op () RETURNS VOID AS $$ 
   use strict; 
   my $name = "foo"; 
   my $ref = $name; 
$$ LANGUAGE plperl;

*/
\echo '-- check that we can "use warnings" (in this case to turn a warn into an error)'
\echo '-- yields "ERROR:  Useless use of sort in scalar context."'
CREATE or REPLACE function pltest.restrict_op () RETURNS VOID AS
$$ 
   use warnings FATAL => qw(void) ; 
   my @y; 
   my $x = sort @y; 
   1; 
$$ LANGUAGE plperl;

