#!/usr/bin/perl
#
# fixup.pl - Greenplum MapReduce fixup script
#
# copyright (c) 2008
# Author: Jeffrey I Cohen, Caleb Welton
#

use strict;
use warnings;
use Pod::Usage;
use Getopt::Long;
use Cwd;

# let Makefile.pl update the connect string if necessary...
my $gen_connect = "__CONNECT: postgres __";
my $glob_id = "";

# find the connect string
my $glob_connect = $gen_connect;
$glob_connect =~ s/^\_\_CONNECT\:(.*)\_\_$/$1/;

my $man  = 0;
my $help = 0;

GetOptions('help|?' => \$help, 
		   man => \$man,
           )
	or pod2usage(2);
pod2usage(-msg => $glob_id, -exitstatus => 1) if $help;
pod2usage(-msg => $glob_id, -exitstatus => 0, -verbose => 2) if $man;

# Check that input file exists and is readable



# convert a postgresql psql formatted table into an array of hashes
sub tablelizer
{
    my ($ini, $got_line1) = @_;

    # first, split into separate lines, the find all the column headings

    my @lines = split(/\n/, $ini);

    return undef
        unless (scalar(@lines));

    # if the first line is supplied, then it has the column headers,
    # so don't try to find them (or the ---+---- separator) in
    # "lines"
    my $line1 = $got_line1;
    $line1 = shift @lines
        unless (defined($got_line1));

    # look for <space>|<space>
    my @colheads = split(/\s+\|\s+/, $line1);

    # fixup first, last column head (remove leading,trailing spaces)

    $colheads[0] =~ s/^\s+//;
    $colheads[0] =~ s/\s+$//;
    $colheads[-1] =~ s/^\s+//;
    $colheads[-1] =~ s/\s+$//;

    return undef
        unless (scalar(@lines));
    
    shift @lines # skip dashed separator (unless it was skipped already)
        unless (defined($got_line1));
    
    my @rows;

    for my $lin (@lines)
    {
        my @cols = split(/\|/, $lin, scalar(@colheads));
        last 
            unless (scalar(@cols) == scalar(@colheads));

        my $rowh = {};

        for my $colhdcnt (0..(scalar(@colheads)-1))
        {
            my $rawcol = shift @cols;

            $rawcol =~ s/^\s+//;
            $rawcol =~ s/\s+$//;

            my $colhd = $colheads[$colhdcnt];
            $rowh->{($colhdcnt+1)} = $rawcol;
        }
        push @rows, $rowh;
    }

    return \@rows;
}

sub gethostname
{
    my $psql_str = "psql ";
    
    $psql_str .= $glob_connect
        if (defined($glob_connect));

    $psql_str .= " -c \'select content, role, status, hostname from gp_segment_configuration\'";

    my $tabdef = `$psql_str`;

#    print $tabdef;

    # do something reasonable on error...
    if (($tabdef =~ m/database.*does not exist/) 
        || ($tabdef =~ m/could not connect/) )
    {
        return `hostname`;
    }

    my $mpp_config_table = tablelizer($tabdef);

#    print Data::Dumper->Dump([$mpp_config_table]);

    my $hostname= "localhost";

    for my $rowh (@{$mpp_config_table})
    {
        if (($rowh->{1} == 0) && # content (seg 0)
            ($rowh->{2} =~ m/p/) && # role =primary
            ($rowh->{3} =~ m/u/))    # status = up 
        {
            $hostname = $rowh->{4};
            last;
        }
        
    }

    return $hostname;
}

if (1)
{
	my $curdir   = getcwd();
	if ($curdir !~ /mapreduce/) {
		$curdir .= "/mapreduce";
	}
        my $db_user    = `whoami`;
	my $hostname = gethostname(); # `hostname`
	chomp $db_user;
	chomp $hostname;

	# We assume that this script is run by the make file in example directory
	my $abs_srcdir = "$curdir";

    for my $file (@ARGV)
    {
        pod2usage(-exitstatus => 1) unless ($file);

        (-d $file) and die "ERROR: '$file' is a directory\n";
        (-f $file) or  die "ERROR: No such file '$file'\n";
        (-r $file) or  die "ERROR: No read permissions for '$file'\n";

        my $outfile = $file;

        # remove the ".in" (input) suffix for the outfile name
        $outfile =~ s/\.in$//g;

		open(INPUT, "<$file");
		open(OUTPUT, ">$outfile");
		while (<INPUT>) {
			s/\@db_user\@/$db_user/gm; 
			s/\@abs_srcdir\@/$abs_srcdir/gm;
			s/\@hostname\@/$hostname/gm;
			print OUTPUT
		}
		close(INPUT);
		close(OUTPUT);
    }
}

exit(0);
