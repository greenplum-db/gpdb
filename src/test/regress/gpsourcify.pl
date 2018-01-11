#!/usr/bin/env perl
#
# Portions Copyright (c) 2007-2012 GreenPlum.  All rights reserved.
# Portions Copyright (c) 2012-Present Pivotal Software, Inc.
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

=head1 NAME

B<gpsourcify.pl> - GreenPlum reverse string substitution 

=head1 SYNOPSIS

B<gpsourcify.pl> filename

Options:

    -help            brief help message
    -man             full documentation
    -token_file      (optional) file with list of tokens and 
                     replacement values (gptokencheck by default)
    -dlsuffix        if flag is set, replace DLSUFFIX (false by default)

=head1 OPTIONS

=over 8

=item B<-help>

    Print a brief help message and exits.

=item B<-man>
    
    Prints the manual page and exits.

=item B<-token_file> (optional: gptokencheck by default)

    File which contains a list of token/value pairs.  If a filename is
    not specified, gpsourcify looks for gptokencheck.out under
    $PWD/results. 

=item B<-dlsuffix> (false by default)

    Because the DLSUFFIX is so small (".so" on unix), there is the
    potential for erroneous replacement.  Only replace if -dlsuffix is
    specified.  Do not replace by default, or if -nodlsuffix is specified.

=back

=head1 DESCRIPTION

gpsourcify reads a pg_regress ".out" file as input and rewrites it to
stdout, replacing certain local or environmental values with portable
tokens.  The output is a candidate for a pg_regress ".source" file.
gpsourcify is the complement to the gpstringsubs and pg_regress
"convert_sourcefiles_in" functions.  

=head1 CAVEATS/LIMITATIONS

In general, gpsourcify always replaces the longest string values
first, so you don't have to worry about some type of string
replacement collision.  However, if abs_srcdir and abs_builddir are
identical values (like on unix), gpsourcify uses the heuristic that
source directory references are less common, and usually followed by
"/data".  This may not do what you want.

=head1 AUTHORS

Jeffrey I Cohen

Portions Copyright (c) 2007-2012 GreenPlum.  All rights reserved.
Portions Copyright (c) 2012-Present Pivotal Software, Inc.

Address bug reports and comments to: bugs@greenplum.org


=cut

    my $glob_id = "";

my $glob_token_file;
my $glob_dlsuffix;

BEGIN {
    my $man  = 0;
    my $help = 0;
    my $tokenf = '';
    my $dlsuf = ''; # negatable, ie -dlsuffix or -nodlsuffix

    GetOptions(
               'help|?' => \$help, man => \$man,
               'dlsuffix!' => \$dlsuf,
               'token_file:s' => \$tokenf
               )
        or pod2usage(2);


    pod2usage(-msg => $glob_id, -exitstatus => 1) if $help;
    pod2usage(-msg => $glob_id, -exitstatus => 0, -verbose => 2) if $man;

    $glob_dlsuffix = $dlsuf;
    $glob_token_file = length($tokenf) ? $tokenf : "results/gptokencheck.out";

    unless (-f $glob_token_file)
    {
		if (length($tokenf))
		{
			warn("ERROR: Cannot find specified token file: $glob_token_file");
		}
		else
		{
			warn("ERROR: Need to run test gptokencheck to generate token file:\n\n\tmake installcheck-good TT=gptokencheck\n\n");
		}

        $glob_id = "ERROR - invalid file: $glob_token_file\n";
        pod2usage(-msg => $glob_id, -exitstatus => 1, -verbose => 0);
    }
#    print "loading...\n";                                                     
}


sub LoadTokens
{
    my $fil = shift;

    return undef
        unless (defined($fil) && (-f $fil));

    my $ini = `grep 'is_transformed_to' $fil`;

    $ini =~ s/^\-\-\s+//gm;

#    print $ini;

    my @foo = split(/\n/, $ini);

    my $bigh = {};

    for my $kv (@foo)
    {
        my @baz = split(/\s*\#is\_transformed\_to\#\s*/, $kv, 2);
        
        if (2 == scalar(@baz))
        {
            $bigh->{$baz[0]} = quotemeta($baz[1]);
        }

    }

#    print Data::Dumper->Dump([$bigh]);

    return $bigh;
}


if (1)
{
    my $bigh = LoadTokens($glob_token_file);
    exit(0) unless (defined($bigh));

    # make an array of the token names (keys), sorted descending by
    # the length of of the replacement value.
    my @sortlen;
    @sortlen = 
        sort {length($bigh->{$b}) <=> length($bigh->{$a})} keys %{$bigh};

    my $src_eq_build = 0;
        
    if (exists($bigh->{abs_srcdir})
        && exists($bigh->{abs_builddir}))
    {
        # see if build directory and source directory are the same...
        $src_eq_build = ($bigh->{abs_srcdir} eq $bigh->{abs_builddir});

    }

#    print Data::Dumper->Dump(\@sortlen);

    while (<>)
    {
        my $ini = $_;

        for my $kk (@sortlen)
        {
            my $vv = $bigh->{$kk};
            my $k2 = '@' . $kk . '@';

            if ($kk =~ m/DLSUFFIX/ && !$glob_dlsuffix)
            {
                # ignore if -nodlsuffix
                next;
            }

            if (($kk =~ m/abs_srcdir/) && $src_eq_build)
            {
                my $v2 = $vv . "/data";
                my $k3 = $k2 . "/data";
                # replace the srcdir only if followed by "/data"
                $ini =~ s/$v2/$k3/g;  
				# or "include/catalog"
                my $v3 = $vv . "/../../include/catalog";
                my $k4 = $k2 . "/../../include/catalog";
                $ini =~ s/$v3/$k4/g;  
            }
            elsif (($kk =~ m/abs_builddir/) && $src_eq_build)
            {
				my $regex1 =
					"/data" . "|" .
					"/../../include/catalog";

                # replace buildir if not followed by "/data" 
				# or "include/catalog"
                # (negative look-ahead)
                $ini =~ 
					s/$vv(?!($regex1))/$k2/g;
            }
            else
            {
                $ini =~ s/$vv/$k2/g;
            }

        }

        print $ini;

    }
   
    exit(0);
}
