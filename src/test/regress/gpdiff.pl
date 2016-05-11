#!/usr/bin/env perl
#
# Copyright (c) 2007, 2008, 2009 GreenPlum.  All rights reserved.
# Author: Jeffrey I Cohen
#

use strict;
use warnings;
use POSIX;
use File::Spec;
use Config;

# Load atmsort module from the same dir as this script
use FindBin;
use lib "$FindBin::Bin";
use atmsort;

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
gpdiff is invoked by pg_regress as part of "make install-check".
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

Copyright (c) 2007, 2008, 2009 GreenPlum.  All rights reserved.

Address bug reports and comments to: bugs@greenplum.org


=cut

# Calls pod2usage, but loads the module first.
# Pod::Usage is loaded lazily when needed, if the --help or other such option
# is actually used. Loading the module takes some time, which adds up when
# running hundreds of regression tests, and pg_regress calls this script twice
# for every test.
sub lazy_pod2usage
{
    require Pod::Usage;
    Pod::Usage::pod2usage(@_);
}

my %glob_atmsort_args;
our $ATMDIFF = "diff";

my $glob_ignore_headers;
my $glob_ignore_plans;
my $glob_init_file = [];

sub gpdiff_files
{
    my ($f1, $f2, $d2d) = @_;
    my $need_equiv = 0;
    my @tmpfils;

    # need gnu diff on solaris
    if ($Config{'osname'} =~ m/solaris|sunos/i)
    {
        $ATMDIFF = "gdiff";
    }

    for my $ii (1..2)
    {
        my $tmpnam;

        for (;;)
        {
            my $tmpfh;

            $tmpnam = tmpnam();
            sysopen($tmpfh, $tmpnam, O_RDWR | O_CREAT | O_EXCL) && last;
        }

        push @tmpfils, $tmpnam;
    }

    my $newf1 = shift @tmpfils;
    my $newf2 = shift @tmpfils;

#    print $glob_atmsort_args, "\n";

    if (defined($d2d) && exists($d2d->{equiv}))
    {
        # assume f1 and f2 are the same...
        atmsort::atmsort_init(%glob_atmsort_args, DO_EQUIV => 'compare');
        atmsort::run($f1, $newf1);

        atmsort::atmsort_init(%glob_atmsort_args, DO_EQUIV => 'make');
        atmsort::run($f2, $newf2);
    }
    else
    {
        atmsort::atmsort_init(%glob_atmsort_args);
        atmsort::run($f1, $newf1);
        atmsort::run($f2, $newf2);
    }

    my $args = join(" ", @ARGV, $newf1, $newf2);

#   print "args: $args\n";

    my $outi =`$ATMDIFF $args`;

    my $stat = $? >> 8; # diff status

    unless (defined($d2d) && exists($d2d->{equiv}))
    {
        # check for start_equiv unless already doing equiv check

        # get the count of matching lines
        my $grepout = `grep -c start_equiv $f1`;
        chomp $grepout;

        $need_equiv = $grepout;
#        $need_equiv = 0;

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
            $outi = "$ATMDIFF $f1 $f2" . ".equiv\n" . $outi;
        }
        else
        {
            $outi = "$ATMDIFF $f1 $f2\n" . $outi;
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

        if ( opendir($dir_h, $dir) )
        {
            my $fnam;
            while ($fnam = readdir($dir_h))
            {
                # ignore ., ..
                next unless ($fnam !~ m/^(\.)(\.)*$/);

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
    my $man         = 0;
    my $help        = 0;
    my $verzion     = 0;
    my $verbose     = 0;
    my $pmsg        = "";
    my @arg2;              # arg list for diff
    my %init_dup;

#    getatm();

    # check for man or help args
    if (scalar(@ARGV))
    {
        my $argc = -1;
        my $maxarg = scalar(@ARGV);

        while (($argc+1) < $maxarg)
        {
            $argc++;
            my $arg = $ARGV[$argc];

            unless ($arg =~ m/^\-/)
            {
                # even if no dash, might be a value for a dash arg...
                push @arg2, $arg;
                next;
            }

            # ENGINF-180: ignore header formatting
            if ($arg =~ m/^\-(\-)*(gpd\_ignore\_headers|gp\_ignore\_headers)$/i)
            {
                $glob_ignore_headers = 1;
                next;
            }
            if ($arg =~ m/^\-(\-)*(gpd\_ignore\_plans|gp\_ignore\_plans)$/i)
            {
                $glob_ignore_plans = 1;
                next;
            }
            if ($arg =~ m/^\-(\-)*(gpd\_init|gp\_init)(\=)*(.*)$/i)
            {
                if ($arg =~ m/\=/) # check if "=filename"
                {
                    my @foo = split (/\=/, $arg, 2);

                    die "no init file" unless (2 == scalar(@foo));

                    my $init_file = pop @foo;

                    # ENGINF-200: allow multiple init files
                    if (exists($init_dup{$init_file}))
                    {
                        warn "duplicate init file \'$init_file\', skipping...";
                    }
                    else
                    {
                        push @{$glob_init_file}, $init_file;

                        $init_dup{$init_file} = 1;
                    }
                }
                else # next arg must be init file
                {
                    $argc++;

                    die "no init file" unless (defined($ARGV[$argc]));

                    my $init_file = $ARGV[$argc];

                    # ENGINF-200: allow multiple init files
                    if (exists($init_dup{$init_file}))
                    {
                        warn "duplicate init file \'$init_file\', skipping...";
                    }
                    else
                    {
                        push @{$glob_init_file}, $init_file;

                        $init_dup{$init_file} = 1;
                    }
                }
                next;
            }
            if ($arg =~ m/^\-(\-)*(v|version)$/)
            {
                $verzion = 1;
                next;
            }
            if ($arg =~ m/^\-(\-)*(V|verbose)$/)
            {
                $verbose = 1;
                next;
            }
            if ($arg =~ m/^\-(\-)*(man|help|\?)$/i)
            {
                if ($arg =~ m/man/i)
                {
                    $man = 1;
                }
                else
                {
                    $help = 1;
                }
                next;
            }

            # put all "dash" args on separate list for diff
            push @arg2, $arg;

        } # end for
    }
    else
    {
        $pmsg = "missing an operand after \`gpdiff\'";
        $help = 1;
    }

    if ((1 == scalar(@ARGV)) && (!($help || $man || $verzion)))
    {
        $pmsg = "unknown operand: $ARGV[0]";
        $help = 1;
    }

    if ($verzion)
    {
        my $VERSION = do { my @r = (q$Revision$ =~ /\d+/g); sprintf "%d."."%02d" x $#r, @r }; # must be all one line, for MakeMaker

        # need gnu diff on solaris
        if ($Config{'osname'} =~ m/solaris|sunos/i)
        {
            $ATMDIFF = "gdiff";
        }
        my $whichdiff = `which $ATMDIFF`;
        chomp $whichdiff;
        print "$0 version $VERSION\n";
        print "Type \'gpdiff.pl --help\' for more information on the standard options\n";
        print "$0 calls the \"", $whichdiff, "\" utility:\n\n";

        my $outi = `$ATMDIFF -v`;

        $outi =~ s/^/   /gm;

        print $outi, "\n";

        exit(1);
    }

    lazy_pod2usage(-msg => $pmsg, -exitstatus => 1) if $help;
    lazy_pod2usage(-msg => $pmsg, -exitstatus => 0, -verbose => 2) if $man;

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

    # use the "stripped" arg list for diff
    @ARGV = ();

    # remove the filenames
    pop @arg2;
    pop @arg2;

    push(@ARGV, @arg2);

    if ($verbose)
    {
        $glob_atmsort_args{VERBOSE} = 1;
    }

    # ENGINF-180: tell atmsort to ignore header formatting (globally)
    if ($glob_ignore_headers)
    {
        $glob_atmsort_args{IGNORE_HEADERS} = 1;
    }

    # Tell atmsort to ignore plan content if -gpd_ignore_plans is set
    if ($glob_ignore_plans)
    {
        $glob_atmsort_args{IGNORE_PLANS} = 1;
    }

    # ENGINF-200: allow multiple init files
    if (defined($glob_init_file) && scalar(@{$glob_init_file}))
    {
        # MPP-12262: test here, because we don't get status for atmsort call
        for my $init_file (@{$glob_init_file})
        {
            die "no such file: $init_file" unless (-e $init_file);
        }
        @{$glob_atmsort_args{INIT_FILES}} = @{$glob_init_file};
    }

    exit(filefunc($f1, $f2));
}
