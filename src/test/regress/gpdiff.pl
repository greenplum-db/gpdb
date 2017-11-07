#!/usr/bin/env perl
#
# Portions Copyright (c) 2007, 2008, 2009 GreenPlum.  All rights reserved.
# Portions Copyright (c) 2012-Present Pivotal Software, Inc.
#
# Author: Jeffrey I Cohen
#
# Pod::Usage is loaded lazily when needed, if the --help or other such option
# is actually used. Loading the module takes some time, which adds up when
# running hundreds of regression tests, and gpdiff.pl calls this script twice
# for every test. See lazy_pod2usage().
#use Pod::Usage;

use strict;
use warnings;
use File::Spec;
use Getopt::Long qw(GetOptions);
Getopt::Long::Configure qw(pass_through);

# Load atmsort module from the same dir as this script
use FindBin;
use lib "$FindBin::Bin";
use atmsort;
use GPTest qw(print_version);

=head1 NAME

B<gpdiff.pl> - GreenPlum diff

=head1 SYNOPSIS

B<gpdiff.pl> [options] logfile [logfile...]

Options:

Normally, gpdiff takes the standard "diff" options and passes them
directly to the diff program.  Try `diff --help' for more information
on the standard options.  The following options are specific to gpdiff:

    -help                 brief help message
    -man                  full documentation
    -version              print gpdiff version and underlying diff version
    -verbose              print verbose info
    -gpd_ignore_headers   ignore header lines in query output
    -gpd_ignore_plans     ignore explain plan content in input files
    -gpd_init <file>      load initialization file

=head1 OPTIONS

=over 8

=item B<-help>

    Print a brief help message and exits.

=item B<-man>

    Prints the manual page and exits.

=item B<-version>

    Prints the gpdiff version and underlying diff version

=item B<-verbose>

    Prints verbose information.

=item B<-gpd_ignore_headers>

gpdiff/atmsort expect PostgreSQL "psql-style" output for SELECT
statements, with a two line header composed of the column names,
separated by vertical bars (|), and a "separator" line of dashes and
pluses beneath, followed by the row output.  The psql utility performs
some formatting to adjust the column widths to match the size of the
row output.  Setting this parameter causes gpdiff to ignore any
differences in the column naming and format widths globally.

=item B<-gpd_ignore_plans>

Specify this option to ignore any explain plan diffs between the
input files. This will completely ignore any plan content in
the input files thus masking differences in plans between the input files.

=item B<-gpd_init> <file>

Specify an initialization file containing a series of directives
(mainly for match_subs) that get applied to the input files.  To
specify multiple initialization files, use multiple gpd_init arguments, eg:

  -gpd_init file1 -gpd_init file2

=back

=head1 DESCRIPTION

gpdiff compares files using diff after processing them with atmsort.pm.
This comparison is designed to ignore certain Greenplum-specific
informational messages, as well as handle the cases where query output
order may differ for a multi-segment Greenplum database versus a
single PostgreSQL instance.  Type "atmsort.pl --man" for more details.
gpdiff is invoked by pg_regress as part of "make installcheck-world".
In this case the diff options are something like:

 "-w -I NOTICE: -I HINT: -I CONTEXT: -I GP_IGNORE:".

Like diff, gpdiff can compare two files, a file and directory, a
directory and file, and two directories.  However, when gpdiff compares
two directories, it only returns the exit status of the diff
comparison of the final two files.

=head1 BUGS

While the exit status is set correctly for most cases,
STDERR messages from diff are not displayed.

Also, atmsort cannot handle "unsorted" SELECT queries where the output
has strings with embedded newlines or pipe ("|") characters due to
limitations with the parser in the "tablelizer" function.  Queries
with these characteristics must have an ORDER BY clause to avoid
potential erroneous comparison.


=head1 AUTHORS

Jeffrey I Cohen

Portions Copyright (c) 2007, 2008, 2009 GreenPlum.  All rights reserved.
Portions Copyright (c) 2012-Present Pivotal Software, Inc.

Address bug reports and comments to: bugs@greenplum.org


=cut

# Calls pod2usage, but loads the module first.
sub lazy_pod2usage
{
    require Pod::Usage;
    Pod::Usage::pod2usage(@_);
}

my %glob_atmsort_args;

my $glob_ignore_plans;
my $glob_init_file = [];

sub gpdiff_files
{
    my ($f1, $f2, $d2d) = @_;
    my $need_equiv = 0;
    my @tmpfils;
    my $newf1;
    my $newf2;

    if (defined($d2d) && exists($d2d->{equiv}))
    {
        # assume f1 and f2 are the same...
        atmsort::atmsort_init(%glob_atmsort_args, DO_EQUIV => 'compare');
        $newf1 = atmsort::run($f1);

        atmsort::atmsort_init(%glob_atmsort_args, DO_EQUIV => 'make');
        $newf2 = atmsort::run($f2);
    }
    else
    {
        atmsort::atmsort_init(%glob_atmsort_args);
        $newf1 = atmsort::run($f1);
        $newf2 = atmsort::run($f2);
    }

    my $args = join(' ', @ARGV, $newf1, $newf2);

#   print "args: $args\n";

    my $outi =`diff $args`;

    my $stat = $? >> 8; # diff status

    unless (defined($d2d) && exists($d2d->{equiv}))
    {
        # check if the file contains any "start_equiv" directives, unless
        # already doing equiv check
        open(FILE, $f1);
        $need_equiv = (grep{/start_equiv/} <FILE>);
        close FILE;

        if ($need_equiv)
        {
            $d2d = {} unless (defined($d2d));
            $d2d->{dir} = 1;
        }
    }

    # prefix the diff output with the files names for a "directory to
    # directory" diff
    if (defined($d2d) && length($outi))
    {
        if (exists($d2d->{equiv}))
        {
            $outi = "diff $f1 $f2" . ".equiv\n" . $outi;
        }
        else
        {
            $outi = "diff $f1 $f2\n" . $outi;
        }
    }

    # replace temp file name references with actual file names
    $outi =~ s/$newf1/$f1/gm;
    $outi =~ s/$newf2/$f2/gm;

    print $outi;

#my $stat = WEXITVALUE($?); # diff status

    unlink $newf1;
    unlink $newf2;

    if ($need_equiv)
    {
        my $new_d2d = {};
        $new_d2d->{equiv} = 1;

        # call recursively if need to perform equiv comparison.

        my $stat1 = gpdiff_files($f1, $f1, $new_d2d);
        my $stat2 = gpdiff_files($f2, $f2, $new_d2d);

        $stat = $stat1 if ($stat1);
        $stat = $stat2 if ($stat2);
    }

    return ($stat);
}

sub filefunc
{
    my ($f1, $f2, $d2d) = @_;

    if ((-f $f1) && (-f $f2))
    {
        return (gpdiff_files($f1, $f2, $d2d));
    }

    # if f1 is a directory, do the filefunc of every file in that directory
    if ((-d $f1) && (-d $f2))
    {
        my $dir = $f1;
        my ($dir_h, $stat);

        if (opendir($dir_h, $dir))
        {
            my $fnam;
            while ($fnam = readdir($dir_h))
            {
                # ignore ., ..
                next if ($fnam eq '.' || $fnam eq '..');

                my $absname = File::Spec->rel2abs(
                                 File::Spec->catfile($dir, $fnam));

                # specify that is a directory comparison
                $d2d = {} unless (defined($d2d));
                $d2d->{dir} = 1;
                $stat = filefunc($absname, $f2, $d2d);
            }
            closedir $dir_h;
        }
        return $stat;
    }

    # if f2 is a directory, find the corresponding file in that directory
    if ((-f $f1) && (-d $f2))
    {
        my $stat;
        my @foo = File::Spec->splitpath($f1);

        return 0 unless (scalar(@foo));
        my $basenam = $foo[-1];

        my $fnam = File::Spec->rel2abs(File::Spec->catfile( $f2, $basenam));

        $stat = filefunc($f1, $fnam, $d2d);

        return $stat;
    }

    # find f2 in dir f1
    if ((-f $f2) && (-d $f1))
    {
        my $stat;
        my @foo = File::Spec->splitpath($f2);

        return 0 unless (scalar(@foo));
        my $basenam = $foo[-1];

        my $fnam = File::Spec->rel2abs( File::Spec->catfile( $f1, $basenam));

        $stat = filefunc($fnam, $f2, $d2d);

        return $stat;
    }

    return 0;
}

if (1)
{
    my $pmsg = "";

    GetOptions(
        "man" => sub { lazy_pod2usage(-msg => $pmsg, -exitstatus => 0, -verbose => 2) },
        "help" => sub { lazy_pod2usage(-msg => $pmsg, -exitstatus => 1) },
        "version|v" => \&print_version ,
        "verbose|Verbose" => \$glob_atmsort_args{VERBOSE},
        "gpd_ignore_headers|gp_ignore_headers" => \$glob_atmsort_args{IGNORE_HEADERS},
        "gpd_ignore_plans|gp_ignore_plans" => \$glob_atmsort_args{IGNORE_PLANS},
        "gpd_init|gp_init_file=s" => \@{$glob_atmsort_args{INIT_FILES}}
    );

    lazy_pod2usage(-msg => $pmsg, -exitstatus => 1) unless (scalar(@ARGV) >= 2);

    my $f2 = pop @ARGV;
    my $f1 = pop @ARGV;

    for my $fname ($f1, $f2)
    {
        unless (-e $fname)
        {
            print STDERR "gpdiff: $fname: No such file or directory\n";
        }
    }
    exit(2) unless ((-e $f1) && (-e $f2));

    exit(filefunc($f1, $f2));
}
