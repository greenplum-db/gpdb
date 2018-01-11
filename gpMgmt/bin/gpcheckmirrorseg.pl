#!/usr/bin/env perl
#
# $Header$
#
# copyright (c) 2010, 2011
# Author: Jeffrey I Cohen
#
#
use Pod::Usage;
use Getopt::Long;
use Data::Dumper;
use File::Spec;
use File::Basename;
use Time::localtime;
use Config;
use Scalar::Util qw(looks_like_number);
use strict;
use warnings;

=head1 NAME

B<gpcheckmirrorseg.pl> - check some mirrors

=head1 SYNOPSIS

B<gpcheckmirrorseg> [options] 

Options:

    -help             brief help message
    -man              full documentation
    -connect          psql connect parameters
    -ignore           filename pattern to ignore; ANY match in the file path WILL be omitted from results
    -dirignore        name pattern to ignore for directory file-types; does not recursively ignore contents of the directory
	-mastermirror     compare master and standby only 
	-forcefilecompare perform full file comparison for master/standby
	-matchdirs        compare directory structure
	-skipsegments     skip mirror check for segments which are not in sync
	-parallel         run checks in parallel
	-nohash           skip md5 hash during file comparison
	-fixfile          location for fix file
	-content          only compare segments with the specified content id
	-filename         only compare the contents of the specified files

=head1 OPTIONS

=over 8

=item B<-help>

    Print a brief help message and exits.

=item B<-man>
    
    Prints the manual page and exits.

=item B<-connect>
    
    psql connect string, e.g:

    -connect '-p 11000 template1'

=item B<-ignore>

Specify a string pattern that should be excluded when comparing the contents of data
directories.  To specify multiple patterns, use multiple ignore
arguments, eg:

  -ignore gp_dump -ignore pg_dump

Note that "ignore" controls comparison of files by matching the ignore strings within the full path whereas
"dirignore" controls the comparison of directory structure.

=item B<-dirignore>

Specify a directory name pattern that should be exclude when comparing
data directory structure during the "matchdir" phase.  To specify
multiple patterns, use multiple dirignore arguments, eg:

  -dirignore gp_dump -dirignore pg_dump

Note that "ignore" controls comparison of files by matching the ignore strings within the full path whereas
"dirignore" controls the comparison of directory structure.

=item B<-mastermirror> 
    
Boolean flag to control whether to only compare the state of the
master with the standby (normally false).  If true, only checks that
the master and standby have identical relfilenode layout, ie the
datafiles are identical in name and number.  Does not compare file
contents, which may differ due to logical mirroring.

=item B<-forcefilecompare> 
    
Boolean flag to control whether to compare file contents for master
mirroring (normally false).  That is, file contents are always
compared for primary/mirror segment pairs, but file contents are not
compared for master/standby unless this flag is set.

=item B<-matchdirs> 
    
Boolean flag to control whether to compare if the segment and mirror
have identical directory structure (normally true), in addition to the
standard check on file contents.  Note that this check is unaffected
by the "ignore" option: that is, while "ignore" patterns will exclude
the comparison of file contents for files that match the pattern,
directory names that match the pattern will not be excluded during the
directory structure comparison unless they are specified by
"dirignore".  Directory structure differences are flagged as extra or
missing files.

=item B<-skipsegments> 
    
Boolean flag to control whether to skip mirror check for segments
which are not in sync (normally true).

=item B<-parallel> 
    
Boolean flag to control whether to generate the md5 hashes for each
segment in parallel, or sequentially (normally true).  If true,
gpcheckmirrorseg forks a separate process on each segment to generate
the md5 hashes for the initial raw diff phase.  Note that the actual
diff of primary/mirror hashes and subsequent "enhanced diff" phases
are performed sequentially.

=item B<-nohash> 
    
Boolean flag to control whether md5 hash generation is disabled
(normally false).  That is, the standard comparison phase generates an
md5 hash for every file, but if "nohash" is set to true, then the
comparison phase only performs a file size difference.

=item B<-fixfile> 
    
Specify a filename for the fixfile (else generic name is generated in /tmp).

=item B<-content>

If specified, choose a particular segment or set of segments that
should be compared.  If not specified all segments are compared.  To
specify multiple segments, use multiple content arguments, eg:

  -content 1 -content 2

=item B<-filename>

If specified, choose a particular file or set of files that should be
compared, versus comparing all files.  For relations, the filename
argument is the pg_class relfilenode.  Note that the filename
filtering takes place after the content filter and subsequent to the
ignore pattern exclusion.  To specify multiple files, use multiple
filename arguments, eg:

  -filename 1 -filename 2

=back

=head1 DESCRIPTION

gpcheckmirrorseg.pl performs validations that the primary and mirror
segments contain the same data.  Sample usage:

perl gpcheckmirrorseg.pl -connect "-p 11000 template1"

Substitute the correct "connect string" for your postgres database.

=head2 Comparison Methods

First, on each primary/mirror pair, the script constructs a list of
all relfilenodes that has their size and an md5 hash of the entire
file contents.  The primary and mirror output is compared to look for
file size differences, hash differences, and whether identical sets of
files are listed.  If the first, or "raw" diff is unsuccessful, the
script proceeds with an "enhanced" diff.  The relfilenodes which
differed are reclassified by type as append-only, heap, btree, or
other (unknown).  Although FileRep is a block-for-block copy, file
contents may differ slightly due to the effects of aborted
transactions or recovery.  For AO tables, the relfilenode on the
primary may contain uncommitted data, so the enhanced diff will only
compare ao files up to the committed EOF.  For heap tables, the MVCC
"hint bits" for rows on the mirror may differ from the primary after
recovery, so the enhanced comparison masks these bits for heap
relfilenodes.  Btree indexes have a similar discrepancy due to the
presence of LP_DELETE hints in the line pointers for deleted tuples.
After it preprocesses the relfilenodes, the enhanced diff constructs
md5 hashes of their contents and compares the results for each
primary/mirror pair.  Finally, if the enhanced diff is still
unsuccessful, even with the known exceptions for heap and ao
relfilenodes, the script proceeds to the final phase: a block-by-block
diff.  For this phase, the script utilizes the ao and heap
preprocessing from the "enhanced" diff, but it generates an Adler32
hash for each block.  The final diff compares the hashes for each
block, highlighting the exact location where each file differs.  If
the final diff is only the result of some trailing empty blocks from
an aborted transaction on a heap table, the diff is suppressed.

=head2 Master Mirror Comparison

When -mastermirror=true, the comparison is slightly different.  The
master and standby are synchronized using logical mirroring, not
file-based replication, so the contents of the relfilenodes can vary.
Thus, the master mirror comparison checks that the master and standby
have an indentical complement of relfilenodes, with the additional
requirement that all files are categorized as zero-length or
non-zero-length.  A zero-length file on the master must have a
matching zero-length file on the standby, while a non-zero-length file
on the master must have a corresponding non-zero file on the standby
(which is not necessarily the same length, as its contents may be
slightly different).

=head1 CAVEATS/LIMITATIONS 

Note that the preprocessing routine for heap files that filters hint
bits makes assumptions about the C struct layout of the block headers
and tuple headers, which is somewhat platform dependent. Column store
tables have a similar issue, since the implementation encodes the eof
for each file of the cols of a segno in a single raw vpinfo struct
(containing a variable length array) that is stored in a catalog
column as a bytea.  In particular (see MPP-10900), we assume that
platform is 64-bit aligned little-endian (x86).

=head1 AUTHORS

Jeffrey I Cohen

Copyright (c) 2010, 2011 GreenPlum.  All rights reserved.  

Address bug reports and comments to: bugs@greenplum.org


=cut

my $glob_id = "";

my $glob_connect;

my $glob_port;

my $glob_ignore;

my $glob_dirignore;

my $glob_exit;

my $glob_mm;

my $glob_ffc;

my $glob_matchdirs;

my $glob_skipseg;

my $glob_diff;

my $glob_usefork;

my $glob_nohash;

my $glob_fixfile;
my $glob_fixlist;

my $glob_content;

my $glob_platform;

my $glob_filename;

my $glob_psql_str = "psql ";
my $glob_debug_fmt = "    DEBUG: ";

# from pg_tablespace.h: 
my $DEFAULTTABLESPACE_OID = 1663;
my $GLOBALTABLESPACE_OID  = 1664;

my $DEFAULTFILESPACE_OID  = 3052;

BEGIN {
	my $man  = 0;
	my $help = 0;
	my $conn;
	my @ignore;
	my @dirignore;
	my $skipseg;
	my $mm;
	my $ffc;
	my $matchdirs;
	my $parallel;
	my $nohash;
	my $fixfile;
	my @content;
	my @filnam;

	GetOptions(
               'help|?' => \$help, man => \$man, 
               "connect=s" => \$conn,
		       'exclude|ignore:s' => \@ignore,
		       'directoryignore|dirignore:s' => \@dirignore,
		       'mm|mastermirror:s' => \$mm,
		       'ffc|forcefilecompare:s' => \$ffc,
		       'matchdirs:s' => \$matchdirs,
		       'skipsegments:s' => \$skipseg,
		       'parallel:s' => \$parallel,
		       'nohash:s' => \$nohash,
		       'fixfile:s' => \$fixfile,
		       'content:i' => \@content,
		       'filename:s' => \@filnam,
               )
        or pod2usage(2);

    
	pod2usage(-msg => $glob_id, -exitstatus => 1) if $help;
	pod2usage(-msg => $glob_id, -exitstatus => 0, -verbose => 2) if $man;

	$glob_connect = $conn;
	$glob_ignore = \@ignore;
	$glob_dirignore = \@dirignore;
	$glob_filename = \@filnam;

	push @{$glob_dirignore}, "pgsql_tmp"; # this directory only on primaries
	push @{$glob_dirignore}, "pg_verify"; # this directory may be just on primary

	$glob_content = \@content;

	if (defined($skipseg))
	{
		$glob_skipseg = ($skipseg =~ m/^\s*(1|t|true|y|yes)/i) ? 1 : 0;
	}
	else
	{
		$glob_skipseg = 1;
	}

	if (defined($mm))
	{
		$glob_mm = ($mm =~ m/^\s*(1|t|true|y|yes)/i) ? 1 : 0;

		push @{$glob_ignore}, "recovery.conf"
			if ($glob_mm);

	}
	else
	{
		$glob_mm = 0;
	}

	if (defined($ffc))
	{
		$glob_ffc = ($ffc =~ m/^\s*(1|t|true|y|yes)/i) ? 1 : 0;
	}
	else
	{
		$glob_ffc = 0;
	}

	if (defined($matchdirs))
	{
		$glob_matchdirs = ($matchdirs =~ m/^\s*(1|t|true|y|yes)/i) ? 1 : 0;
	}
	else
	{
		$glob_matchdirs = 1;
	}

	if (defined($parallel))
	{
		$glob_usefork = ($parallel =~ m/^\s*(1|t|true|y|yes)/i) ? 1 : 0;
	}
	else
	{
		$glob_usefork = 1;
	}

	if (defined($nohash))
	{
		$glob_nohash = ($nohash =~ m/^\s*(1|t|true|y|yes)/i) ? 1 : 0;
	}
	else
	{
		$glob_nohash = 0;
	}

	$glob_diff = "diff";
	# MPP-9810: need gnu diff on solaris
	if ($Config{'osname'} =~ m/solaris|sunos/i)
	{
		$glob_diff = "gdiff";
	}

	$glob_fixfile = $fixfile if (defined($fixfile) && 
								 length($fixfile));
	$glob_fixlist = [];
}

sub printdivider
{
	print "____________________________________________________________________________________________________________\n";
}

sub directorydivider
{
	print "############################################################################################################\n";
}

# For dumping datastructure in this tool should use this routine.
# This allows controling stuff like:
#  - adding KEYWORD at start of each statement for better readability plus greping 
#  - disabling the logging etc.
sub debug_print
{
	my ($input) = @_;
	my $foo;
	($foo = $input) =~ s/\n/\n${glob_debug_fmt}/g;
	print "${glob_debug_fmt}$foo \n";
}

# convert a postgresql psql formatted table into an array of hashes
sub tablelizer
{
    my ($ini, $got_line1) = @_;

    # first, split into separate lines, then find all the column headings

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
            $rowh->{$colhd} = $rawcol;
        }
        push @rows, $rowh;
    }

    return \@rows;
}

# build a list of expressions to ignore
sub ignore_list
{
	my $iggy = shift;

	# file/directory names
	# MPP-8973: ignore changetracking files
	# MPP-9461: ignore filespace gp_dbid files
	my @m1 = qw(
		pg_stat
		pg_verify
		pg_log
		pg_internal
		pg_fsm.cache
		pgstat
		pgsql_tmp
		pg_changetracking
		pg_utilitymodedtmredo
		gp_dbid
		core
		db_dumps
		gp_temporary_files_filespace
		gp_transaction_files_filespace
		slru_checksum_file
                errlog
		);

	# add any other expressions (eg user-supplied)
	push @m1, @{$iggy} if (defined($iggy) && scalar(@{$iggy}));

	my @master;

	my @m3 = map(quotemeta, @m1);

	# file suffixes 
	# MPP-8887: postgresql.conf.bak generated by recovery
	my @m2 = qw(
		.log
		.opts
		.conf
		.bak
		_args
		.pid
		core
		_segargs
		);

	for my $ww (@m2)
	{
		push @m3, quotemeta($ww) . '$'; # check for EOL
	}

	my $regex = join ("|", @m3);

	# use to build a collection of "grep -v" commands, now use just one...
	push @master, " perl -nle \'print unless /(" . $regex . ")/  \' ";

	return \@master;
}

