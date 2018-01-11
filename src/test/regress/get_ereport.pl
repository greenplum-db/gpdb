#!/usr/bin/env perl
#
# Portions Copyright (c) 2009, 2010, 2011 Greenplum Inc
# Portions Copyright (c) 2012-Present Pivotal Software, Inc.
#
# Author: Jeffrey I Cohen
#
#
use Pod::Usage;
use Getopt::Long;
Getopt::Long::Configure qw(pass_through);
use Data::Dumper;
use File::Spec;
use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin";
use GPTest qw(print_version);

=head1 NAME

B<get_ereport.pl> - find error messages in c source files

=head1 SYNOPSIS

B<get_ereport.pl> [-quiet] [-lax] filename [filename...]

Options:

    -help            brief help message
    -man             full documentation
    -version         print version 
	-quiet           be less verbose
	-lax             be very lax
	-fullname        print full file name (with GP_IGNORE)
	-elog            include elog messages as well

=head1 OPTIONS

=over 8

=item B<-help>

    Print a brief help message and exits.

=item B<-man>
    
    Prints the manual page and exits.

=item B<-version>

    Prints the get_ereport.pl version 

=item B<-quiet>

    Prints only messages, no line numbers or summaries.

=item B<-lax>

    Cleanup spaces, vertical bar (|), etc, that might effect gpdiff
    comparison (if invoked via a sql script)

=item B<-fullname>

    Print the full file name information in a GP_IGNORE message to
    make it easier to track ereport diffs back to the original file.

=item B<-elog>

    Include elog messages as well.


=back

=head1 DESCRIPTION

For each supplied file, get_ereport extracts the ereport (error message) 
entries of level ERROR, WARNING, NOTICE, or FATAL.

=head1 AUTHORS

Jeffrey I Cohen

Portions Copyright (c) 2009-2011 GreenPlum.  All rights reserved.
Portions Copyright (c) 2012-Present Pivotal Software, Inc.

Address bug reports and comments to: bugs@greenplum.org


=cut

if (1)
{
    my $man		  = 0;
    my $help	  = 0;
    my $verzion	  = 0;
    my $pmsg	  = "";
	my $bQuiet	  = 0;
	my $bLax	  = 0;
	my $bFullname = 0;
	my $bElog	  = 0;

GetOptions(
    'help|h|?' => \$help, man => \$man, 
    'quiet' => \$bQuiet,
    'lax' => \$bLax,
    'fullname' => \$bFullname,
    'elog' => \$bElog,
    'version|v' => \&print_version
    );

    pod2usage(-msg => $pmsg, -exitstatus => 1) unless (scalar(@ARGV) >= 1);

    pod2usage(-msg => $pmsg, -exitstatus => 1) if $help;
    pod2usage(-msg => $pmsg, -exitstatus => 0, -verbose => 2) if $man;

	my $printname = 1;
	
	my $linecnt = 0;
	my $msgcnt = 0;
	my $totmsgcnt = 0;

    for my $filnam (@ARGV)
    {
		if ($bLax)
		{
			next
				if ($filnam =~ m/(\/bootstrap\.)|(\/bootscanner\.)/i);
		}

		my $gotmsg = 0;
		
		my $fh;

        # $$$ $$$ undefine input record separator (\n")
        # and slurp entire file into variable

        local $/;
        undef $/;

		open ($fh, "<$filnam") or die "can't open $filnam: $!";

		my $whole_file = <$fh>;

		close $fh;

		# split file into separate lines
		my @lines = split(/\n/, $whole_file);

		$linecnt = 1;

		my $orig_filnam = $filnam;

		for my $ii (0..(scalar(@lines) - 1) )
		{
			my $ini = $lines[$ii] . "\n"; # add the newline back...

			my $regex = "ereport";

			$regex = "(ereport|elog)" if ($bElog);

			if ($ini =~ m/$regex/)
			{
			   if ($ini =~ m/$regex\s*\(\s*(ERROR|WARNING|NOTICE|FATAL)\,/)
			   {
				   $gotmsg = 1;
				   $msgcnt++;
				   $totmsgcnt++;
			   }
			   else
			   {
				   # check for errSendAlert in next 10 lines
				   
				   my $lookahead = (scalar(@lines) - 1);

				   $lookahead = $ii + 10
					   if (($lookahead - $ii) > 10);

				   for my $jj ($ii..$lookahead)
				   {
					   my $l2 = $lines[$jj];

					   if ($l2 =~ m/errSendAlert\(\s*true\s*\)/)
					   {
						   $gotmsg = 1;
						   $msgcnt++;
						   $totmsgcnt++;
						   last;
					   }
					   last # check for semicolon ending statement
						   if ($l2 =~ m/\;/);
				   }
			   }
			}

			if ($gotmsg)
			{
				if ($printname)
				{
					print "FILENAME: ", $filnam, "\n"
						unless ($bQuiet);

					# shorten the filename
					if ($filnam =~ m/\//)
					{
						my @foo = split("/", $filnam);

						$filnam = pop(@foo);
					}

					if ($bLax)
					{
						# avoid case-sensitive sort, plus lexical ordering for
						# non-alpha characters
						$filnam = lc($filnam);
						
						$filnam =~ s/\.c$//;
						$filnam =~ s/\W/x/g;
						$filnam =~ s/\_/x/g;

						$filnam =~ s/1/one/g;
						$filnam =~ s/2/two/g;
						$filnam =~ s/3/three/g;
						$filnam =~ s/4/four/g;
						$filnam =~ s/5/five/g;
						$filnam =~ s/6/six/g;
						$filnam =~ s/7/seven/g;
						$filnam =~ s/8/eight/g;
						$filnam =~ s/9/nine/g;
						$filnam =~ s/0/zero/g;

						# ugh.  build a prefix which is the first char
						# of the filename plus the encoded length, so
						# only sort subgroups of same length (to avoid
						# more collation sequence issues).
						
						my @ww = split (//, $filnam);
						my $fprefix = $ww[0];

						my $flen = sprintf( "%03d", length($filnam));
						$flen =~ tr/0123456789/abcdefghij/;

						$filnam = $fprefix . $flen . ": " . $filnam;
						

					}

					if ($bFullname)
					{
						# add prefix for sorting, but still get ignored
						print "aaaaa GP_IGNORE: ", $filnam, " - ", 
						$orig_filnam, "\n";
					}

					$printname = 0;
				}

				print $filnam, ":";
				printf "%08d: ", $linecnt;
#					unless ($bQuiet);

				$ini =~ s/\t/ /gm
					if ($bLax);
				$ini =~ s/\s+/ /gm
					if ($bLax);

				$ini =~ s/\|/\*PIPE\*/gm
					if ($bLax);

				$ini .= "\n"
					unless ($ini =~ m/\n$/);

				print $ini;
				$gotmsg = 0
					if ($ini =~ m/\;\s*$/);
			}
			$linecnt++;
		}

		if ($msgcnt)
		{
			print "\n", $orig_filnam, ":  ", $msgcnt, " messages\n\n"
				unless ($bQuiet);
			$msgcnt = 0;
		}

		$printname = 1;

    } # end for filnam

	if ($totmsgcnt)
	{
		print "\n\nfound ", $totmsgcnt, " total messages\n"
			unless ($bQuiet);
	}


}


exit();
