#!/usr/bin/env perl
#
# Portions Copyright (c) 2007-2010 GreenPlum.  All rights reserved.
# Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
#
# Author: Jeffrey I Cohen
#
use Pod::Usage;
use Getopt::Long;
use strict;
use warnings;
use POSIX;
use File::Spec;
use Env;
use Data::Dumper;

use FindBin;
use lib "$FindBin::Bin";
use GPTest qw(print_version);

=head1 NAME

B<gpstringsubs.pl> - GreenPlum string substitution 

=head1 SYNOPSIS

B<gpstringsubs.pl> filename

Options:

    -help            brief help message
    -man             full documentation
    -connect         psql connect parameters

=head1 OPTIONS

=over 8

=item B<-help>

    Print a brief help message and exits.

=item B<-man>
    
    Prints the manual page and exits.

=item B<-connect>

    psql connect string, e.g:

    -connect '-p 11000 template1'

    If the connect string is not defined gpstringsubs uses 
    PGPORT and template1.  If the PGPORT is undefined then 
    the port number defaults to 11000.
        
=back

=head1 DESCRIPTION

gpstringsubs find tokens in an input file and replaces them (in place)
with specified or environmental values, supplementing the pg_regress
"convert_sourcefiles_in" function.

The tokens are:

=over 8

=item gpwhich_(executable)

 Find the full path for the executable and substitute.  For example, 
 @gpwhich_gpfdist@ is replaced with the full path for "gpfdist".

=back


=head1 AUTHORS

Jeffrey I Cohen

Portions Copyright (c) 2007-2010 GreenPlum.  All rights reserved.
Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.

Address bug reports and comments to: bugs@greenplum.org


=cut

    my $glob_id = "";
    my $glob_connect;


BEGIN {
    my $man  = 0;
    my $help = 0;
    my $conn;

    GetOptions(
               'help|?' => \$help, man => \$man,
               "connect=s" => \$conn,
               'version|v' => \&print_version
               )
        or pod2usage(2);


    pod2usage(-msg => $glob_id, -exitstatus => 1) if $help;
    pod2usage(-msg => $glob_id, -exitstatus => 0, -verbose => 2) if $man;

    $glob_connect = $conn;

    my $porti = $ENV{PGPORT} if (exists($ENV{PGPORT}));

    $porti = 5432                 # 11000
        unless (defined($porti));

    $glob_connect = "-p $porti -d template1"
        unless (defined($glob_connect));

#    print "loading...\n" ;      

}

if (1)
{
    exit(-1)
        unless (scalar(@ARGV));

    my $filnam = $ARGV[0];

    unless (-f $filnam)
    {
        die "invalid filename: $filnam";
        exit(-1);
    }

	# Get the locale that the database was initialized with- it should be safe to
	# use this locale for any tests that need to import a locale installed on the
	# system.
	my $psql_str = "psql ";

	$psql_str .= $glob_connect
		if (defined($glob_connect));

	$psql_str .= " -t -A -c 'show LC_CTYPE'";

	my $syslocale = `$psql_str`;
	my $syslocaleexp = '\\@gp_syslocale\\@';

	chomp $syslocale;

    my $gpwhich_all  = `grep gpwhich_ $filnam`;
	my $curdir = `pwd`;
	chomp $curdir;

#    print "$filnam\n";
    system "perl -i -ple \' s/$syslocaleexp/$syslocale/gm; \' $filnam\n";

    # replace all "which" expressions with binary
    if (defined($gpwhich_all) && length($gpwhich_all))
    {
        my @foo = split(/\@/, $gpwhich_all);

#        print Data::Dumper->Dump(\@foo);

        my @gpwhich_list;

        if (scalar(@foo))
        {
            for my $gpw (@foo)
            {
                my @exe_thing;

                next
                    if (($gpw =~ m/is\_transformed\_to/));

                next
                    unless (($gpw =~ m/gpwhich\_\w/));

                @exe_thing = ($gpw =~ m/gpwhich\_(.*)/);

                next
                    unless (scalar(@exe_thing));

                my $binloc = `which $exe_thing[0]`;

                next
                    unless (length($binloc));

                chomp $binloc;
                $binloc = quotemeta($binloc);

                my $whichexp = '\\@gpwhich_' . $exe_thing[0] . '\\@';

                system "perl -i -ple \' s/$whichexp/$binloc/gm;\' $filnam\n";

            } # end for
        } # end if scalar foo
    } # end if which all

    exit(0);
}