sub get_presh_txt
{
	my $bigstr = <<'EOF_bigstr';
##tiny perl script to list filename, filesize, md5 hash
#for my $filnam (@ARGV) # xargs supplies up to 5000 args
#{
#  my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
#      $atime,$mtime,$ctime,$blksize,$blocks) = stat($filnam);
# my $sz = $size;
# print $filnam, " ", $sz, " ";
# my $sslstr = `openssl dgst -md5 -hex $filnam`;
# if (defined($sslstr) && length($sslstr))
# {
#   print $sslstr;
# }
# else
# {
#   print "badMD5 badMD5\n";
# }
#}
EOF_bigstr

	$bigstr =~ s/^\#//gm;

	return $bigstr;
}

# For the case of master mirroring, build the "pre" sh script, but
# only check file names -- supply zero for size and hash to prevent
# subsequent "enhanced" diff phases.
sub get_mm_presh_txt
{
	my $bigstr = <<'EOF_bigstr';
##tiny perl script to list filename, filesize, md5 hash
#for my $filnam (@ARGV) # xargs supplies up to 5000 args
#{
#  my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
#      $atime,$mtime,$ctime,$blksize,$blocks) = stat($filnam);
# my $sz = $size ? 1 : 0; # nonzero size normalized to 1
# print $filnam, " ", $sz, " ($filnam) 0\n";
#}
EOF_bigstr

	$bigstr =~ s/^\#//gm;

	return $bigstr;
}

# just do size, not hash
sub get_nohash_presh_txt
{
	my $bigstr = <<'EOF_bigstr';
##tiny perl script to list filename, filesize, md5 hash
#for my $filnam (@ARGV) # xargs supplies up to 5000 args
#{
#  my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
#      $atime,$mtime,$ctime,$blksize,$blocks) = stat($filnam);
# print $filnam, " ", $size, " ($filnam) 0\n";
#}
EOF_bigstr

	$bigstr =~ s/^\#//gm;

	return $bigstr;
}

sub get_presh2_txt
{
	my $bigstr = <<'EOF_bigstr';
##nontiny perl script to preprocess ao or heap files
##by truncating to length (ao) or filtering hint bits (heap)
##
## args: -adler|noadler -btree|-heap|-other|(-ao <filesize> ) filename
##
## if adler is specified, prints blockno and adler32 for each block,
## else if noadler, just preprocesses each block according to ao/heap/btree 
## rules and prints it out.
#
##use strict;
##use warnings;
##use Data::Dumper;
#use POSIX; # for floor
#my $glob_adler = 0;
#my $zero_adler = 2147483649; # for case of blank block
#my $glob_blanksummary; # MPP-11432: check for blanks -- 
#					   # only valid for adler&&heap
#
#sub doprint
#{
#	my ($blockno, $buf, $isLastblock) = @_;
#	
#	if (!$glob_adler)
#	{
#		print $buf;
#	}
#	else
#	{
#		my $adlerval = adler32(1, [$buf]);
#		print $blockno, ":", $adlerval, "\n";
#
#		# NOTE: can ignore trailing empty blocks at end of file
#		#
#		# MPP-11432: look for trailing regions of contiguous blank blocks
#		if (defined($glob_blanksummary))
#		{
#			# if the current summary is NONE (ie no blanks), then set
#			# to TRAILING when first blank block is found.  If a
#			# subsequent block is non-blank, then reset the summary to
#			# ORPHAN.
#			if ($adlerval == $zero_adler)
#			{
#				$glob_blanksummary = "TRAILING"			
#					if ($glob_blanksummary eq "NONE");
#
#				# if NONE, set to TRAILING.  Subsequent checks will
#				# verify that there is a contiguous region of blank
#				# blocks until the eof, or reset the status to ORPHAN
#				# if a non-blank is discovered.
#				#
#				# if TRAILING and still blank, that's good - no change.
#				#
#				# if ORPHAN and found another blank, don't
#				# change. That's still bad.
#			}
#			else
#			{
#				# found non-blank block after blank region - bad!
#				$glob_blanksummary = "ORPHAN"			
#					if ($glob_blanksummary eq "TRAILING");
#			}
#		}
#	}
#	
#}
#
## adapted from Digest::Adler32
## see RFC 1950 section 9
#sub adler32 
#{
#	my ($ad32, $bufs) = @_;
#	
#	my $s1 = $ad32 & 0x0000FFFF;
#	my $s2 = ($ad32 >> 16) & 0x0000FFFF;
#
#	for my $buf (@{$bufs}) 
#	{
#		for my $c (unpack("C*", $buf)) 
#		{
#			$s1 = ($s1 + $c ) % 65521;
#			$s2 = ($s2 + $s1) % 65521;
#		}
#	}
#	$ad32 = ($s2 << 16) + $s1;
#	return $ad32;
#}
#
#if (1)
#{
#	my $flag1 = shift @ARGV;
#	my $flag2 = shift @ARGV;
#	my ($sz, $orig_sz, $cur_sz);
#	my $blockno = 0;
#	my $isBtree = 0;
#	my $isOther = 0;
#
#	if ($flag1 =~ m/noadler/i)
#	{
#		$glob_adler = 0;
#	}
#	else
#	{
#		$glob_adler = 1;
#	}
#	
#	if ($flag2 =~ m/ao/i)
#	{
#		$sz = shift @ARGV;
#		$orig_sz = $sz;
#		$cur_sz = 0;
#	}
#	else
#	{
#		# if type "other" (not btree or heap), then don't process block.
#		# Otherwise, treat (isBtree == 0) as heap block.
#		$isBtree = ($flag2 =~ m/btree/i);
#		$isOther = ($flag2 =~ m/other/i);
#
#		$glob_blanksummary = "NONE" if ($glob_adler && !$isOther);
#	}
#
#	my $filnam = shift @ARGV;
#
#	# AO case - truncate file to size
#	if (defined($sz))
#	{
#		my $fh;
#		
#		open ($fh, "<", $filnam) or die ("cannot open $filnam: $!");
#		my $bufsiz = 32768;
#		my $bufl;
#		
#		if ($bufsiz > $sz)
#		{
#			$bufl = $sz;
#			$sz = 0;
#		}
#		else
#		{
#			$sz -= $bufsiz;
#			$bufl = $bufsiz;
#		}
#		
#		while ($bufl)
#		{
#			my $buf;
#			my $readlen = read($fh, $buf, $bufl);
#			doprint($blockno, $buf);
#			
#			$cur_sz += $readlen;
#			
#			if ($readlen && ($readlen < $bufl))
#			{
#				warn "file $filnam shorter than $orig_sz bytes\n";
#				die "read only $cur_sz bytes (last block $readlen bytes vs $bufl)\n" ;
#			}
#			
#			if ($bufsiz > $sz)
#			{
#				$bufl = $sz;
#				$sz = 0;
#			}
#			else
#			{
#				$sz -= $bufsiz;
#				$bufl = $bufsiz;
#			}
#			$blockno++;
#		}
#		close $fh;
#	} # else heap or btree index
#	else
#	{
#		my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$statsize,
#			$atime,$mtime,$ctime,$statblksize,$statblocks) = stat($filnam);
#
#		my $fh;
#		
#		open ($fh, "<", $filnam) or die ("cannot open $filnam: $!");
#		
#		my $bufsiz = 32768;
#		my $buf;
#		my $realsiz = 0;
#		my $lastblock = POSIX::floor($statsize/$bufsiz);
#		
#		while (1) # read whole file
#		{
#			$realsiz = read($fh, $buf, $bufsiz);
#			
#			if ($realsiz < $bufsiz)
#			{
#				doprint($blockno, $buf, ($blockno == $lastblock)) if $realsiz;
#				last;
#			}
#
#			# just print block unless btree/heap case
#			goto L_endloop
#				if ($isOther);
#
#			# bits			  64 16			
#			# bytes			   8 2
#			my @foo = unpack("L2 S S S S S S ", $buf); # referring to PageHeaderData in bufpage.h for details
#			
#			shift @foo; # discard XLogRecPtr
#			shift @foo;
#			
#			shift @foo; # discard pd_checksum, pd_flags
#			shift @foo;
#			
#			my $pd_lower = shift @foo;
#			my $pd_upper = shift @foo;
#			my $pd_special = shift @foo;
#			
#			my $pd_pagesize_version = shift @foo;
#			
#			# print "page size: ", $pd_pagesize_version & 0xFF00, "\n";
#			# print "page version: ", $pd_pagesize_version & 0x00FF, "\n";
#			# print "pd special: $pd_special\n";
#
#			my $do_items; # controls unpack/masking of tuple items
#			$do_items = 1; # normally true, unless a btree META block
#
#			# btree blocks have BTPageOpaqueData struct (in pd_special region)
#			if ($isBtree
#				&& ($pd_special &&
#					($bufsiz > $pd_special)))
#			{
#				my $speclen = ($bufsiz - $pd_special)/2;
#				my @btpo = unpack("x$pd_special s$speclen", $buf);
#
##				print "btpo: ", Data::Dumper->Dump(\@btpo);
#
#				my $BTP_LEAF = 1;	 # leaf page, i.e. not internal page 
#				my $BTP_ROOT = 2;	 # root page (has no parent) 
#				my $BTP_DELETED	= 4; # page has been deleted from tree 
#				my $BTP_META = 8;	 # meta-page 
#				my $BTP_HALF_DEAD = 16;	    # empty, but still in tree 
#				my $BTP_SPLIT_END = 32;	    # rightmost page of split group 
#				my $BTP_HAS_GARBAGE = 64;	# page has LP_DELETEd tuples 
#
#				if (scalar(@btpo) > 7)
#				{
#					# check BTPageOpaqueData btpo_flags;
#
#					if ($btpo[6])
#					{
#						# XXX XXX XXX
#						# NOTE: don't unpack items for META block -- they
#						# are *not* index tuples!!
#						# XXX XXX XXX
#
#						$do_items = 0
#							if ($btpo[6] & $BTP_META);
#
#						# clear garbage flag
#						if ($btpo[6] & $BTP_HAS_GARBAGE)
#						{
#							$btpo[6] -= $BTP_HAS_GARBAGE;
#
#							my $fixbuf = pack("s$speclen", @btpo);
#							substr($buf, $pd_special, length($fixbuf)) 
#								= $fixbuf;
#						}
#					
#					}
#				}
#			} # end if special
#
#			goto L_endloop
#				unless ($do_items);
#			
#			my $szofpagehdrdata = 8 + (6 * 2);
#			
#			my $maxoffsetnum = ($pd_lower <= $szofpagehdrdata) ? 0 : 
#				# divide by sizeof ItemIdData
#				(($pd_lower - $szofpagehdrdata) / 4 ); 
#			
#			my $twomax = 2 * $maxoffsetnum ;
#			
#			my @baz = unpack("L2 S S S S S S S$twomax", $buf);			
#
#			my @pagehdr; # need to rebuild btree btpopaque
#			
#			for my $jj (1..8)
#			{
#				my $skippi = shift @baz;
#
#				push @pagehdr, $skippi;
#			}
#
#			@pagehdr[2] = 0; # mask out the checksum
#			my $page_header_with_checksum_masked = pack("L2 S6", @pagehdr);
#			substr($buf, 0, length($page_header_with_checksum_masked)) = $page_header_with_checksum_masked;
#			
#			my $ii = 0;
#			
#			# for all ItemIdData ("line pointers")
#			#   btree case: fix LP_DELETE
#			#   heap case:  find tuples and fix hint bits
#			while ($ii < scalar(@baz))
#			{
#				my $s1 = $baz[$ii]; # $s1 and $s2 is representing data stored in ItemIdData, see itemid.h for details
#				my $s2 = $baz[$ii+1];
#				
#				$ii += 2;
#				
#				my $lp_off = $s1;
#				$lp_off = $lp_off & 0x7FFF; # discard bit of lp_flags information
#
#				my $lp_len = $s2 >> 1; # discard bit of lp_flags information
#
#				if ($isBtree)
#				{
#					# fixup s2 (part of lp_flags) -- mask out
#					# LP_DELETE hint bit
#					$baz[$ii-1] = 0xFFFE & $s2;
#				}
#				else # heap tuple hint bit cleanup
#				{
#					my @ggg = unpack("x$lp_off S11 c c  ", $buf);
#				
#					# clear infomask flags
#					$ggg[9] = 0;
#					$ggg[10] = 0;
#				
#					my $fixbuf = pack("S11 c c  ", @ggg);
#					substr($buf, $lp_off, length($fixbuf)) = $fixbuf;
#				}
#				
#			} # end while ii < scalar(@baz)
#
#			if ($isBtree)
#			{
#				# use revised lp_flags in the lp array
#				my $fix_lp_array = pack("S$twomax", @baz);
#				substr($buf, length($page_header_with_checksum_masked), length($fix_lp_array)) = $fix_lp_array;
#			}
#
#		  L_endloop:			
#	        doprint($blockno, $buf, ($blockno == $lastblock)) if ($realsiz);
#			$blockno++;
#
#		} # end while read whole file
#		
#		if (defined($glob_blanksummary))
#		{
#			# MPP-11432: Note the presence of trailing contiguous
#			# blank blocks for the datafile, which is a valid
#			# difference for an aborted transaction.  However, a blank
#			# block or region of blank blocks that is followed by a
#			# non-blank block is an "orphan", which is an error.
#			print "BLANK_SUMMARY:", $glob_blanksummary, "\n";
#			#print "SIZE_SUMMARY:", $blockno, "\n";
#		}
#		
#		close $fh;
#	}
#}
EOF_bigstr

	$bigstr =~ s/^\#//gm;

	return $bigstr;
}

