#!/usr/bin/perl

=head1 NAME

B<mrdiff.pl> - Map/Reduce Diff utility script

=head1 SYNOPSIS

B<mrdiff.pl> [options] <output_dir> <expected_dir> <test_name>

Options:

    -help            brief help message
    -man             full documentation
    -version         print version information

=head1 OPTIONS

=over 8

=item B<-help>

    Print a brief help message and exits.

=item B<-man>
    
    Prints the manual page and exits.

=item B<-version>

    Prints version information

=back

=head1 DESCRIPTION

The diff utility looks for all files matching <test_name>.*" in
the output directory and compares them against the like named files
in the expected directory.

It produces either $outdir/$test_name.suc or $outdir/$test_name.fail
depending on the results of the diff.

To handle the underlying diff the utility calls gpdiff.pl

=head1 AUTHORS

Caleb Welton

Copyright (c) 2008 GreenPlum.  All rights reserved

Address bug reports and comments to: cwelton@greenplum.com

=cut

use warnings;
use strict;
use Pod::Usage;
use Getopt::Long;
use Cwd;

my $man  = 0;
my $help = 0;

GetOptions('help|?' => \$help, 
		   man => \$man,
           )
	or pod2usage(2);
pod2usage(-exitstatus => 1) if $help;
pod2usage(-exitstatus => 0, -verbose => 2) if $man;

pod2usage(1) unless ($#ARGV == 2);
my ($output, $expected, $testname) = @ARGV;

my @outfiles = sort(glob("$output/$testname.*"));
my @expfiles = sort(glob("$expected/$testname.*"));

# We find gpdiff in the source tree assuming that this is being run
# from the source tree as well.
my $GPDIFF = getcwd();
$GPDIFF =~ s|(.*/cdb-pg/src)(/bin/gpmapreduce/.*)|$1/test/regress/gpdiff.pl|;
die "unable to find gpdiff.pl" unless (-e $GPDIFF);


sub printheader($$)
{
	my ($a, $b) = @_;
	my $fill = "-" x (length($a) + length($b));
	print "\n";
	print "------------$fill\n";
	print "FILE:  $a <=> $b\n";
	print "------------$fill\n";
}

my ($out, $exp, $ofile, $efile);
while ($#outfiles >= 0 && $#expfiles >= 0) 
{
	$out = shift @outfiles;
	$exp = shift @expfiles;

	# we have paths, and want to check if the filename is the same
	while (1)
	{
		$out =~ m/$output\/(.*)/ or die();
		$ofile = $1;
		$exp =~ m/$expected\/(.*)/  or die();
		$efile = $1;
	
		if ($ofile eq $efile)
		{
			my $diff = `$GPDIFF $out $exp`;
			if (length($diff) > 0)
			{
				printheader($out,$exp);
				print $diff;
			}
			last;
		}
		elsif ($ofile lt $efile) 
		{
			printheader($out,"<missing>") unless ($ofile =~ m/^$testname.suc$/);
			last if ($#outfiles < 0);
			$out = shift @outfiles;
		}
		else
		{
			printheader("<missing>", $exp);
			last if ($#expfiles < 0);
			$exp = shift @expfiles;
		}

	}
}

while ($#outfiles >= 0)
{
	$out = shift @outfiles;
	$out =~ m/$output\/(.*)/ or die();
	$ofile = $1;
	printheader($out, "<missing>") unless ($ofile =~ m/^$testname.suc$/);
}
while ($#expfiles >= 0)
{
	$exp = shift @expfiles;
	printheader("<missing>", $exp);
}
