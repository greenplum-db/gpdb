\echo '-- start_ignore'
CREATE OR REPLACE FUNCTION pltest.super_untrustfunc() RETURNS integer AS $$
     my $tmpfile = "TMPFILENAME";
     open my $fh, '>', $tmpfile or elog(ERROR, qq{could not open the file "$tmpfile": $!});
     print $fh "Testing superuser can execute unsafe function\n";
     close $fh or elog(ERROR, qq{could not close the file "$tmpfile": $!});
     return 1;
$$ LANGUAGE plperlu;
\echo '-- end_ignore'

SELECT  pltest.super_untrustfunc();

DROP FUNCTION pltest.super_untrustfunc();