# diff primary and mirror file lists. entries are of the form:
#  filename filesize md5hash
#
# build a hash listing "extra" files (no match on primary/mirror),
# plus diffs in file sizes and hashes
sub dandydiff
{
	my ($prim, $mirr) = @_;
	my ($pfh, $mfh);

	open ($pfh, '<', $prim) or die ("cannot open $prim: $!");
	open ($mfh, '<', $mirr) or die ("cannot open $mirr: $!");
		
	my ($plin, $mlin);
	my $pstat = ($plin = <$pfh>);
	my $mstat = ($mlin = <$mfh>);

	my (@extrafiles, %sizediff, %hashdiff, @topdir);

	# MPP-11315: check if top directory exists on primary and mirror.
	# make sure file is non-zero, and first line is a TOPDIR line.
	if ($pstat && ($plin =~ m/TOPDIR/)) 
	{
		unless ($plin =~ m/TOPDIR_TRUE/)
		{
			push @topdir, "missing_topdir_p: " ;
		}

		$pstat = ($plin = <$pfh>);
	}
	if ($mstat && ($mlin =~ m/TOPDIR/))
	{
		unless ($mlin =~ m/TOPDIR_TRUE/)
		{
			push @topdir, "missing_topdir_m: " ;
		}

		$mstat = ($mlin = <$mfh>);
	}

	# if missing a top directory, don't bother with rest of comparison
	goto L_skipdandy
		if (scalar(@topdir));

	while ($pstat && $mstat)
	{
		$plin =~ s/\s+/ /gm;
		$mlin =~ s/\s+/ /gm;

		goto L_dandy1 if ($plin eq $mlin);

		# check filenames
		my @ppp = split(/\s+/, $plin);
		my @mmm = split(/\s+/, $mlin);

		die ("bad line: $plin\nin file: $prim") unless (scalar(@ppp));
		die ("bad line: $mlin\nin file: $mirr") unless (scalar(@mmm));
		die ("bad something: $plin\n$mlin")
			unless (scalar(@ppp) == scalar(@mmm));

		my $pfilnam = shift @ppp;
		my $mfilnam = shift @mmm;

		my $pfilsiz = shift @ppp;
		my $mfilsiz = shift @mmm;

		# GPDB_84_MERGE_FIXME: ignore relation forks other than the main on the
		# primary. Review to ensure gpcheckmirrorseg deals with relation forks
		# correctly in all use cases.
		if (($pfilnam =~ /_fsm$/)
			|| ($pfilnam =~ /_vm$/))
		{
			# Advance, and don't mark this file as an extra.
			$pstat = ($plin = <$pfh>);
			next;
		}

		# if filenames don't match, advance the lesser (and mark it as
		# an "extra")
		if ($pfilnam gt $mfilnam)
		{
			push @extrafiles, ["m:". $mfilnam, $mfilsiz];
			$mstat = ($mlin = <$mfh>);
			next;
		}
		elsif ($pfilnam lt $mfilnam)
		{
			push @extrafiles, ["p:". $pfilnam, $pfilsiz];
			$pstat = ($plin = <$pfh>);
			next;
		}

		# filenames match, so check file sizes
		
		if ($pfilsiz != $mfilsiz)
		{
			$sizediff{$pfilnam} = [$pfilsiz, $mfilsiz];

			# this is ok for ao tables -- need to check eof in db.
			# don't bother checking the hash -- they won't match, either

			goto L_dandy1;
		}

		# file sizes match, so check hash
		my $pfilhsh = pop @ppp;
		my $mfilhsh = pop @mmm;

		if ($pfilhsh eq $mfilhsh)
		{
			# uh oh -- why did we diff if filenames match, file sizes
			# match, and md5 hashes match??

			die ("bad hash funkiness: $plin\n$mlin");
		}

		# either a case of hint bits for heap tables, or aborted txns for ao
		$hashdiff{$pfilnam} = [$pfilhsh, $mfilhsh];		

	  L_dandy1:
		$pstat = ($plin = <$pfh>);
		$mstat = ($mlin = <$mfh>);
	}

	# pstat and mstat should both hit EOF at same time, otherwise have
	# extra files

	while ($pstat)
	{
		# check filenames
		my @ppp = split(/\s+/, $plin);

		warn ("bad line: $plin\nin file: $prim") unless (scalar(@ppp));

		my $pfilnam = shift @ppp;
		my $pfilsiz = shift @ppp;

		push @extrafiles, ["p:". $pfilnam, $pfilsiz];

		$pstat = ($plin = <$pfh>);
	}

	while ($mstat)
	{
		# check filenames
		my @mmm = split(/\s+/, $mlin);

		warn ("bad line: $mlin\nin file: $mirr") unless (scalar(@mmm));

		my $mfilnam = shift @mmm;
		my $mfilsiz = shift @mmm;

		push @extrafiles, ["m:". $mfilnam, $mfilsiz];

		$mstat = ($mlin = <$mfh>);
	}

  L_skipdandy:
	close $pfh;
	close $mfh;

	my %bigh = (extrafiles => \@extrafiles, sizediff => \%sizediff, 
				hashdiff => \%hashdiff, topdir => \@topdir);

	return \%bigh;
} # end dandydiff

sub get_relation_info_query_txt
{
	# MPP-9087: for shared (global) tables, we look for them in
	# template1 (which is dbid=1), but the prn.database_oid=0 (not 1)
	# for this case.

	my $bigstr = <<'EOF_bigstr';
with prn as (
  select gp_segment_id, * from gp_persistent_relation_node
  union all select gp_segment_id, * from gp_dist_random($q$gp_persistent_relation_node$q$)
),
pc as (
  select gp_segment_id, oid, * from pg_class
  union all select gp_segment_id, oid, * from gp_dist_random($q$pg_class$q$)
),
pt as (
  select gp_segment_id, oid, * from pg_tablespace
  union all select gp_segment_id, oid, * from gp_dist_random($q$pg_tablespace$q$)
),
pd as (
  select gp_segment_id, oid, * from pg_database
  union all select gp_segment_id, oid, * from gp_dist_random($q$pg_database$q$)
)
select distinct pt.spcname, prn.tablespace_oid, pd.datname, prn.database_oid, pc.relname, pc.oid as relid,
  prn.relfilenode_oid, pc.relfilenode, pc.relisshared, pc.relstorage, pc.reltablespace, pc.relkind, pc.relam
from prn, pc, pt, pd
where prn.tablespace_oid = pt.oid and prn.gp_segment_id = pt.gp_segment_id
  and (prn.database_oid = pd.oid or (prn.database_oid = 0 and pd.datname = $q$template1$q$)) and prn.gp_segment_id = pd.gp_segment_id
  and prn.relfilenode_oid = pc.relfilenode and prn.gp_segment_id = pc.gp_segment_id and pc.relfilenode in (
EOF_bigstr

	return $bigstr;
} # end get_relation_info_query_txt

sub get_pg_class_query_txt
{
	my $bigstr = <<'EOF_bigstr';
select
gp_segment_id,* from gp_dist_random($q$pg_class$q$)
where relfilenode in (
EOF_bigstr

	return $bigstr;
} # end get_pg_class_query_txt

sub get_prn_query_txt
{
	my $bigstr = <<'EOF_bigstr';
select
gp_segment_id,* from gp_dist_random($q$gp_persistent_relation_node$q$)
where relfilenode_oid in (
EOF_bigstr

	return $bigstr;
} # end get_prn_query_txt

# Ugh.  column store tables shove a vpinfo c struct directly into the
# catalog, so we have to jump thru hoops to unpack it.  The vpinfo has
# a variable length array, which encodes the eofs for the multiple
# "pseudo-SegNos" that correspond to the columns of the table.  So
# each aoseg entry is copied into a series of "pseudo_aoseg" entries, 1
# per column.
sub find_eof_in_vpinfo
{
	my $rnod = shift;

	my $AOTupleId_MultiplierSegmentFileNum = 128;  # Next up power of 2
												   # as multiplier.
	my $pseudo_aoseg = [];

	for my $rr (@{$rnod->{aoseg}})
	{
		my $biga;
		my $bigbuf;

		$biga = [];
		$bigbuf = '';

		my @foo = split(/(\\\d\d\d)/, $rr->{vpinfo});

		for my $cc (@foo)
		{
			# oh, did I neglect to mention that postgres returns the
			# bytea as a combination of octal values (3 digit strings
			# prefixed with a "\") and printable characters?  Well,
			# now I have mentioned it.  How convenient.  So we need to
			# translate the string back to raw byte values and build a
			# buffer to unpack.

			next unless length($cc);
			if ($cc =~ m/\\\d\d\d/)
			{
				$cc =~ s/^\\//;
				push @{$biga}, oct($cc);
				$bigbuf .= chr(oct($cc));
			}
			else
			{
				my @baz = split(//, $cc);

				for my $bb (@baz)
				{
					push @{$biga}, ord($bb);
				}
				$bigbuf .= $cc;
			}
		}
		# two 32 bit ints
		my @ppp = unpack("I2", $bigbuf);

		next unless (scalar(@ppp) > 1);

		# discard version
		shift @ppp;

		my $nentry = $ppp[0];

		# two 64 bit (I2) fields in each entry, so multiply by 4
		my $nentry2 = $nentry * 4;

		# 64 bit aligned, so we need another 32 bits of packing.
		$nentry2++;

		@ppp = unpack("I2 I$nentry2", $bigbuf);

		# discard version, nentry
		shift @ppp;
		shift @ppp;

		# and discard the packing...
		shift @ppp;

#		print "\nppp: \n",Data::Dumper->Dump(\@ppp);

		# the nentry corresponds to the number of columns in the table
		for my $column_num (0..($nentry-1))
		{
			my $new_entry;

			$new_entry = {
				'gp_segment_id' => $rr->{gp_segment_id},
				'varblockcount' => $rr->{varblockcount},
				'tupcount' => $rr->{tupcount}
			};

			# We unpack each 64-bit value as two 32-bit ints. 
			# Hope this works!
			$new_entry->{eof} = $ppp[4*$column_num] + 
				((2**32) * $ppp[(4*$column_num) + 1] );
			$new_entry->{eofuncompressed} = $ppp[(4*$column_num) + 2] +
				((2**32) * $ppp[(4*$column_num) + 3] );

			# such fun! a normal ao table has a relfilenode composed
			# of multiple segments, which have corresponding files
			# named "relfilenode.segno".  But a co table has multiple
			# "pseudo" segments, one per column, where:
			#
			#    pseudosegno = (colno*multiplier) + segno
			#
			# having colno=(0..(n-1)), multiplier=128
			# ie we only support 128 concurrent users, since 
			# col0 is pseudosegs 1-128
			# col1 is pseudosegs 129-256
			# col2 is pseudosegs 257-384, etc.

			my $pseudoSegNo = 
				($column_num*$AOTupleId_MultiplierSegmentFileNum) 
				+ $rr->{segno};

			$new_entry->{segno} = $pseudoSegNo;

			push @{$pseudo_aoseg}, $new_entry;
		}
	} # end for rr
	$rnod->{pseudo_aoseg} = $pseudo_aoseg;
} # end find_eof_in_vpinfo

sub get_aoseg_func
{
	my ($relid, $rnod, $dbname, $diffi, $row, $fselochsh, $bigh) = @_;
	my $storage_class = $rnod->{relstorage};
	my $psql_str = $glob_psql_str;
	$psql_str .= "-p $glob_port " . $dbname;

	# MPP-10866: segname construction is different for ALTER TABLE and
	# TRUNCATE, so get the segname from pg_appendonly
	$psql_str .= " -c \' " .
		"select relname,oid  from pg_class where oid in ( " .
		"select segrelid from pg_appendonly " . 
		"where relid = " . $relid . " ) " .
		"\' ";

	my $tabdef = `$psql_str`;
	my $segname = "unknown_segname";
	my $segs = tablelizer($tabdef);

	for my $rr (@{$segs})
	{
		if (exists($rr->{relname}))
		{
			$segname = $rr->{relname};
			last;
		}
	}

	$psql_str = $glob_psql_str;
	$psql_str .= "-p $glob_port " . $dbname;

	# select from gp_persistent_relation_node, pg_class, etc

	# use "dollar-quoting" to avoid dealing with nested single-quotes:
	# $q$p$q$ is equivalent to 'p'
	$psql_str .= " -c \' " .
		"select gp_segment_id,* from gp_dist_random(" . '$q$' .
		"pg_aoseg.". $segname . '$q$' . ") " .
		# filter by content/segment_id (which is identical for primary
		# and mirror, ie not the dbid)
		"where gp_segment_id = " . $row->{content} .
		"\' ";

	# for column store, will extract the eofs from vpinfo in
	# pg_aocsseg_XXX

	$tabdef = `$psql_str`;
	my $ao_seg = tablelizer($tabdef);
	return $ao_seg;
}

# helper function for resolve_sizediff, resolve_hashdiff
sub get_relfilenode_func
{
	my ($h1, $diffi, $row, $fselochsh, $bigh, $bstat_ref) = @_;

	my @relfnods;
	my %dbs;

	# list of filenames for hash or size diff
	for my $spec (keys(%{$h1}))
	{
		# spec like: /base/10892/139321.2'

		my @foo = split(/\//, $spec);

		die "bad spec: $spec" unless scalar(@foo);

		my $fil			= pop @foo;
		my $db			= pop @foo; # normally a dbid, but could be "global";
		my $relfilenode = $fil;

		# for a filespace, $tspace is the oid of the tablespace, but
		# otherwise it should be "base" or "global"
		my $tspace = "no_tablespace";

		$tspace = pop @foo
			if (scalar(@foo));

		$tspace = "global" if ($db =~ m/^global$/i);

		$relfilenode =~ s/\.\d+$//gm; # remove the trailing ".number"...

		$dbs{$db} = {relfilenodeh=>{}} unless exists($dbs{$db});

		unless (exists($dbs{$db}->{relfilenodeh}->{$relfilenode}))
		{
			$dbs{$db}->{relfilenodeh}->{$relfilenode} = 
			{ files => [], tspace => {} } ;

			# Build a list of relfilenodes to lookup in catalog.
			# Note: filter out non-numeric names like "pg_auth"
			if ($relfilenode =~ m/^\d+$/)
			{
				# MPP-11263: don't look up long names as relfilenodes
				# or the query will fail with "OID out of range"
				if (length($relfilenode) > 15)
				{
					warn "relfilenode too long: $relfilenode in db: $db";
				}
				else
				{
					push @relfnods, $relfilenode;
				}
			}
		}
		
		push @{$dbs{$db}->{relfilenodeh}->{$relfilenode}->{files}}, $fil;
		$dbs{$db}->{relfilenodeh}->{$relfilenode}->{tspace}->{$tspace} = 1;
	}

	my $psql_str = $glob_psql_str;
	$psql_str .= $glob_connect
	if (defined($glob_connect));

	# read from all db's, but separate out "connectable ones later...
	$psql_str .= " -c \' " . 
		"select oid, datname, datallowconn from pg_database  \' ";

	my $tabdef = `$psql_str`;
	chomp($tabdef);
	debug_print("DATABASES (with connect state):");
	debug_print($tabdef);
	my $dbidinfo = tablelizer($tabdef);

	# Note: treat all global objects as belonging to template1 (dbid=1)
	# but really have to find them in "database_oid"=0
	if (exists($dbs{global}))
	{
		$dbs{global}->{dbname} = "template1";
	}

	# find the dbnames for the dbids
	for my $rr (@{$dbidinfo})
	{
		if (exists($dbs{$rr->{oid}}))
		{
			if ($rr->{datallowconn} eq 't')
			{
				# normal case -- database allows connections
				$dbs{$rr->{oid}}->{dbname} = $rr->{datname};
			}
			else # cannot connect
			{
				# handle template0 - treat as template1
				if ($rr->{datname} eq "template0")
				{
					print "cannot connect to template0 (oid $rr->{oid}) - ",
					"will resolve relfiles against template1 instead\n",
					Data::Dumper->Dump([$dbs{$rr->{oid}}]);
					$dbs{$rr->{oid}}->{dbname} = "template1";
				}
			}
		}
	}

	for my $dbkey (keys(%dbs))
	{
		next
			unless (exists($dbs{$dbkey}->{dbname}));
		next
			unless (scalar(@relfnods)); # MPP-9689: fix syntax error 

		$psql_str = $glob_psql_str;
		$psql_str .= "-p $glob_port " . $dbs{$dbkey}->{dbname};
		# select from gp_persistent_relation_node, pg_class, etc
		$psql_str .= " -c \' ". get_relation_info_query_txt()
			. join (", ", @relfnods) . ") and pc.gp_segment_id = " . $row->{content} . " \'";

		print "Fetching the details from pg_class and gp_relation_node for $dbs{$dbkey}->{dbname} \n";
		debug_print($psql_str);

		$tabdef = `$psql_str`;
		chomp($tabdef);

		printdivider();
		print $tabdef;
		printdivider();

		my $gp_prn = tablelizer($tabdef);

		for my $rr2 (@{$gp_prn})
		{
			if (exists($dbs{$dbkey}->{relfilenodeh}->{$rr2->{relfilenode}}))
			{
				my $rnod = $dbs{$dbkey}->{relfilenodeh}->{$rr2->{relfilenode}};

				unless (exists($rnod->{tspace_match}))
				{
					$rnod->{relname}	   = $rr2->{relname};
					$rnod->{relid}		   = $rr2->{relid};
					$rnod->{relisshared}   = $rr2->{relisshared};
					$rnod->{relstorage}	   = $rr2->{relstorage};
					$rnod->{relkind}	   = $rr2->{relkind};
					$rnod->{reltablespace} = $rr2->{reltablespace};

					# Note: reltablespace is 0 for pg_default (vs 1663),
					# so get tablespace_oid too
					
					$rnod->{tablespace_oid} = $rr2->{tablespace_oid};
					$rnod->{database_oid}	= $rr2->{database_oid};

					# and index access method
					$rnod->{relam}		    = $rr2->{relam};

					if (exists($rnod->{tspace}))
					{
						my $tspoid = $rnod->{tablespace_oid};

						if (exists($rnod->{tspace}->{$tspoid}))
						{
							$rnod->{tspace_match} = 1;
						}
						else
						{
							if (exists($rnod->{tspace}->{base})
								&& ($DEFAULTTABLESPACE_OID == $tspoid))
							{
								$rnod->{tspace_match} = 1;
							}
							elsif (exists($rnod->{tspace}->{global})
								   && ($GLOBALTABLESPACE_OID == $tspoid))
							{
								$rnod->{tspace_match} = 1;
							}
						}
					} # end if exists rnod->tspace
				} # end unless exists tspace_match
			} # end if exists dbs dbkey
		} # end for my rr2
	}

	# sanity check
	my $bstat = 1;

	my %unknown;

	# for all db's
	while (my ($kk, $vv) = each(%dbs))	
	{
		my $dbname;
		if (exists($vv->{dbname}))
		{
			$dbname = $vv->{dbname};
		}
		else
		{
			$bstat = 0;
			$dbname = "dbid: $kk";
		}

		# for all relfilenodes
		while (my ($kk2, $vv2) = each(%{$vv->{relfilenodeh}}))
		{
			my $rnod = $vv2;
			my $relname;

			if (exists($rnod->{relname}))
			{
				$relname = $rnod->{relname};
			}
			else
			{
				$bstat = 0;
				$relname = "relfilenode: $kk2";
				$unknown{$kk} = []
					unless (exists($unknown{$kk}));
				push @{$unknown{$kk}}, $kk2;

				next;
			}

			unless (exists($rnod->{relkind})
					&& ($rnod->{relkind} =~ m/^(r|o|t|i|b|m|S)$/))
			{
				$bstat = 0;
				print Data::Dumper->Dump([$vv2]),"\n";
				warn "$relname for $dbname not of kind relation (r), toast (t), aoseg (o), or index (i)";
				next;
			}

			# ao, heap, columnstore
			unless (exists($rnod->{relstorage})
					&& ($rnod->{relstorage} =~ m/^(a|h|c)$/))
			{
				$bstat = 0;
				print Data::Dumper->Dump([$vv2]),"\n";
				warn "$relname for $dbname storage is not ao or heap";
			}
			# treat column-oriented same as ao
			if (exists($rnod->{relstorage})
					&& ($rnod->{relstorage} =~ m/a|c/))
			{
				# MPP-10866: aoseg/aocsseg naming is inconsistent
				$rnod->{aoseg} = 
					get_aoseg_func($rnod->{relid}, $rnod, $dbname,
								   $diffi, $row, $fselochsh, $bigh);

				$rnod->{aoseg_eof} = {};

				# build hash by segno (ie file.segno) of eof lengths
				if ($rnod->{relstorage} eq "a")
				{
					for my $rr2 (@{$rnod->{aoseg}})
					{
						$rnod->{aoseg_eof}->{$rr2->{segno}} = $rr2->{eof};
					}
				}
				else # column store
				{
					# unpack the list of pseudo-segments from the vpinfo
					find_eof_in_vpinfo($rnod);
					for my $rr2 (@{$rnod->{pseudo_aoseg}})
					{
						$rnod->{aoseg_eof}->{$rr2->{segno}} = $rr2->{eof};
					}
				}
			}

			# MPP-10123: validate tablespace in filesystem and catalog
			if (scalar(keys(%{$rnod->{tspace}})) > 1)
			{
				$unknown{$kk} = []
					unless (exists($unknown{$kk}));
				push @{$unknown{$kk}}, $kk2;
			}
			elsif ($rnod->{tablespace_oid})
			{
				my $nomatch = 0;
				my @tskk = sort(keys(%{$rnod->{tspace}}));
				my $tspace1 = $tskk[0]; # should only be one

				unless (defined($tspace1))
				{
					$tspace1 = "no tablespace";
					$nomatch = 1;
				}

				if ($tspace1 =~ m/base/)
				{
					$nomatch = 
						($DEFAULTTABLESPACE_OID != $rnod->{tablespace_oid});
				}
				elsif ($tspace1 =~ m/global/)
				{
					$nomatch = 
						($GLOBALTABLESPACE_OID != $rnod->{tablespace_oid});
				}
				elsif ($tspace1 =~ m/\d+/)
				{
					$nomatch = ($tspace1 != $rnod->{tablespace_oid});
				}
				# else ignore no tablespace case for now
				if ($nomatch)
				{
					warn "relfilenode $kk2 found in wrong tablespace!!";
					warn "$tspace1 vs $rnod->{tablespace_oid}";
					$unknown{$kk} = []
						unless (exists($unknown{$kk}));
					push @{$unknown{$kk}}, $kk2;
				}
			}
		} # end for all relfilenodes
	} # end for all db's
		
	# check for unknown relfilenodes separately across pg_class and
	# gp_persistent_relation_node on all segments
	if (scalar(keys(%unknown)))
	{
		while (my ($ukk, $uvv) = each(%unknown))
		{
			my $relfnod2;
			my $bad_relfnod;

			# see if have connectable database first...
			if (exists($dbs{$ukk}) && exists($dbs{$ukk}->{dbname}))
			{
#				print "Checking db: ", $dbs{$ukk}->{dbname}, "\n";
			}
			else
			{
				$bstat = 0;
				print "found ", scalar(@{$uvv}),
				" relfilenodes in unknown database dbid: ", $ukk, 
				" -- no lookup possible!\n";
				next;
			}

			$relfnod2 = []; # MPP-9689: fix syntax error 
			$bad_relfnod = [];

			for my $rnod (@{$uvv})
			{
				if ($rnod =~ m/^\d+$/)
				{
					push @{$relfnod2}, $rnod;
				}
				else
				{
					push @{$bad_relfnod}, $rnod;
				}
			}
			
			if (scalar(@{$bad_relfnod}))
			{
				$bstat = 0;
				my $str1 = "invalid relfilenodes: " . 
					join(", ", @{$bad_relfnod}) . 
					"\n in db -- no lookup possible";
				warn $str1;
			}

			next
				unless (scalar(@{$relfnod2}));

			$psql_str = $glob_psql_str;
			$psql_str .= "-p $glob_port " . $dbs{$ukk}->{dbname};
			# select from gp_persistent_relation_node, pg_class, etc
			$psql_str .= " -c \' ". get_pg_class_query_txt()
				. join (", ", @{$relfnod2}) . ") \'";


			$tabdef = `$psql_str`;

			if ($tabdef !~ m/\(0 rows\)/)
			{
#				print $tabdef;
			}
			else
			{
				$bstat = 0;
				print "no pg_class entries for (",
				join (", ", @{$relfnod2}) . ")\n";
			}

			$psql_str = $glob_psql_str;
			$psql_str .= "-p $glob_port " . $dbs{$ukk}->{dbname};
			# select from gp_persistent_relation_node, pg_class, etc
			$psql_str .= " -c \' ". get_prn_query_txt()
				. join (", ", @{$relfnod2}) . ") \'";

			$tabdef = `$psql_str`;
			if ($tabdef !~ m/\(0 rows\)/)
			{
#				print $tabdef;
			}
			else
			{
				$bstat = 0;
				print "no gp_persistent_relation_node entries for (",
				join (", ", @{$relfnod2}) . ")\n";
			}
		}
	} # end if unknowns

	my $do_anyway = 1;
	unless ($do_anyway || $bstat)
	{
		print Data::Dumper->Dump([%dbs]);
		print $diffi;
		print Data::Dumper->Dump([$row]);
		print Data::Dumper->Dump([$bigh]);	 
		die "too many failures resolving relfilenodes";
		exit(1);
	}

	# XXX XXX: return status in array (for extra files check)
	if (!($bstat) && defined($bstat_ref))
	{
		push @{$bstat_ref}, 1;
	}

	return \%dbs;
} # end get_relfilenode_func

sub resolve_sizediff
{
	my ($diffi, $row, $fselochsh, $bigh) = @_;
	my $dbs = 
		get_relfilenode_func($bigh->{sizediff},
							 $diffi, $row, $fselochsh, $bigh);
	my $bstat = 1;

	# for all db's
	while (my ($kk, $vv) = each(%{$dbs}))	
	{
		my $dbname = exists($vv->{dbname}) ? $vv->{dbname} :
			"<unknown dbname: ?>";

		# for all relfilenodes
		while (my ($kk2, $vv2) = each(%{$vv->{relfilenodeh}}))
		{
			my $rnod = $vv2;
			my $relname = exists($rnod->{relname}) ? $rnod->{relname} : 
				"<unknown relname: $kk2>";

			unless (exists($rnod->{relstorage})
					&& ($rnod->{relstorage} =~ m/^(a|c)$/))
			{
				$bstat = 0;
				print Data::Dumper->Dump([$vv2]),"\n";
				warn "bad sizediff for non-ao file $relname for db $dbname (but could be ok - see block by block results)";
			}
		} # end for all relfilenodes
	} # end for all db's

	my $do_anyway = 1;

	unless ($do_anyway || $bstat)
	{
		print Data::Dumper->Dump([$dbs]);
		print $diffi;
		print Data::Dumper->Dump([$row]);
		print Data::Dumper->Dump([$bigh]);	 
		die "found size differences in non-ao files";
		exit(1);
	}
	return $dbs;
} # end resolve_sizediff

sub resolve_hashdiff
{
	my ($diffi, $row, $fselochsh, $bigh) = @_;
	my $dbs = 
		get_relfilenode_func($bigh->{hashdiff},
							 $diffi, $row, $fselochsh, $bigh);
	return $dbs;
} # end resolve_hashdiff

sub extra_files_check_for_zero_length_heap
{
	my ($dbs3,  
		$diffi, $row, $fselochsh, $bigh,
		$primfil, $mirrfil, $primhost, $mirrhost, 
		$primtmp, $mirrtmp,
		$ppresh2, $mpresh2,
		$primsh, $mirrsh) = @_;

	my $bstat = 1;

	# MPP-11639: 
	print "extra files: check for zero-length heap files\n";

	for my $e2 (@{($bigh->{extrafiles})})
	{
		my $extry;
		my $elen;
		
		$extry = "" . $e2->[0];
		$elen = $e2->[1];

		if ($elen !~ /\d+/)
		{
			warn "bad length $elen for extra file $extry";
			$bstat = 0;
			next; # keep going
		}
		if ($elen > 0)
		{
			warn "extra file $extry has non-zero length $elen";
			# if extra file has non-zero length always complain
			$bstat = 0;
			next; # keep going
		}

		# if extra file has zero length, then only complain for ao/co
		# or non-index heap

		$extry =~ s|^m\:(/)?||i;
		$extry =~ s|^p\:(/)?||i;
		$extry =~ s|\.\d+$||i;

		if ($extry =~ m|/|)
		{
			my @baz = split(/\//, $extry);
			$extry = pop (@baz);
		}

		while (my ($kk, $vv) = each(%{$dbs3}))	
		{
			my $dbname = $vv->{dbname};
			$dbname = "<unknown dbname>" 
				unless (defined($dbname) && length($dbname));

			# check if "extra" file exists in this database
			my $kk2 = $extry;
			next unless (exists($vv->{relfilenodeh}) &&
						 exists($vv->{relfilenodeh}->{$kk2}));
			my $vv2 = $vv->{relfilenodeh}->{$kk2};
			my $rnod = $vv2;
			my $relname = exists($rnod->{relname}) ? $rnod->{relname} : 
				"<unknown relname: $kk2>";
			if ($rnod->{relstorage} ne "h")
			{
				warn "extra file $e2->[0] for relation $relname in db $dbname is storage class: " . $rnod->{relstorage} . ", not heap";
				# if extra file is zero length but not heap, complain
				$bstat = 0;
				last; # keep going
			}
			if ($rnod->{relkind} eq "i")
			{
				warn "extra file $e2->[0] for relation $relname in db $dbname is index";
				# if extra file is zero length and is heap, but is an
				# index, complain
				$bstat = 0;
				last; # keep going
			}
		} # end while kk
	} # end while e2

	if ($bstat)
	{
		print "extra files: no differences\n";
	}
	else
	{
		my $floc = $row->{ploc};

		# found differences
		print "\nextra files [$floc]: found differences\n\n";
		push @{$glob_exit}, $row->{ploc}." ".$row->{mloc}; 
		return (0);
	}

	return (1);
} # end extra_files_check_for_zero_length_heap

sub copyfiles
{
	my ($allfilescopy, $primhost, $mirrhost, $primfil, $mirrfil, $primtmp, $mirrtmp) = @_;
	my $cmd_template;
	my $primsh = $primtmp . "_copy.sh";
	my $mirrsh = $mirrtmp . "_copy.sh";
	my $dir;

	return undef unless scalar(@{$allfilescopy});

	for my $file (@{$allfilescopy})
	{
		$dir = "MYDIRTEMP_data/" . dirname($file);
		$cmd_template .= "mkdir -p $dir; cp MYDIRDATA/$file $dir/;\n"
	}

	my $cmd = $cmd_template;
	$cmd =~ s/MYDIRTEMP/$primtmp/g;
	$cmd =~ s/MYDIRDATA/$primfil/g;

	my $fh;
	open ($fh, '>', $primsh);
	print $fh $cmd, "\n";
	close $fh;

	$cmd = $cmd_template;
	$cmd =~ s/MYDIRTEMP/$mirrtmp/g;
	$cmd =~ s/MYDIRDATA/$mirrfil/g;

	open ($fh, '>', $mirrsh);
	print $fh $cmd, "\n";
	close $fh;

	# copy the shell scripts to the segments, execute them, then
	# copy back the results. Note that we add "1" as a suffix to
	# the script name and output file name to handle the case
	# where the master and segment are on the same machine (useful
	# for testing)
	my $cmd_template2 = 
			'gpscp -h MYHOST MYSHSCRIPT =:MYSHSCRIPT1 ; ' .
			'gpssh -h MYHOST \' sh MYSHSCRIPT1 \' > /dev/null 2>&1;';

	$cmd = $cmd_template2;
	$cmd =~ s/MYHOST/$primhost/g;
	$cmd =~ s/MYSHSCRIPT/$primsh/g;

	system ($cmd);

	$cmd = $cmd_template2;
	$cmd =~ s/MYHOST/$mirrhost/g;
	$cmd =~ s/MYSHSCRIPT/$mirrsh/g;

	system ($cmd);
}

sub diff_again
{
	my ($dbs1, $dbs2,
		$diffi, $row, $fselochsh, $bigh,
		$primfil, $mirrfil, $primhost, $mirrhost, 
		$primtmp, $mirrtmp,
		$ppresh2, $mpresh2,
		$primsh, $mirrsh) = @_;

	my $bstat = 1;
	my (@aofiles, @heapfiles, @btreefiles, @unknownfiles, @otheridx);

	# MPP-10021: differentiate index types by access method.  Build a
	# copy of pg_am
	my %pg_am = ( 403 => "btree", 405 => "hash", 783 => "gist",
				  2742 => "gin", 3013 => "bitmap" 
		);

	for my $dbs ($dbs1, $dbs2)
	{
		next unless ($dbs);

		while (my ($kk, $vv) = each(%{$dbs}))	
		{
			my $dbname = $vv->{dbname};

			$dbname = "<unknown dbname>" 
				unless (defined($dbname) && length($dbname));

			# for all relfilenodes
			while (my ($kk2, $vv2) = each(%{$vv->{relfilenodeh}}))
			{
				my $rnod = $vv2;
				my $relname = exists($rnod->{relname}) ? $rnod->{relname} : 
					"<unknown relname: $kk2>";

				{
					my @filpref;
					
					# MPP-9415: use "base" or tablespace oid in directory name
					push @filpref, "base" 
						unless ((not looks_like_number($kk))
								# MPP-9415: no "base" for other filespaces
								|| ($row->{fsoid} != $DEFAULTFILESPACE_OID)); 
					                                 # pg_system
					
					# MPP-9415: and use the tablespace oid in the name
					# if not in pg_system filespace
					#
					# MPP-10123: use gprn.tablespace_oid vs
					# pg_class.reltablespace, which is 0 for pg_default
					push @filpref, $rnod->{tablespace_oid}
						if (($row->{fsoid} != $DEFAULTFILESPACE_OID)
							&& exists($rnod->{tablespace_oid}));

					push @filpref, $kk; # dbid

					# build full path to filename
					for my $f1 (@{$rnod->{files}})
					{
						my $filnam = join("/", @filpref) . "/" . $f1;

						if (exists($rnod->{relstorage})
							&& ($rnod->{relstorage} =~ m/h|a|c/))
						{
							if ($rnod->{relstorage} eq "h")
							{
								if ($rnod->{relkind} eq "i")
								{
									# MPP-10021: differentiate index
									# types by access method.  Treat
									# btree special
									if (403 == $rnod->{relam})
									{
										push @btreefiles, $filnam;
									}
									else
									{
										my $idx_type = "unknown_index";

										$idx_type = $pg_am{$rnod->{relam}}
										if (exists($pg_am{$rnod->{relam}}));

										push @otheridx, 
										$idx_type . ":" . $filnam;
									}

								}
								else
								{
									push @heapfiles, $filnam;
								}
							}
							elsif ($rnod->{relstorage} =~ m/a|c/)
							{
								my @foo = split(/\./, $filnam);
								
								die "bad fil no segno" unless scalar(@foo);

								my $segno = pop @foo;
								my $eof;

								die "no eof for segno $segno" 
								unless (exists($rnod->{aoseg_eof}->{$segno}));

								$eof = $rnod->{aoseg_eof}->{$segno};
								
								push @aofiles, {file => $filnam, eof=> $eof};

							}
						}
						else 
						{
							my $s1 = exists($rnod->{relstorage}) ? 
								$rnod->{relstorage} : "<unknown>";
							push @unknownfiles, $filnam;
						}
					}
				}
			} # end for all relfilenodes
		} # end for all db's
	}

	my (@allfiles1, @allfiles2);
	my (@allfilescopy);

	for my $fil (@aofiles)
	{
		my $eof = $fil->{eof};

		# calculate hash for files, but prefix hash with filename
		push @allfiles1, 
		$fil->{file} ."  ( perl MYPRESH21 -noadler -ao  $eof  MYDIR/" . $fil->{file}
		. " | openssl dgst -md5 -hex ) 2>&1  "
		# should be: perl -e 'for (<STDIN>) { print "filename: ", $_ ;}'
		. "| perl -e 'for (<STDIN>) { print \"" . $fil->{file} . 
		':ao: ", $_ ;}' . "'";
	}
	for my $fil (@heapfiles)
	{
		push @allfiles1, 
		$fil . "  ( perl MYPRESH21 -noadler -heap  MYDIR/" . $fil
		. " | openssl dgst -md5 -hex ) 2>&1  "
		# should be: perl -e 'for (<STDIN>) { print "filename: ", $_ ;}'
		. "| perl -e 'for (<STDIN>) { print \"" . $fil . 
		':heap: ", $_ ;}' . "'";
	}
	for my $fil (@btreefiles)
	{
		push @allfiles1, 
		$fil . "  ( perl MYPRESH21 -noadler -btree  MYDIR/" . $fil
		. " | openssl dgst -md5 -hex ) 2>&1  "
		# should be: perl -e 'for (<STDIN>) { print "filename: ", $_ ;}'
		. "| perl -e 'for (<STDIN>) { print \"" . $fil . 
		':btree: ", $_ ;}' . "'";
	}
	for my $fil (@unknownfiles)
	{
		print "raw diff:\n";
		debug_print($diffi);
		# found differences
		push @{$glob_exit}, $row->{ploc}." ".$row->{mloc} ;

		push @{$glob_fixlist}, "unknown: " . $row->{paddress} . ":" . 
			File::Spec->catfile($primfil, $fil) . " " .
			$row->{maddress} . ":" . 
			File::Spec->catfile($mirrfil, $fil);
		push @allfilescopy, $fil;
	}
	for my $fil2 (@otheridx)
	{
		my @ttt = split(/:/, $fil2, 2);

		my $idx_type = shift @ttt;
		my $fil = shift @ttt;

		push @allfiles1, 
		$fil . "  ( cat MYDIR/" . $fil
		. " | openssl dgst -md5 -hex ) 2>&1  "
		# should be: perl -e 'for (<STDIN>) { print "filename: ", $_ ;}'
		. "| perl -e 'for (<STDIN>) { print \"" . $fil . 
		':' . $idx_type . ': ", $_ ;}' . "'";
	}

	# need filename to sort correctly
	@allfiles2 = sort @allfiles1;
	@allfiles1 = ();

	# remove the filename sort key, leaving command string
	for my $fff (@allfiles2)
	{
		my @zzz = split(/\s/, $fff, 2);
		shift @zzz;
		push @allfiles1, $zzz[0];
	}

	my $numfiles = scalar(@allfiles1);
	my $floc = $row->{ploc};

	copyfiles(\@allfilescopy, $primhost, $mirrhost, $primfil, $mirrfil, $primtmp, $mirrtmp);

	return undef unless ($numfiles);
	print "\nGoing for 2nd Phase enhanced diff (using storage-specific techniques) $numfiles files for $floc\n";
	# checkpoint the database to flush pending writes
	my $psql_str = $glob_psql_str;
	$psql_str .= $glob_connect
	if (defined($glob_connect));
	$psql_str .= " -c \' checkpoint \'" ;
	my $tabdef = `$psql_str`;

	$primsh .= "_deux";
	$mirrsh .= "_deux";
	$primtmp .= "_deux";
	$mirrtmp .= "_deux";

	# use openssl to construct an md5 hash for every file
	my $cmd_template = join ("\n", @allfiles1);
	my $cmd = $cmd_template;
	$cmd =~ s/MYHOST/$primhost/g;
	$cmd =~ s/MYDIR/$primfil/g;
	$cmd =~ s/MYOUT/$primtmp/g;
	$cmd =~ s/MYPRESH2/$ppresh2/g;

	my $fh;
	open ($fh, '>', $primsh);
	print $fh $cmd, "\n";
	close $fh;

	$cmd = $cmd_template;
	$cmd =~ s/MYHOST/$mirrhost/g;
	$cmd =~ s/MYDIR/$mirrfil/g;
	$cmd =~ s/MYOUT/$mirrtmp/g;
	$cmd =~ s/MYPRESH2/$mpresh2/g;

	open ($fh, '>', $mirrsh);
	print $fh $cmd, "\n";
	close $fh;

	# copy the shell scripts to the segments, execute them, then
	# copy back the results. Note that we add "1" as a suffix to
	# the script name and output file name to handle the case
	# where the master and segment are on the same machine (useful
	# for testing)
	my $cmd_template2 = 
			'gpscp -h MYHOST MYSHSCRIPT =:MYSHSCRIPT1 ; ' .
			'gpssh -h MYHOST \' sh MYSHSCRIPT1 > MYOUT  \' > /dev/null 2>&1; ' .
			'gpscp -h MYHOST  =:MYOUT MYOUT1 ; ' ;

	$cmd = $cmd_template2;
	$cmd =~ s/MYHOST/$primhost/g;
	$cmd =~ s/MYDIR/$primfil/g;
	$cmd =~ s/MYOUT/$primtmp/g;
	$cmd =~ s/MYSHSCRIPT/$primsh/g;

	system ($cmd);

	$cmd = $cmd_template2;
	$cmd =~ s/MYHOST/$mirrhost/g;
	$cmd =~ s/MYDIR/$mirrfil/g;
	$cmd =~ s/MYOUT/$mirrtmp/g;
	$cmd =~ s/MYSHSCRIPT/$mirrsh/g;

	system ($cmd);

	# add "1" suffix on master
	$primtmp .= '1';
	$mirrtmp .= '1';

	my $diffi2 = `$glob_diff -b $primtmp $mirrtmp`;

	if (length($diffi2))
	{
		print "Enhanced diff: 			[ 2nd PHASE ALSO FOUND DIFFERENCES ] \n";

		my $fdiff = $diffi2;

		$fdiff =~ s/\:ao\:\s+.*/:ao:/gm;
		$fdiff =~ s/\:heap\:\s+.*/:heap:/gm;
		$fdiff =~ s/\:btree\:\s+.*/:btree:/gm;
		$fdiff =~ s/\:unknown\:\s+.*/:unknown:/gm;
		$fdiff =~ s/^\>\s+//gm;
		$fdiff =~ s/^\<\s+//gm;

		my %thrice;
		my @foo = split(/\n/, $fdiff);

		for my $ff (@foo)
		{
			next
				unless ($ff =~ m/\:ao\:|\:heap\:|\:btree\:/);
			$thrice{$ff} = 1;
		}
		print "\nWill be performing block by block diff: ", join("\n", sort(keys(%thrice))), "\n";

		diff_thrice($dbs1, $dbs2,
					$diffi, $diffi2, $row, $fselochsh, $bigh,
					$primfil, $mirrfil, $primhost, $mirrhost, 
					$primtmp, $mirrtmp, 
					$ppresh2, $mpresh2,
					$primsh, $mirrsh,
					\@aofiles, \@heapfiles, 
					\@btreefiles,
					\@unknownfiles,
					\@otheridx
			);
	}
	else
	{
		print "\nEnhanced diff \"$floc\" [ NO DIFFERENCES ]\n\n";
	}
} # end diff_again

# MPP-8555: block by block diff 
sub diff_thrice
{
	my ($dbs1, $dbs2,
		$diffi, $diffi2, $row, $fselochsh, $bigh,
		$primfil, $mirrfil, $primhost, $mirrhost, 
		$primtmp, $mirrtmp,
		$ppresh2, $mpresh2,
		$primsh, $mirrsh, $aofiles, $heapfiles, 
		$btreefiles,
		$unknownfiles,
		$otheridx) = @_;

	my $bstat = 1;

	my (@allfiles1, @allfiles2);
	my @allfilescopy;

	for my $fil (@{$aofiles})
	{
		my $eof = $fil->{eof};

		# calculate hash for files, but prefix hash with filename
		push @allfiles1, 
		$fil->{file} ."  ( perl MYPRESH21 -adler -ao  $eof  MYDIR/" . $fil->{file}
		. " ) 2>&1  "
		# should be: perl -e 'for (<STDIN>) { print "filename: ", $_ ;}'
		. "| perl -e 'for (<STDIN>) { print \"" . $fil->{file} . 
		':ao: ", $_ ;}' . "'";

		push @{$glob_fixlist}, "ao: " . $row->{paddress} . ":" . 
			File::Spec->catfile($primfil, $fil->{file}) . " " .
			$row->{maddress} . ":" . 
			File::Spec->catfile($mirrfil, $fil->{file});
		push @allfilescopy, $fil;	
	}
	for my $fil (@{$heapfiles})
	{
		push @allfiles1, 
		$fil . "  ( perl MYPRESH21 -adler -heap  MYDIR/" . $fil
		. " ) 2>&1  "
		# should be: perl -e 'for (<STDIN>) { print "filename: ", $_ ;}'
		. "| perl -e 'for (<STDIN>) { print \"" . $fil . 
		':heap: ", $_ ;}' . "'";

		push @{$glob_fixlist}, "heap: " . $row->{paddress} . ":" .
			File::Spec->catfile($primfil, $fil) . " " .
			$row->{maddress} . ":" . 
			File::Spec->catfile($mirrfil, $fil);
		push @allfilescopy, $fil;	
	}
	for my $fil (@{$btreefiles})
	{
		push @allfiles1, 
		$fil . "  ( perl MYPRESH21 -adler -btree  MYDIR/" . $fil
		. " ) 2>&1  "
		# should be: perl -e 'for (<STDIN>) { print "filename: ", $_ ;}'
		. "| perl -e 'for (<STDIN>) { print \"" . $fil . 
		':btree: ", $_ ;}' . "'";

		push @{$glob_fixlist}, "btree: " . $row->{paddress} . ":" .
			File::Spec->catfile($primfil, $fil) . " " .
			$row->{maddress} . ":" . 
			File::Spec->catfile($mirrfil, $fil);
		push @allfilescopy, $fil;	
	}
	for my $fil2 (@{$otheridx})
	{
		my @ttt = split(/:/, $fil2, 2);

		my $idx_type = shift @ttt;
		my $fil = shift @ttt;

		push @allfiles1, 
		# XXX XXX: use "-adler -other" to do block by block
		$fil . "  ( perl MYPRESH21 -adler -other MYDIR/" . $fil
		. " ) 2>&1  "
		# should be: perl -e 'for (<STDIN>) { print "filename: ", $_ ;}'
		. "| perl -e 'for (<STDIN>) { print \"" . $fil . 
		':' . $idx_type . ': ", $_ ;}' . "'";

		push @{$glob_fixlist}, "$idx_type: " . $row->{paddress} . ":" . 
			File::Spec->catfile($primfil, $fil) . " " .
			$row->{maddress} . ":" . 
			File::Spec->catfile($mirrfil, $fil);
		push @allfilescopy, $fil;	
	}

	# need filename to sort correctly
	@allfiles2 = sort @allfiles1;
	@allfiles1 = ();

	# remove the filename sort key, leaving command string
	for my $fff (@allfiles2)
	{
		my @zzz = split(/\s/, $fff, 2);
		shift @zzz;
		push @allfiles1, $zzz[0];
	}

	my $numfiles = scalar(@allfiles1);
	my $floc = $row->{ploc};

	print "Block by block diff: $numfiles files for $floc\n";

	$primsh  =~ s/deux/trois/;
	$mirrsh  =~ s/deux/trois/;
	$primtmp =~ s/deux/trois/;
	$mirrtmp =~ s/deux/trois/;

	# use openssl to construct an md5 hash for every file
	my $cmd_template = join ("\n", @allfiles1);
	my $cmd = $cmd_template;
	$cmd =~ s/MYHOST/$primhost/g;
	$cmd =~ s/MYDIR/$primfil/g;
	$cmd =~ s/MYOUT/$primtmp/g;
	$cmd =~ s/MYPRESH2/$ppresh2/g;

	my $fh;
	open ($fh, '>', $primsh);
	print $fh $cmd, "\n";
	close $fh;

	$cmd = $cmd_template;
	$cmd =~ s/MYHOST/$mirrhost/g;
	$cmd =~ s/MYDIR/$mirrfil/g;
	$cmd =~ s/MYOUT/$mirrtmp/g;
	$cmd =~ s/MYPRESH2/$mpresh2/g;

	open ($fh, '>', $mirrsh);
	print $fh $cmd, "\n";
	close $fh;

	# copy the shell scripts to the segments, execute them, then
	# copy back the results. Note that we add "1" as a suffix to
	# the script name and output file name to handle the case
	# where the master and segment are on the same machine (useful
	# for testing)

	my $cmd_template2 = 
			'gpscp -h MYHOST MYSHSCRIPT =:MYSHSCRIPT1 ; ' .
			'gpssh -h MYHOST \' sh MYSHSCRIPT1 > MYOUT  \' > /dev/null 2>&1; ' .
			'gpscp -h MYHOST  =:MYOUT MYOUT1 ; ' ;

	$cmd = $cmd_template2;
	$cmd =~ s/MYHOST/$primhost/g;
	$cmd =~ s/MYDIR/$primfil/g;
	$cmd =~ s/MYOUT/$primtmp/g;
	$cmd =~ s/MYSHSCRIPT/$primsh/g;

	system ($cmd);

	$cmd = $cmd_template2;
	$cmd =~ s/MYHOST/$mirrhost/g;
	$cmd =~ s/MYDIR/$mirrfil/g;
	$cmd =~ s/MYOUT/$mirrtmp/g;
	$cmd =~ s/MYSHSCRIPT/$mirrsh/g;

	system ($cmd);

	# add "1" suffix on master
	$primtmp .= '1';
	$mirrtmp .= '1';

	my $diffi3 = `$glob_diff -I GP_IGNORE -b $primtmp $mirrtmp`;
	{
		my $diffi4 = "";
		goto L_diff_fini unless (length($diffi3));

		# do a side-by-side diff (minus heap)
		print "Performing side-by-side diff (minus heap).\n";
		$diffi4 = 
		`$glob_diff  -y --suppress -b $primtmp $mirrtmp | grep -v ':heap:' | grep -v ':btree:'`;
		
		# print all non-heap diffs
		goto L_diff_fini if (length($diffi4));

		# look for heap diffs that aren't trailing blanks (remove
		# blocks with "zero_adler")
		print "Performing heap diffs that aren't trailing blanks.\n";
		$diffi4 = 
		`$glob_diff -y --suppress -b $primtmp $mirrtmp | grep -v ':2147483649' | grep -v 'SUMMARY:TRAILING' | grep -v 'SUMMARY:NONE'`;

		# print heap diffs that aren't trailing blanks
		goto L_diff_fini if (length($diffi4));

		# find TRAILING only
		print "Find TRAILING only.\n";
		$diffi4 = 
		`$glob_diff -y --suppress -b $primtmp $mirrtmp | grep ':2147483649\\|SUMMARY:TRAILING\\|SUMMARY:NONE'`;

		if (length($diffi4))
		{
			print "raw diff:\n";
			debug_print($diffi);

			print "\nenhanced diff:\n";
			debug_print($diffi2);

			print "\npreprocessed block by block diff:\n";
			debug_print($diffi3);
			print "\npostprocessed block by block diff:\n";
			debug_print($diffi4);

			$diffi3 = "";
		}

	  L_diff_fini:
		if (length($diffi3))
		{
			print "raw diff:\n";
			debug_print($diffi);

			print "\nenhanced diff:\n";
			debug_print($diffi2);

			print "\npreprocessed block by block diff:\n";
			debug_print($diffi3);
			print "\npostprocessed block by block diff:\n";
			debug_print($diffi4);

			# found differences
			push @{$glob_exit}, $row->{ploc}." ".$row->{mloc};

			$primtmp =~ s/_trois.*$//;
			$mirrtmp =~ s/_trois.*$//;
			copyfiles(\@allfilescopy, $primhost, $mirrhost, $primfil, $mirrfil, $primtmp, $mirrtmp);
		}
		else
		{
			print "\nBlock by block diff \"$floc\":   [ NO DIFFERENCES ]\n\n";
		}
	}
} # end diff_thrice

#$Data::Dumper::Indent = 3;
$Data::Dumper::Terse = 1;
$Data::Dumper::Pad=${glob_debug_fmt};
if (1)
{
	$glob_exit = [];
	my $psql_str = $glob_psql_str;

	print "Starting the integrity check.\n";

	$psql_str .= $glob_connect
        if (defined($glob_connect));

	# select from gp_segment_configuration and pg_filespace_entry,
	# to get file system location and port number

	$psql_str .= " -c \' ".
		"select f.fselocation as fselocation, " .
		"g.port " .
		"from pg_filespace_entry f, gp_segment_configuration g " .
		"where f.fsedbid = 1 and g.dbid = 1  \' ";

	my $tabdef = `$psql_str`;
	my $fseloc = tablelizer($tabdef);

	pod2usage(-msg => "\nERROR: Could not connect to database.  Please check connect string.\n\n",
			  -exitstatus => 1) unless (defined($fseloc));
	my $strtime = ctime();
	$strtime =~ s/ /_/g;
	$strtime =~ s/:/-/g;
	my $fselochsh = "CK_" . ${strtime};

	print "Timestamp used in filenames for this run \"", $fselochsh, "\"\n\n";

	# XXX XXX: port number!
	$glob_port = $fseloc->[0]->{port};

	$psql_str = $glob_psql_str;

	$psql_str .= $glob_connect
        if (defined($glob_connect));

	# select from gp_segment_configuration and pg_filespace_entry,
	# constructing a single row for each primary/mirror pair

	$psql_str .= " -c \' ".
		"select gscp.content, " .
		"gscp.hostname as phostname, gscp.address as paddress, " .
		"fep.fselocation as ploc, " .
		"gscm.hostname as mhostname, gscm.address as maddress, " .
		"fem.fselocation as mloc, " .
		"pfs.oid fsoid, " .
		"pfs.fsname, " .
		"gscp.mode, " .
		"gscp.status, " .
		"gscm.mode as mmode, " .
		"gscp.status as mstatus " .
		"from " .
		"gp_segment_configuration gscp, pg_filespace_entry fep, " .
		"gp_segment_configuration gscm, pg_filespace_entry fem, " .
		"pg_filespace pfs " .
		"where " .
		"fep.fsedbid=gscp.dbid " . # and gscp.content > -1 " .
		"and " .
		"fem.fsedbid=gscm.dbid " . # and gscm.content > -1 " .
		"and " .
		# match the filespace ids
		"fem.fsefsoid = fep.fsefsoid " . 
		"and gscp.content = gscm.content " .
		# use "dollar-quoting" to avoid dealing with nested single-quotes:
		# $q$p$q$ is equivalent to 'p'
		"and gscp.role =  " .  '$q$p$q$ ' .
		"and gscm.role =  " .  '$q$m$q$ ' .
		"and pfs.oid = fep.fsefsoid " ;

	# MPP-9883, MPP-9891: run integrity check only for desired segments
	$psql_str .= " and gscp.content in ( " . 
		join(", ", @{$glob_content}) . " ) "
		if (defined($glob_content) && scalar(@{$glob_content}));

	$psql_str .= " order by gscp.dbid, content  \'";

	print "Fetching Primary-Mirror pair configuration information\n";
	$tabdef = `$psql_str`;
	chomp($tabdef);

	printdivider();
	print $tabdef;
	printdivider();
	
	my $gp_seg_config = tablelizer($tabdef);

	$psql_str = $glob_psql_str;

	$psql_str .= $glob_connect if (defined($glob_connect));

	my $arr = `pg_controldata $fseloc->[0]->{fselocation}`;
	my @lines = split(/\n/, $arr);
	my %pairs = map { my ($key, $value) = split(/:\s+/); {$key => $value} } @lines;

	print "Performing CHECKPOINT to flush pending writes.\n";
	# checkpoint the database to flush pending writes
	$psql_str .= " -c \' checkpoint \'" ;
	$tabdef = `$psql_str`;

	$arr = `pg_controldata $fseloc->[0]->{fselocation}`;
	@lines = split(/\n/, $arr);
	my %pairs_post = map { my ($key, $value) = split(/:\s+/); {$key => $value} } @lines;

	if ($pairs{'Time of latest checkpoint'} eq $pairs_post{'Time of latest checkpoint'})
	{
		if ($pairs{'Latest checkpoint location'} eq $pairs_post{'Latest checkpoint location'})
		{
			print "Pre-checkpoint ==>.\n";
			print Data::Dumper->Dump([%pairs]);
			print "\nPost-checkpoint ==>.\n";
			print Data::Dumper->Dump([%pairs_post]);
			die ("CHECKPOINT command failed/skipped (pg_control timestamp pre and post the command same \"$pairs{'Time of latest checkpoint'}\"");
		}
	}
	printdivider();

	# Fetch the latest clog file and copy it at temp location to compare at end of tool
	# if any diference is found means IO was performed while tool was running, which invalidates the results.
	my @clog_list = `ls -t $fseloc->[0]->{fselocation}/pg_clog`;
	chomp($clog_list[0]);
	my $prev_clog_file=$clog_list[0];
	my $ret = `cp $fseloc->[0]->{fselocation}/pg_clog/${prev_clog_file} /tmp/${fselochsh}_clog.${prev_clog_file}`;
	warn "Failed to copy file \"$fseloc->[0]->{fselocation}/pg_clog/${prev_clog_file}\", ERROR: $ret" if ($ret);

	my $do_mm_skip = 0;
	# MPP-10881: separate validation check for standby master
	if ($glob_mm)
	{
		$psql_str = $glob_psql_str;

		$psql_str .= $glob_connect
			if (defined($glob_connect));

		$psql_str .= " -c \' ".
			"select * from pg_stat_replication \'";

		$tabdef = `$psql_str`;

		my $gp_mm_state = tablelizer($tabdef);

		$do_mm_skip = scalar(@{$gp_mm_state}) == 0;

		warn "master mirror not synchronized"
			if ($do_mm_skip);
	}

	my $presh_txt = ($glob_mm && !$glob_ffc) ? get_mm_presh_txt() : 
		($glob_nohash ? get_nohash_presh_txt() : get_presh_txt());
	my $presh2_txt = get_presh2_txt();

	my $ignore_arr = ignore_list($glob_ignore);

	my $ignore_cmd = '| ' . join (" | ", @{$ignore_arr}) . " " ;

	my $dirignore_cmd = "";

	if (defined($glob_dirignore) && scalar(@{$glob_dirignore}))
	{
		# MPP-11093: use "dirignore" to ignore directories 
		my @m3 = map(quotemeta, @{$glob_dirignore});

		$dirignore_cmd .= 
			" | perl -nle \'print unless /(" . join("|", @m3) . ")/  \' ";
	}

	if (defined($glob_filename) && scalar(@{$glob_filename}))
	{
		# MPP-11507: filter to only check specified filenames
		my @m3 = map(quotemeta, @{$glob_filename});

		$ignore_cmd .= 
			" | perl -nle \'print if /(" . join("|", @m3) . ")/  \' ";
	}

	# build an array of "good" segments/filespaces
	my @good_segs;

	for my $row (@{$gp_seg_config})
	{
		my $prim_dd = $row->{ploc};
		my $mirr_dd = $row->{mloc};
		my $fsname  = $row->{fsname}; # MPP-9086: filespace name
		my $fsoid   = $row->{fsoid}; 

		if ($glob_mm)
		{
			next
				unless (-1 == $row->{content});

		}
		else
		{
			next
				if (-1 == $row->{content});
		}

		my @foo = File::Spec->splitdir($prim_dd);
		my $fil = pop @foo;
		my $fil2 = $fil . "_" . $fsoid;

		# check that primary and mirror are in sync.  If they are not,
		# don't bother checking for differences (unless skipseg is
		# disabled)
		if (!(($row->{status} eq 'u') &&
			  ($row->{mode} eq 's'))
			|| $do_mm_skip)
		{
			if ($glob_skipseg)
			{
				my $msgstr = <<'EOF_msgstr';
#******************************************************************
#******************************************************************
#**  Warning: primary and mirror are out of sync!!
#**  
#**  primary: MMPRIM
#**  mirror:  MMMIRR
#**  
#**  status: MMSTAT
#**  mode:   MMMODE
#**  
#**  Skipping...
#**  
#******************************************************************
#******************************************************************
EOF_msgstr
				
				$msgstr =~ s/^#//gm;
				$msgstr =~ s/MMPRIM/$row->{ploc}/;
				$msgstr =~ s/MMMIRR/$row->{mloc}/;
				$msgstr =~ s/MMSTAT/$row->{status}/;
				$msgstr =~ s/MMMODE/$row->{mode}/;

				warn($msgstr);

				# found differences (ie not in sync)
				push @{$glob_exit}, $row->{ploc}." ".$row->{mloc} ; 
				next;
			}
			else
			{
				warn "segment and mirror are out of sync - comparison will probably fail!";
			}
		}

		my $primfil = $prim_dd;
		my $mirrfil = $mirr_dd;
		
		my $primhost = $row->{phostname};
		my $mirrhost = $row->{mhostname};

		my $primtmp = File::Spec->catfile("/tmp", $fselochsh . "_" . $fil2 . ".prim");
		my $mirrtmp = File::Spec->catfile("/tmp", $fselochsh . "_" . $fil2 . ".mirr");

		my $ppresh = File::Spec->catfile("/tmp", $fselochsh . "_" . $fil2 . "_ppre.pl");
		my $mpresh = File::Spec->catfile("/tmp", $fselochsh . "_" . $fil2 . "_mpre.pl");

		my $ppresh2 = File::Spec->catfile("/tmp", $fselochsh . "_" . $fil2 . "_ppre2.pl");
		my $mpresh2 = File::Spec->catfile("/tmp", $fselochsh . "_" . $fil2 . "_mpre2.pl");

		my $primsh = File::Spec->catfile("/tmp", $fselochsh . "_" . $fil2 . "_prim.sh");
		my $mirrsh = File::Spec->catfile("/tmp", $fselochsh . "_" . $fil2 . "_mirr.sh");

		push @good_segs, {
			row	   => $row,
			fil	   => $fil,
			fsname => $fsname,
			primfil  => $primfil,
			mirrfil  => $mirrfil,
			primhost => $primhost,
			mirrhost => $mirrhost,
			primtmp  => $primtmp,
			mirrtmp  => $mirrtmp,
			ppresh   => $ppresh,
			mpresh   => $mpresh,
			ppresh2  => $ppresh2,
			mpresh2  => $mpresh2,
			primsh   => $primsh,
			mirrsh   => $mirrsh
		};
	} # end for my row
						
	print "\nStarting to calculate full file hash on Primary and Mirror\n";

	my $time1 = time();

	for my $ii (0..(scalar(@good_segs)-1))
	{
		my $seg = $good_segs[$ii];
		my $row		 = $seg->{row};
		my $fil		 = $seg->{fil};
		my $fsname	 = $seg->{fsname};
		my $primfil	 = $seg->{primfil};
		my $mirrfil	 = $seg->{mirrfil};
		my $primhost = $seg->{primhost};
		my $mirrhost = $seg->{mirrhost};
		my $primtmp	 = $seg->{primtmp};
		my $mirrtmp	 = $seg->{mirrtmp};
		my $ppresh	 = $seg->{ppresh};
		my $mpresh	 = $seg->{mpresh};
		my $ppresh2	 = $seg->{ppresh2};
		my $mpresh2	 = $seg->{mpresh2};
		my $primsh	 = $seg->{primsh};
		my $mirrsh	 = $seg->{mirrsh};

		print "DIRECTORY: ", $fil, "  FILESPACE: $fsname\n";

		my $cmd_template = "";

		if ($glob_matchdirs)
		{
			my @mdirs = qw(
base
global
pg_changetracking
pg_clog
pg_distributedlog
pg_distributedxidmap
pg_multixact
pg_stat_tmp
pg_subtrans
pg_tblspc
pg_twophase
pg_xlog
);

			my $std_dirs = "";
			my $post_fix = "";

			# MPP-11315: check for missing top-level filespace directory
			$cmd_template .= 
				'perl -e \'{if (-e "MYDIR") {print "TOPDIR_TRUE" } else {print "TOPDIR_FALSE"}}\' ' .
				'| awk \'{print $1" 0 (TOPDIR) 0"}\' ' .
				'| perl -i -ple \'s|MYDIR||gm \' ' . 
				# strip out [hostname] prefix
				'| perl -i -ple \'s|^\\s*\\[.*\\]\\s*||gm \'  ' ;

			$cmd_template .= "\n\n";

			if ($fsname eq 'pg_system')
			{
				# filter out special directories for standard
				# pg_system filespace, but not for other filespaces

				$std_dirs =
					'| perl -nle \'print if /' . join('|', @mdirs) . '/ \' ' ;
			}
			else
			{
				# remove line for top-level directory of filespace
				# because it prints as " 0 () 0" after filtering,
				# which is useless...

				$post_fix =
					'| perl -nle \'print unless /0\s+\(\)\s+0/ \' ' ;
			}

			# MPP-11041: check for important directories
			# MPP-11301: check for even more important directories
			$cmd_template .= 
				'find MYDIR -type d | awk \'{print $1" 0 ("$1") 0"}\' ' .
				$std_dirs .
				$dirignore_cmd .
				'| sort  ' .
				'| perl -i -ple \'s|MYDIR||gm \' ' . 
				# strip out [hostname] prefix
				'| perl -i -ple \'s|^\\s*\\[.*\\]\\s*||gm \'  ' .
				$post_fix ;

			$cmd_template .= "\n\n";

		}

		# use openssl to construct an md5 hash for every file
		# Note: use mypresh*1*, because this script runs on the copy
		# of the file on the segment
		$cmd_template .= 
			'find MYDIR -type f  ' . 
			$ignore_cmd .
			'| sort  ' .
			'| xargs perl MYPRESH1  ' .
			'| perl -i -ple \'s|MYDIR||gm \' ' . 
			# strip out [hostname] prefix
			'| perl -i -ple \'s|^\\s*\\[.*\\]\\s*||gm \'  ' ;

		my $cmd = $cmd_template;
		$cmd =~ s/MYHOST/$primhost/g;
		$cmd =~ s/MYDIR/$primfil/g;
		$cmd =~ s/MYOUT/$primtmp/g;
		$cmd =~ s/MYPRESH/$ppresh/g;

		# build local shell scripts for the commands

		my $fh;

		open ($fh, '>', $ppresh);
		print $fh $presh_txt;
		close $fh;

		open ($fh, '>', $ppresh2);
		print $fh $presh2_txt;
		close $fh;

		open ($fh, '>', $primsh);
		print $fh $cmd, "\n";
		close $fh;

		$cmd = $cmd_template;
		$cmd =~ s/MYHOST/$mirrhost/g;
		$cmd =~ s/MYDIR/$mirrfil/g;
		$cmd =~ s/MYOUT/$mirrtmp/g;
		$cmd =~ s/MYPRESH/$mpresh/g;

		open ($fh, '>', $mpresh);
		print $fh $presh_txt;
		close $fh;

		open ($fh, '>', $mpresh2);
		print $fh $presh2_txt;
		close $fh;

		open ($fh, '>', $mirrsh);
		print $fh $cmd, "\n";
		close $fh;

		# copy the shell scripts to the segments, execute them, then
		# copy back the results. Note that we add "1" as a suffix to
		# the script name and output file name to handle the case
		# where the master and segment are on the same machine (useful
		# for testing)

		my $cmd_template2 = 
			'gpscp -h MYHOST MYSHSCRIPT =:MYSHSCRIPT1 ; ' .
			'gpscp -h MYHOST MYPRESCRIPT =:MYPRESCRIPT1 ; ' .
			'gpscp -h MYHOST MYPRE2SCRIPT =:MYPRE2SCRIPT1 ; ' .
			'gpssh -h MYHOST \' sh MYSHSCRIPT1 > MYOUT  \' > /dev/null 2>&1 ;' .
			'gpscp -h MYHOST  =:MYOUT MYOUT1 ; ' ;

		$cmd = $cmd_template2;
		$cmd =~ s/MYHOST/$primhost/g;
		$cmd =~ s/MYDIR/$primfil/g;
		$cmd =~ s/MYOUT/$primtmp/g;
		$cmd =~ s/MYSHSCRIPT/$primsh/g;
		$cmd =~ s/MYPRESCRIPT/$ppresh/g;
		$cmd =~ s/MYPRE2SCRIPT/$ppresh2/g;

		$seg->{primthread} = fork()
			if ($glob_usefork);

		if (exists($seg->{primthread}) &&
			defined($seg->{primthread}))
		{
			if ($seg->{primthread})
			{
				# parent
				sleep 1;
				goto L_skip1;
			}
			else
			{
				system ($cmd);
				exit(0);
			}

		}

		system ($cmd);

	  L_skip1:

		$cmd = $cmd_template2;
		$cmd =~ s/MYHOST/$mirrhost/g;
		$cmd =~ s/MYDIR/$mirrfil/g;
		$cmd =~ s/MYOUT/$mirrtmp/g;
		$cmd =~ s/MYSHSCRIPT/$mirrsh/g;
		$cmd =~ s/MYPRESCRIPT/$mpresh/g;
		$cmd =~ s/MYPRE2SCRIPT/$mpresh2/g;

		$seg->{mirrthread} = fork()
			if ($glob_usefork);

		if (exists($seg->{mirrthread}) &&
			defined($seg->{mirrthread}))
		{
			if ($seg->{mirrthread})
			{
				# parent
				sleep 1;
				goto L_skip2;
			}
			else
			{
				system ($cmd);
				exit(0);
			}

		}

		system ($cmd);

	  L_skip2:

	} # end for my $ii

	if ($glob_usefork)
	{
		for my $seg (@good_segs)
		{
			
			if (exists($seg->{primthread}) &&
				defined($seg->{primthread}))
			{
				waitpid($seg->{primthread}, 0);
			}
			
			if (exists($seg->{mirrthread}) &&
				defined($seg->{mirrthread}))
			{
				waitpid($seg->{mirrthread}, 0);
			}
		}
	}
	my $time2 = time();
	print "Done calculating hashes in ", $time2-$time1, " seconds for " .
		scalar(@good_segs) . " segs/filespaces. \n";

	print "\nNow starting to perform raw diff comparison of Primary and Mirror\n";
	for my $ii (0..(scalar(@good_segs)-1))
	{
		my $seg = $good_segs[$ii];
		my $row		 = $seg->{row};
		my $fil		 = $seg->{fil};
		my $fsname	 = $seg->{fsname};
		my $primfil	 = $seg->{primfil};
		my $mirrfil	 = $seg->{mirrfil};
		my $primhost = $seg->{primhost};
		my $mirrhost = $seg->{mirrhost};
		my $primtmp	 = $seg->{primtmp};
		my $mirrtmp	 = $seg->{mirrtmp};
		my $ppresh	 = $seg->{ppresh};
		my $mpresh	 = $seg->{mpresh};
		my $ppresh2	 = $seg->{ppresh2};
		my $mpresh2	 = $seg->{mpresh2};
		my $primsh	 = $seg->{primsh};
		my $mirrsh	 = $seg->{mirrsh};

		my $primthr  = $seg->{primthread};
		my $mirrthr  = $seg->{primthread};

		my $wcstr    = "wc -l < $primtmp" . "1"; # MPP-8951
		my $numfiles = `$wcstr`;
		chomp $numfiles;

		directorydivider();
		print "DIRECTORY: ", $fil, "  FILESPACE: $fsname";
		print " PRIMARY: ", $numfiles, " files ";

		$wcstr    = "wc -l < $mirrtmp" . "1"; # MPP-8951
		$numfiles = `$wcstr`;
		chomp $numfiles;

		print " MIRROR: ", $numfiles, " files";

		# add "1" suffix on master
		$primtmp .= '1';
		$mirrtmp .= '1';

		my $diffi = `$glob_diff -b $primtmp $mirrtmp`;
		my $grepi = "";

		if (!length($diffi))
		{
			# check for openssl failures
			$grepi = `grep badMD5 $primtmp $mirrtmp`;
		}

		if ((!length($diffi)) && (!length($grepi))) 
		{
			print "   [ NO DIFFERENCES ]\n";
		}
		else
		{
			print "   [ 1st PHASE FOUND DIFFERENCES ]\n";
			my $bigh = dandydiff($primtmp, $mirrtmp);

			if (exists($bigh->{topdir})
				&& scalar(@{($bigh->{topdir})}))
			{
				warn "Missing top-level data directory -- this is very bad";
				print "Dumping the data structure reporting differences:\n";
				print Data::Dumper->Dump([$bigh]);

				for my $td (@{($bigh->{topdir})})
				{

					if ($td =~ m/missing_topdir_p/)
					{
						push @{$glob_fixlist}, $td .
							$row->{maddress} . ":" .
							$mirrfil . " " .
							$row->{paddress} . ":" .
							$primfil ;
					}
					else
					{
						push @{$glob_fixlist}, $td .						
							$row->{paddress} . ":" .
							$primfil . " " .
							$row->{maddress} . ":" .
							$mirrfil ;
					}
					# mark as difference for exit condition
					push @{$glob_exit}, $row->{ploc}." ".$row->{mloc} ; 
				}
			}

			my @allfilescopy;

			if (exists($bigh->{extrafiles})
				&& scalar(@{($bigh->{extrafiles})}))
			{
				print "Found extra files in data directory (could be ok if they are heap files with zero size)\n";

				for my $e2 (@{($bigh->{extrafiles})})
				{
					my $extry;

					$extry = "" . $e2->[0];

					# make primary/mirror names explicit (and remove
					# leading "/" in filename)

					if ($extry =~ m/^m/i)
					{
						$extry =~ s|^m\:(/)?||i;
						print "MIRROR: \"$extry\" (SIZE: $e2->[1])\n";

						push @{$glob_fixlist}, "extra_m: " . 
							$row->{maddress} . ":" .
							File::Spec->catfile($mirrfil, $extry) . " " .
							$row->{paddress} . ":" .
							File::Spec->catfile($primfil, $extry);
						push @allfilescopy, $extry;
					}
					else
					{
						$extry =~ s|^p\:(/)?||i;
						print "PRIMARY: \"$extry\" (SIZE: $e2->[1])\n";

						push @{$glob_fixlist}, "extra_p: " . 
							$row->{paddress} . ":" .
							File::Spec->catfile($primfil, 
												$extry) . " " .
							$row->{maddress} . ":" .
							File::Spec->catfile($mirrfil, 
												$extry);
						push @allfilescopy, $extry;
					}
				}
				copyfiles(\@allfilescopy, $primhost, $mirrhost, $primfil, $mirrfil, $primtmp, $mirrtmp);
			}

			my ($dbs1, $dbs2, $dbs3);

			if (exists($bigh->{sizediff}) 
				&& scalar(keys(%{($bigh->{sizediff})})))
			{
				$dbs1 = resolve_sizediff($diffi, $row, $fselochsh, $bigh);
			}
			if (exists($bigh->{hashdiff})
				&& scalar(keys(%{($bigh->{hashdiff})})))
			{
				$dbs2 = resolve_hashdiff($diffi, $row, $fselochsh, $bigh);
			}

			# lookup extra files to see what is going on.
			if (exists($bigh->{extrafiles})
				&& scalar(@{($bigh->{extrafiles})}))
			{
				my $extras = {};

				print "Trying to resolve extra files in catalog\n";

				for my $e2 (@{($bigh->{extrafiles})})
				{
					my $extrafil = "" . $e2->[0];
					$extrafil =~ s/^p\://;
					$extrafil =~ s/^m\://;

					$extras->{$extrafil} = [0,0];
				}

				# XXX XXX: hackery: if could not resolve extra files in
				# catalog, then need to complain here
				my @bstat_ref_arr;

				$dbs3 = 
					get_relfilenode_func($extras,
										 $diffi, $row, $fselochsh, $bigh,
										 \@bstat_ref_arr
					);

				print "Extra files:\n", Data::Dumper->Dump([$dbs3]);

				if (scalar(@bstat_ref_arr))
				{
					# found differences
					push @{$glob_exit}, $row->{ploc}." ".$row->{mloc};
				}
				else
				{
					extra_files_check_for_zero_length_heap(
					   $dbs3,
					   $diffi, $row, $fselochsh, $bigh,
					   $primfil, $mirrfil, $primhost, $mirrhost, 
					   $primtmp, $mirrtmp, 
					   $ppresh2, $mpresh2,
					   $primsh, $mirrsh);
				}
			}
		
			if (($dbs1) or ($dbs2))
			{
				diff_again($dbs1, $dbs2,
					   $diffi, $row, $fselochsh, $bigh,
					   $primfil, $mirrfil, $primhost, $mirrhost, 
					   $primtmp, $mirrtmp, 
					   $ppresh2, $mpresh2,
					   $primsh, $mirrsh);
			}
		}
	} # end for my seg
	
	printdivider();

	# MPP-8186: supply fix file location for repair utility
	{
		my $ffh;

		my $fixfile = (defined($glob_fixfile) &&
					   length($glob_fixfile)) ? 
					   $glob_fixfile :
					   File::Spec->catfile("/tmp", 
										   $fselochsh . "_file.fix" );

		unless(open ($ffh, ">", $fixfile))
		{
			warn("could not open $fixfile : $!");
			goto L_nofix;
		}

		print $ffh join("\n", @{$glob_fixlist}), "\n"
			if (scalar(@{$glob_fixlist}));

		close $ffh;

		print "\n\nWrote fix list to: $fixfile \n";
		# number of items
		print "(" . scalar(@{$glob_fixlist}) . " items)\n\n";

	  L_nofix:

	}

	printdivider();
	if (scalar(@{$glob_exit}))
	{
		my %hash   = map { $_, 1 } @{$glob_exit};
		print "found differences: \n  ", join("\n  ", keys %hash), "\n";
		printdivider();
		print "Differing files are copied to directory \"/tmp/${fselochsh}*_data\" on affected primary and mirror.\n";
		print "Please upload the same along with OUTPUT and FIX file while opening bug ticket.\n";
		printdivider();

		@clog_list = `ls -t $fseloc->[0]->{fselocation}/pg_clog`;
		chomp($clog_list[0]);
		if (($prev_clog_file ne $clog_list[0]) or 
			(length(`$glob_diff -b $fseloc->[0]->{fselocation}/pg_clog/$clog_list[0] /tmp/${fselochsh}_clog.$prev_clog_file`)))
		{
			print "PreClogName=\"/tmp/${fselochsh}_clog.$prev_clog_file\", PostClogName=\"$fseloc->[0]->{fselocation}/pg_clog/$clog_list[0]\"";
			print "\nNOTE: Write happened to CLOG file while tool was running, indicating IO was perfomed on DB";
			print "\nSince this tool requires no IO activity, above results reported cannot be trusted.\n";
			printdivider();
		}
		exit(1);
	}

	print "no differences found\n";
	exit(0);
}
exit();
