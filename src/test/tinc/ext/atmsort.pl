#!/usr/bin/env perl
#
# $Header: //cdb2/feature/PARISTX/ORCA/cdb-pg/src/test/regress/atmsort.pl#1 $
#
# copyright (c) 2007, 2008, 2009 GreenPlum.  All rights reserved.  
# Author: Jeffrey I Cohen
#
#
use Pod::Usage;
use Getopt::Long;
use Data::Dumper;
use strict;
use warnings;

=head1 NAME

B<atmsort.pl> - [A] [T]est [M]echanism Sort: sort the contents of SQL log files to aid diff comparison

=head1 SYNOPSIS

B<atmsort.pl> [options] logfile [logfile...]

Options:

    -help            brief help message
    -man             full documentation
    -ignore_headers  ignore header lines in query output
    -ignore_plans    ignore explain plan content in query output
    -init <file>     load initialization file
    -do_equiv        construct or compare equivalent query regions

=head1 OPTIONS

=over 8

=item B<-help>

    Print a brief help message and exits.

=item B<-man>
    
    Prints the manual page and exits.

=item B<-ignore_headers> 

gpdiff/atmsort expect Postgresql "psql-style" output for SELECT
statements, with a two line header composed of the column names,
separated by vertical bars (|), and a "separator" line of dashes and
pluses beneath, followed by the row output.  The psql utility performs
some formatting to adjust the column widths to match the size of the
row output.  Setting this parameter causes gpdiff to ignore any
differences in the column naming and format widths globally.

=item B<-ignore_plans> 

Specify this option to ignore any explain plan diffs between the
input files. This will completely ignore any plan content in 
the input files thus masking differences in plans between the input files.

For example, for the following plan: 
explain select i from foo where i > 10;
                                 QUERY PLAN
-----------------------------------------------------------------------------
 Gather Motion 2:1  (slice1; segments: 2)  (cost=0.00..2.72 rows=45 width=4)
   ->  Table Scan on foo  (cost=0.00..1.55 rows=45 width=4)
         Filter: i > 10
 Settings:  optimizer=on
(4 rows)

atmsort.pl -ignore_plans will reduce this to: 

explain select i from foo where i > 10;
QUERY PLAN
___________
GP_IGNORE:{
GP_IGNORE:  'child' => [
GP_IGNORE:    {
GP_IGNORE:      'id' => 2,
GP_IGNORE:      'parent' => 1,
GP_IGNORE:      'short' => 'Table Scan on foo'
GP_IGNORE:    }
GP_IGNORE:  ],
GP_IGNORE:  'id' => 1,
GP_IGNORE:  'short' => 'Gather Motion'
GP_IGNORE:}
GP_IGNORE:(4 rows)


=item B<-init> <file>

Specify an initialization file containing a series of directives
(mainly for match_subs) that get applied to the input files.  To
specify multiple initialization files, use multiple init arguments,
eg:

  -init file1 -init file2 


=item B<-do_equiv>
    
    Choose one of the following options
    
=over 12

=item ignore_all:  

(default) ignore all content in a start_equiv/end_equiv block.

=item make:  

replace the query output of all queries in a start_equiv/end_equiv
block with the output of the first query, processed according to any
formatting directives for each query.


=item compare:  

process the query output of all queries in a start_equiv/end_equiv
block (versus prefixing the entire block with GP_IGNORE).

=back

=back

=head1 DESCRIPTION

atmsort reads sql log files from STDIN and sorts the query output for
all SELECT statements that do *not* have an ORDER BY, writing the
result to STDOUT.  This change to the log facilitates diff comparison,
since unORDERed query output does not have a guaranteed order.  Note
that for diff to work correctly, statements that do use ORDER BY must
have a fully-specified order.  The utility gpdiff.pl invokes atmsort
in order to compare the Greenplum test results against standard
Postgresql.

The log content must look something like:

 SELECT a, b, c, d 
   from foo 
   ORDER BY 1,2,3,4;
      a      |        b        |     c     |       d       
 ------------+-----------------+-----------+---------------
  1          | 1               | 1         | 1
  1          | 1               | 1         | 2
  3          | 2               | 2         | 5
 (3 rows)

The log file must contain SELECT statements, followed by the query
output in the standard Postgresql format, ie a set of named columns, a
separator line constructed of dashes and plus signs, and the rows,
followed by an "(N rows)" row count.  The SELECT statement must be
unambiguous, eg no embedded SQL keywords like INSERT, UPDATE, or
DELETE, and it must be terminated with a semicolon.  Normally, the
query output is sorted, but if the statement contains an ORDER BY
clause the query output for that query is not sorted.

=head2 EXPLAIN PLAN

atmsort can also use explain.pl to process EXPLAIN and EXPLAIN ANALYZE
output in a configuration-independent way.  It strips out all timing,
segment, and slice information, reducing the plan to a simple nested
perl structure.  For example, for the following plan:

explain analyze select * from customer;

                                     QUERY PLAN                              
------------------------------------------------------------------------
 Gather Motion 2:1  (slice1)  (cost=0.00..698.88 rows=25088 width=550)
   Rows out:  150000 rows at destination with 0.230 ms to first row, 
   386 ms to end, start offset by 8.254 ms.
   ->  Seq Scan on customer  (cost=0.00..698.88 rows=25088 width=550)
         Rows out:  Avg 75000.0 rows x 2 workers.  Max 75001 rows (seg0) 
         with 0.056 ms to first row, 26 ms to end, start offset by 7.332 ms.
 Slice statistics:
   (slice0)    Executor memory: 186K bytes.
   (slice1)    Executor memory: 130K bytes avg x 2 workers, 
               130K bytes max (seg0).
 Total runtime: 413.401 ms
(8 rows)

atmsort reduces the plan to:

                                     QUERY PLAN                              
------------------------------------------------------------------------
{
  'child' => [
              {
      'id' => 2,
      'parent' => 1,
      'short' => 'Seq Scan on customer'
      }
  ],
  'id' => 1,
  'short' => 'Gather Motion'
  }
(8 rows)


=head2 Advanced Usage 

atmsort supports several "commands" that allow finer-grained control
over the comparison process for SELECT queries.  These commands are
specified in comments in the following form:

 --
 -- order 1
 --
 SELECT a, b, c, d 
   from foo 
   ORDER BY 1;

or

 SELECT a, b, c, d 
   from foo 
   ORDER BY 1; -- order 1

The supported commands are:

=over 12

=item -- order column number[, column number...]

  The order directive is used to compare 
  "partially-ordered" query
  output.  The specified columns are assumed 
  to be ordered, and the  remaining columns are 
  sorted to allow for deterministic comparison.

=item -- ignore

The ignore directive prefixes the SELECT output with GP_IGNORE.  The
diff command can use the -I flag to ignore lines with this prefix.

=item -- mvd colnum[, colnum...] -> colnum[, colnum...] [; <additional specs>]

mvd is designed to support Multi-Value Dependencies for OLAP queries.
The syntax "col1,col2->col3,col4" indicates that the col1 and col2
values determine the col3, col4 result order.

=item -- start_ignore

Ignore all results until the next "end_ignore" directive.  The
start_ignore directive prefixes all subsequent output with GP_IGNORE,
and all other formatting directives are ignored as well.  The diff
command can use the -I flag to ignore lines with this prefix.

=item -- end_ignore

  Ends the ignored region that started with "start_ignore"

=item -- start_headers_ignore

Similar to the command-line "ignore_headers", ignore differences in
column naming and format widths.

=item -- end_headers_ignore

  Ends the "headers ignored" region that started with "start_headers_ignore"

=item -- start_equiv

Begin an "equivalent" region, and treat contents according to the
specified --do_equiv option.  Normally, the results are ignored.  The
"--do_equiv=make" option replaces the contents of all queries in the
equivalent region with the results of the first query.  If
"--do_equiv=compare" option is specified, the region is processed
according to the standard query formatting rules.

=item -- end_equiv

  Ends the equivalent region that started with "start_equiv"

=item -- copy_stdout

Marks the start of a "copy" command that writes to stdout.  In this
case, the directive must be on the same line as the command, e.g:

  copy mystuff to stdout ; -- copy_stdout

=item -- start_matchsubs

Starts a list of match/substitution expressions, where the match and
substitution are specified as perl "m" and "s" operators for a single
line of input.  atmsort will compile the expressions and use them to
process the current input file.  The format is:

    -- start_matchsubs
    --
    -- # first, a match expression
    -- m/match this/
    -- # next, a substitute expression
    -- s/match this/substitute this/
    --
    -- # and can have more matchsubs after this...
    --
    -- end_matchsubs

  Blank lines are ignored, and comments may be used if they are
  prefixed with "#", the perl comment character, eg:

    -- # this is a comment

  Multiple match and substitute pairs may be specified.  See "man
  perlre" for more information on perl regular expressions.

=item -- end_matchsubs
  
  Ends the match/substitution region that started with "start_matchsubs"

=item -- start_matchignore

Similar to matchsubs, starts a list of match/ignore expressions as a
set of perl match operators.  Each line that matches one of the
specified expressions is elided from the atmsort output.  Note that
there isn't an "ignore" expression -- just a list of individual match
operators.

=item -- end_matchignore

  Ends the match/ignore region that started with "start_matchignore"

=item -- force_explain

Normally, atmsort can detect that a SQL query is being EXPLAINed, and
the expain processing will happen automatically.  However, if the
query is complex, you may need to tag it with a comment to force the
explain.  Using this command for non-EXPLAIN statements is
inadvisable.

=back

Note that you can combine the directives for a single query, but each
directive must be on a separate line.  Multiple mvd specifications
must be on a single mvd line, separated by semicolons.  Note that 
start_ignore overrides all directives until the next end_ignore.

=head1 CAVEATS/LIMITATIONS

atmsort cannot handle "unsorted" SELECT queries where the output has
strings with embedded newlines or pipe ("|") characters due to
limitations with the parser in the "tablelizer" function.  Queries
with these characteristics must have an ORDER BY clause to avoid
potential erroneous comparison.

=head1 AUTHORS

Jeffrey I Cohen

Copyright (c) 2007, 2008, 2009 GreenPlum.  All rights reserved.  

Address bug reports and comments to: jcohen@greenplum.com


=cut

my $glob_id = "";

# optional set of prefixes to identify sql statements, query output,
# and sorted lines (for testing purposes)
#my $apref = 'a: ';
#my $bpref = 'b: ';
#my $cpref = 'c: ';
#my $dpref = 'S: ';
my $apref = '';
my $bpref = '';
my $cpref = '';
my $dpref = '';

my $glob_compare_equiv;
my $glob_make_equiv_expected;
my $glob_ignore_headers;
my $glob_ignore_plans;
my $glob_ignore_whitespace;
my $glob_init;

my $glob_orderwarn;
my $glob_verbose;
my $glob_fqo;

# array of "expected" rows from first query of equiv region
my $equiv_expected_rows;

BEGIN
{ 
    $glob_compare_equiv       = 0;
    $glob_make_equiv_expected = 0;
    $glob_ignore_headers      = 0;
    $glob_ignore_plans        = 0;
    $glob_ignore_whitespace   = 0;
    $glob_init                = [];

	$glob_orderwarn           = 0;
	$glob_verbose             = 0;
	$glob_fqo                 = {count => 0};
}

BEGIN {
    my $man  = 0;
    my $help = 0;
    my $compare_equiv = 0;
    my $make_equiv_expected = 0;
    my $do_equiv;
    my $ignore_headers;
    my $ignore_plans;
    my @init_file;
    my $verbose;
    my $orderwarn;

    GetOptions(
               'help|?' => \$help, man => \$man, 
               'gpd_ignore_headers|gp_ignore_headers|ignore_headers' => \$ignore_headers,
               'gpd_ignore_plans|gp_ignore_plans|ignore_plans' => \$ignore_plans,
               'gpd_init|gp_init|init:s' => \@init_file,
               'do_equiv:s' => \$do_equiv,
		        'order_warn|orderwarn' => \$orderwarn,
		        'verbose' => \$verbose
               )
        or pod2usage(2);

    if (defined($do_equiv))
    {
        if ($do_equiv =~ m/^(ignore)/i)
        {
            # ignore all - default
        }
        elsif ($do_equiv =~ m/^(compare)/i)
        {
            # compare equiv region
            $compare_equiv = 1;
        }
        elsif ($do_equiv =~ m/^(make)/i)            
        {
            # make equiv expected output
            $make_equiv_expected = 1;
        }
        else
        {
            $glob_id = "unknown do_equiv option: $do_equiv\nvalid options are:\n\tdo_equiv=compare\n\tdo_equiv=make";
            $help = 1;
        }

    }

    pod2usage(-msg => $glob_id, -exitstatus => 1) if $help;
    pod2usage(-msg => $glob_id, -exitstatus => 0, -verbose => 2) if $man;

    $glob_compare_equiv       = $compare_equiv;
    $glob_make_equiv_expected = $make_equiv_expected;
    $glob_ignore_headers      = $ignore_headers;
    $glob_ignore_plans        = $ignore_plans;

    $glob_ignore_whitespace   = $ignore_headers; # XXX XXX: for now

	# ENGINF-200: allow multiple init files
    push @{$glob_init}, @init_file;

	$glob_orderwarn           = $orderwarn;
    $glob_verbose      		  = $verbose;

}

my $glob_match_then_sub_fnlist;

sub _build_match_subs
{
    my ($here_matchsubs, $whomatch) = @_;

    my $stat = [1];

     # filter out the comments and blank lines
     $here_matchsubs =~ s/^\s*\#.*$//gm;
     $here_matchsubs =~ s/^\s+$//gm;

#    print $here_matchsubs;

    # split up the document into separate lines
    my @foo = split(/\n/, $here_matchsubs);

    my $ii = 0;

    my $matchsubs_arr = [];
    my $msa;

    # build an array of arrays of match/subs pairs
    while ($ii < scalar(@foo))
    {
        my $lin = $foo[$ii];

        if ($lin =~ m/^\s*$/) # skip blanks
        {
            $ii++;
            next;
        }

        if (defined($msa))
        {
            push @{$msa}, $lin;

            push @{$matchsubs_arr}, $msa;

            undef $msa;
        }
        else
        {
            $msa = [$lin];
        }
        $ii++;
        next;
    } # end while

#    print Data::Dumper->Dump($matchsubs_arr);

    my $bigdef;

    my $fn1;

    # build a lambda function for each expression, and load it into an
    # array
    my $mscount = 1;

    for my $defi (@{$matchsubs_arr})
    {
        unless (2 == scalar(@{$defi}))
        {
            my $err1 = "bad definition: " . Data::Dumper->Dump([$defi]);
            $stat->[0] = 1;
            $stat->[1] = $err1;
            return $stat;
        }

        $bigdef = '$fn1 = sub { my $ini = shift; '. "\n";
        $bigdef .= 'if ($ini =~ ' . $defi->[0];
        $bigdef .= ') { ' . "\n";
#        $bigdef .= 'print "match\n";' . "\n";
        $bigdef .= '$ini =~ ' . $defi->[1];
        $bigdef .= '; }' . "\n";
        $bigdef .= 'return $ini; }' . "\n";

#        print $bigdef;

        if (eval $bigdef)
        {
            my $cmt = $whomatch . " matchsubs \#" . $mscount;
            $mscount++;

            # store the function pointer and the text of the function
            # definition
            push @{$glob_match_then_sub_fnlist}, 
			[$fn1, $bigdef, $cmt, $defi->[0], $defi->[1]];

			if ($glob_verbose)
			{
				print "GP_IGNORE: Defined $cmt\t$defi->[0]\t$defi->[1]\n"
			}
        }
        else
        {
            my $err1 = "bad eval: $bigdef";
            $stat->[0] = 1;
            $stat->[1] = $err1;
            return $stat;
        }

    }

#    print Data::Dumper->Dump($glob_match_then_sub_fnlist);
   
    return $stat;

} # end _build_match_subs

# list of all the match/substitution expressions
BEGIN
{
    my $here_matchsubs;
	

# construct a "HERE" document of match expressions followed by
# substitution expressions.  Embedded comments and blank lines are ok
# (they get filtered out).

    $here_matchsubs = << 'EOF_matchsubs';

# some cleanup of greenplum-specific messages
m/\s+(\W)?(\W)?\(seg.*pid.*\)/
s/\s+(\W)?(\W)?\(seg.*pid.*\)//

m/WARNING:\s+foreign key constraint \".*\" will require costly sequential scans/
s/\".*\"/\"dummy key\"/

m/CONTEXT:.*\s+of this segment db input data/
s/\s+of this segment db input data//

# distributed transactions
m/(ERROR|WARNING|CONTEXT|NOTICE):.*gid\s+=\s+(\d+)/
s/gid.*/gid DUMMY/

m/(ERROR|WARNING|CONTEXT|NOTICE):.*DTM error.*gathered (\d+) results from cmd.*/
s/gathered.*results/gathered SOME_NUMBER_OF results/

# fix code locations eg "(xact.c:1458)" to "(xact.c:SOME_LINE)"
m/(ERROR|WARNING|CONTEXT|NOTICE):\s+Raise an error as directed by/
s/\.c\:\d+\)/\.c\:SOME_LINE\)/

m/(DETAIL|ERROR|WARNING|CONTEXT|NOTICE):\s+Raise .* for debug_dtm_action\s*\=\s* \d+/
s/\.c\:\d+\)/\.c\:SOME_LINE\)/

m/(ERROR|WARNING|CONTEXT|NOTICE):\s+Could not .* savepoint/
s/\.c\:\d+\)/\.c\:SOME_LINE\)/

m/(ERROR|WARNING|CONTEXT|NOTICE):.*connection.*failed.*(http|gpfdist)/
s/connection.*failed.*(http|gpfdist).*/connection failed dummy_protocol\:\/\/DUMMY_LOCATION/

# the EOF ends the HERE document
EOF_matchsubs

    $glob_match_then_sub_fnlist = [];
	
    my $stat = _build_match_subs($here_matchsubs, "DEFAULT");

    if (scalar(@{$stat}) > 1)
    {
        die $stat->[1];
    }

}

sub match_then_subs
{
    my $ini = shift;

    for my $ff (@{$glob_match_then_sub_fnlist})
    {

        # get the function and execute it
        my $fn1 = $ff->[0];
		if (!$glob_verbose)
		{
			$ini = &$fn1($ini);
		}
		else
		{
			my $subs = &$fn1($ini);

			unless ($subs eq $ini)
			{
				print "GP_IGNORE: was: $ini";				
				print "GP_IGNORE: matched $ff->[-3]\t$ff->[-2]\t$ff->[-1]\n"
			}

			$ini = &$fn1($ini);
		}

    }
    return $ini;
}

my $glob_match_then_ignore_fnlist;

sub _build_match_ignores
{
    my ($here_matchignores, $whomatch) = @_;

    my $stat = [1];

     # filter out the comments and blank lines
     $here_matchignores =~ s/^\s*\#.*$//gm;
     $here_matchignores =~ s/^\s+$//gm;

#    print $here_matchignores;

    # split up the document into separate lines
    my @foo = split(/\n/, $here_matchignores);

    my $matchignores_arr = [];

    # build an array of match expressions
    for my $lin (@foo)
    {
        next
            if ($lin =~ m/^\s*$/); # skip blanks
        
        push @{$matchignores_arr}, $lin;
    }

#    print Data::Dumper->Dump($matchignores_arr);

    my $bigdef;

    my $fn1;

    # build a lambda function for each expression, and load it into an
    # array
    my $mscount = 1;

    for my $defi (@{$matchignores_arr})
    {
        $bigdef = '$fn1 = sub { my $ini = shift; '. "\n";
        $bigdef .= 'return ($ini =~ ' . $defi;
        $bigdef .= ') ; } ' . "\n";
#        print $bigdef;

        if (eval $bigdef)
        {
            my $cmt = $whomatch . " matchignores \#" . $mscount;
            $mscount++;

            # store the function pointer and the text of the function
            # definition
            push @{$glob_match_then_ignore_fnlist}, 
			[$fn1, $bigdef, $cmt, $defi, "(ignore)"];
			if ($glob_verbose)
			{
				print "GP_IGNORE: Defined $cmt\t$defi\n"
			}

        }
        else
        {
            my $err1 = "bad eval: $bigdef";
            $stat->[0] = 1;
            $stat->[1] = $err1;
            return $stat;
        }

    }

#    print Data::Dumper->Dump($glob_match_then_ignore_fnlist);
   
    return $stat;

} # end _build_match_ignores

# list of all the match/ignore expressions
BEGIN
{
    my $here_matchignores;

# construct a "HERE" document of match expressions to ignore in input.
# Embedded comments and blank lines are ok (they get filtered out).

    $here_matchignores = << 'EOF_matchignores';

        # XXX XXX: note the discrepancy in the NOTICE messages
        # 'distributed by' vs 'DISTRIBUTED BY'
m/NOTICE:\s+Table doesn\'t have \'distributed by\' clause\, and no column type is suitable/i

m/NOTICE:\s+Table doesn\'t have \'DISTRIBUTED BY\' clause/i

m/NOTICE:\s+Dropping a column that is part of the distribution policy/

m/NOTICE:\s+Table has parent\, setting distribution columns to match parent table/

m/HINT:\s+The \'DISTRIBUTED BY\' clause determines the distribution of data/

m/WARNING:\s+Referential integrity \(.*\) constraints are not supported in Greenplum Database/


m/^\s*Distributed by:\s+\(.*\)\s*$/

        # ignore notices for DROP sqlobject IF EXISTS "objectname"
        # eg NOTICE:  table "foo" does not exist, skipping
        #
        # the NOTICE is different from the ERROR case, which does not
        # end with "skipping"
m/^NOTICE:\s+\w+\s+\".*\"\s+does not exist\,\s+skipping\s*$/


# the EOF ends the HERE document
EOF_matchignores

    $glob_match_then_ignore_fnlist = [];

    my $stat = _build_match_ignores($here_matchignores, "DEFAULT");

    if (scalar(@{$stat}) > 1)
    {
        die $stat->[1];
    }

}

# if the input matches, return 1 (ignore), else return 0 (keep)
sub match_then_ignore
{
    my $ini = shift;

    for my $ff (@{$glob_match_then_ignore_fnlist})
    {
        # get the function and execute it
        my $fn1 = $ff->[0];

		if (&$fn1($ini))
		{
			if ($glob_verbose)
			{
				print "GP_IGNORE: matched $ff->[-3]\t$ff->[-2]\t$ff->[-1]\n"
			}
			return 1; # matched
		}
    }
    return 0; # no match
}

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
# reformat the EXPLAIN output according to the directive hash
sub format_explain
{
    my ($outarr, $directive) = @_;
    my $prefix = "";
	my $xopt = "perl"; # normal case
	my $psuffix = "";

    $directive = {} unless (defined($directive));
    
    # Ignore plan content if its between start_ignore and end_ignore blocks
    # or if -ignore_plans is specified.
    $prefix = "GP_IGNORE:"
         if (exists($directive->{ignore})) || ($glob_ignore_plans);

    {
        use IO::File;
        use POSIX qw(tmpnam);

        my ($tmpnam, $tmpfh);

        for (;;) {
            $tmpnam = tmpnam();
            sysopen($tmpfh, $tmpnam, O_RDWR | O_CREAT | O_EXCL) && last;
        }

        if (scalar(@{$outarr}))
        {
            print $tmpfh "QUERY PLAN\n";
            # explain.pl expects a long string of dashes
            print $tmpfh "-" x 71, "\n";
            for my $lin (@{$outarr})
            {
                print $tmpfh $lin;
            }
            print $tmpfh "(111 rows)\n";
        }

        close $tmpfh;
		
        if (exists($directive->{explain})
			&& ($directive->{explain} =~ m/operator/i))
		{
			$xopt = "operator";
			$psuffix = " | sort ";
		}

		my $plantxt = "explain.pl -opt $xopt -prune heavily < $tmpnam $psuffix";

                
		my $xplan = `$plantxt`;


        unlink $tmpnam;

        if (defined($prefix) && length($prefix))
        {
            $xplan =~ s/^/$prefix/gm;
        }


		print $xplan;

		# for "force_explain operator", replace the outarr with the
		# processed output (for equivalence regions )
        if (scalar(@{$outarr}) 
			&& exists($directive->{explain})
			&& ($directive->{explain} =~ m/operator/i))
		{
			my @foo = split (/\n/, $xplan);

			# gross -- need to add the carriage return back!
			for my $ii (0..(scalar(@foo)-1))
			{
				$foo[$ii] .= "\n";
			}

			return  \@foo;
		}

    }
}    

# reformat the query output according to the directive hash
sub format_query_output
{
    my ($fqostate, $has_order, $outarr, $directive) = @_;
    my $prefix = "";

    $directive = {} unless (defined($directive));

	$fqostate->{count} += 1;

	if ($glob_verbose)
	{
		print "GP_IGNORE: start fqo $fqostate->{count}\n";
	}

    if (exists($directive->{make_equiv_expected}))
    {
		# special case for EXPLAIN PLAN as first "query"
		if (exists($directive->{explain}))
		{
			my $stat = format_explain($outarr, $directive);

			# save the first query output from equiv as "expected rows"

			if ($stat)
			{
				push @{$equiv_expected_rows}, @{$stat};
			}
			else
			{
				push @{$equiv_expected_rows}, @{$outarr};
			}

			if ($glob_verbose)
			{
				print "GP_IGNORE: end fqo $fqostate->{count}\n";
			}

			return ;

		}

        # save the first query output from equiv as "expected rows"
        push @{$equiv_expected_rows}, @{$outarr};
    }
    elsif (defined($equiv_expected_rows)
           && scalar(@{$equiv_expected_rows}))
    {
        # reuse equiv expected rows if you have them
        $outarr = [];
        push @{$outarr}, @{$equiv_expected_rows};
    }

	# explain (if not in an equivalence region)
    if (exists($directive->{explain}))
    {
       format_explain($outarr, $directive);
	   if ($glob_verbose)
	   {
		   print "GP_IGNORE: end fqo $fqostate->{count}\n";
	   }
	   return;
    }
	
    $prefix = "GP_IGNORE:"
        if (exists($directive->{ignore}));

    if (exists($directive->{sortlines}))
    {
        my $firstline = $directive->{firstline};
        my $ordercols = $directive->{order};
        my $mvdlist   = $directive->{mvd};

        # lines already have newline terminator, so just rejoin them.
        my $lines = join ("", @{$outarr});

        my $ah1 = tablelizer($lines, $firstline);

        unless (defined($ah1) && scalar(@{$ah1}))
        {
#            print "No tablelizer hash for $lines, $firstline\n";
#            print STDERR "No tablelizer hash for $lines, $firstline\n";

			if ($glob_verbose)
			{
				print "GP_IGNORE: end fqo $fqostate->{count}\n";
			}

            return;
        }

        my @allcols = sort (keys(%{$ah1->[0]}));

        my @presortcols;
        if (defined($ordercols) && length($ordercols))
        {
#        $ordercols =~ s/^.*order\s*//;
            $ordercols =~ s/\n//gm;
            $ordercols =~ s/\s//gm;

            @presortcols = split(/\s*\,\s*/, $ordercols);
        }

        my @mvdcols;
        my @mvd_deps;
        my @mvd_nodeps;
        my @mvdspec;
        if (defined($mvdlist) && length($mvdlist))
        {
            $mvdlist  =~ s/\n//gm;
            $mvdlist  =~ s/\s//gm;

            # find all the mvd specifications (separated by semicolons)
            my @allspecs = split(/\;/, $mvdlist);

#            print "allspecs:", Data::Dumper->Dump(\@allspecs);

            for my $item (@allspecs)
            {
                my $realspec;
                # split the specification list, separating the
                # specification columns on the left hand side (LHS)
                # from the "dependent" columns on the right hand side (RHS)
                my @colset = split(/\-\>/, $item, 2);
                unless (scalar(@colset) == 2)
                {
                    print "invalid colset for $item\n";
                    print STDERR "invalid colset for $item\n";
                    next;
                }
                # specification columns (LHS)
                my @scols = split(/\,/, $colset[0]);
                unless (scalar(@scols))
                {
                    print "invalid dependency specification: $colset[0]\n";
                    print STDERR 
                        "invalid dependency specification: $colset[0]\n";
                    next;
                }
                # dependent columns (RHS)
                my @dcols = split(/\,/, $colset[1]);
                unless (scalar(@dcols))
                {
                    print "invalid specified dependency: $colset[1]\n";
                    print STDERR "invalid specified dependency: $colset[1]\n";
                    next;
                }
                $realspec = {};
                my $scol2 = [];
                my $dcol2 = [];
                my $sdcol = [];
                $realspec->{spec} = $item;
                push @{$scol2}, @scols;
                push @{$dcol2}, @dcols;
                push @{$sdcol}, @scols, @dcols;
                $realspec->{scol} = $scol2;
                $realspec->{dcol} = $dcol2;
                $realspec->{allcol} = $sdcol;

                push @mvdcols, @scols, @dcols;
                # find all the dependent columns
                push @mvd_deps, @dcols;
                push @mvdspec, $realspec;
            }

            # find all the mvd cols which are *not* dependent.  Need
            # to handle the case of self-dependency, eg "mvd 1->1", so
            # must build set of all columns, then strip out the
            # "dependent" cols.  So this is the set of all LHS columns
            # which are never on the RHS.
            my %get_nodeps;

            for my $col (@mvdcols)
            {
                $get_nodeps{$col} = 1;
            }

            # remove dependent cols
            for my $col (@mvd_deps)
            {
                if (exists($get_nodeps{$col}))
                {
                    delete $get_nodeps{$col};
                }
            }
            # now sorted and unique, with no dependents
            @mvd_nodeps = sort (keys(%get_nodeps));
#            print "mvdspec:", Data::Dumper->Dump(\@mvdspec);
#            print "mvd no deps:", Data::Dumper->Dump(\@mvd_nodeps);
        }

        my %unsorth;
        
        for my $col (@allcols)
        {
            $unsorth{$col} = 1;
        }

		# clear sorted column list if just "order 0"
        if ((1 == scalar(@presortcols))
			&& ($presortcols[0] eq "0"))
        {
			@presortcols = ();
		}


        for my $col (@presortcols)
        {
            if (exists($unsorth{$col}))
            {
                delete $unsorth{$col};
            }
        }
        for my $col (@mvdcols)
        {
            if (exists($unsorth{$col}))
            {
                delete $unsorth{$col};
            }
        }
        my @unsortcols = sort(keys(%unsorth));

#        print Data::Dumper->Dump([$ah1]);

        if (scalar(@presortcols))
        {
            my $hd1 = "sorted columns " . join(", ", @presortcols);

            print $hd1, "\n", "-"x(length($hd1)), "\n";

            for my $h_row (@{$ah1})
            {
                my @collist;

                @collist = ();

#            print "hrow:",Data::Dumper->Dump([$h_row]), "\n";            

                for my $col (@presortcols)
                {
#                print "col: ($col)\n";
                    if (exists($h_row->{$col}))
                    {
                        push @collist, $h_row->{$col};
                    }
                    else
                    {
                        my $maxcol = scalar(@allcols);
                        my $errstr = 
                            "specified ORDER column out of range: $col vs $maxcol\n";
                        print $errstr;
                        print STDERR $errstr;
                        last;
                    }
                }
                print $prefix, join(' | ', @collist), "\n";
            }
        }

        if (scalar(@mvdspec))
        {
            my @outi;

            my $hd1 = "multivalue dependency specifications";

            print $hd1, "\n", "-"x(length($hd1)), "\n";

            for my $mspec (@mvdspec)
            {
                $hd1 = $mspec->{spec};
                print $hd1, "\n", "-"x(length($hd1)), "\n";

                for my $h_row (@{$ah1})
                {
                    my @collist;

                    @collist = ();

#            print "hrow:",Data::Dumper->Dump([$h_row]), "\n";            

                    for my $col (@{$mspec->{allcol}})
                    {
#                print "col: ($col)\n";
                        if (exists($h_row->{$col}))
                        {
                            push @collist, $h_row->{$col};
                        }
                        else
                        {
                            my $maxcol = scalar(@allcols);
                            my $errstr = 
                            "specified MVD column out of range: $col vs $maxcol\n";
                            print $errstr;
                            print STDERR $errstr;
                            last;
                        }

                    }
                    push @outi, join(' | ', @collist);
                }
                my @ggg= sort @outi;

                for my $line (@ggg)
                {
                    print $prefix, $line, "\n";
                }
                @outi = ();
            }
        }
        my $hd2 = "unsorted columns " . join(", ", @unsortcols);

        # the "unsorted" comparison must include all columns which are
        # not sorted or part of an mvd specification, plus the sorted
        # columns, plus the non-dependent mvd columns which aren't
        # already in the list
        if ((scalar(@presortcols))
            || scalar(@mvd_nodeps))
        {
            if (scalar(@presortcols))
            {
                if (scalar(@mvd_deps))
                {
                    my %get_presort;

                    for my $col (@presortcols)
                    {
                        $get_presort{$col} = 1;
                    }
                    # remove "dependent" (RHS) columns
                    for my $col (@mvd_deps)
                    {
                        if (exists($get_presort{$col}))
                        {
                            delete $get_presort{$col};
                        }
                    }
                    # now sorted and unique, minus all mvd dependent cols
                    @presortcols = sort (keys(%get_presort));

                }

                if (scalar(@presortcols))
                {
                    $hd2 .= " ( " . join(", ", @presortcols) . ")";
                    # have to compare all columns as unsorted 
                    push @unsortcols, @presortcols;
                }
            }
            if (scalar(@mvd_nodeps))
            {
                my %get_nodeps;

                for my $col (@mvd_nodeps)
                {
                    $get_nodeps{$col} = 1;
                }
                # remove "nodeps" which are already in the output list
                for my $col (@unsortcols)
                {
                    if (exists($get_nodeps{$col}))
                    {
                        delete $get_nodeps{$col};
                    }
                }
                # now sorted and unique, minus all unsorted/sorted cols
                @mvd_nodeps = sort (keys(%get_nodeps));
                if (scalar(@mvd_nodeps))
                {
                    $hd2 .= " (( " . join(", ", @mvd_nodeps) . "))";
                    # have to compare all columns as unsorted 
                    push @unsortcols, @mvd_nodeps;
                }

            }
            
        }

        print $hd2, "\n", "-"x(length($hd2)), "\n";

        my @finalunsort;

        if (scalar(@unsortcols))
        {
            for my $h_row (@{$ah1})
            {
                my @collist;

                @collist = ();

                for my $col (@unsortcols)
                {
                    if (exists($h_row->{$col}))
                    {
                        push @collist, $h_row->{$col};
                    }
                    else
                    {
                        my $maxcol = scalar(@allcols);
                        my $errstr = 
                            "specified UNSORT column out of range: $col vs $maxcol\n";
                        print $errstr;
                        print STDERR $errstr;
                        last;
                    }

                }
                push @finalunsort, join(' | ', @collist);
            }
            my @ggg= sort @finalunsort;

            for my $line (@ggg)
            {
                print $prefix, $line, "\n";
            }
        }

		if ($glob_verbose)
		{
			print "GP_IGNORE: end fqo $fqostate->{count}\n";
		}

        return;
    } # end order


    if ($has_order)
    {
        my @ggg= @{$outarr};

        if ($glob_ignore_whitespace)
        {
           my @ggg2;

           for my $line (@ggg)
           {
              # remove all leading, trailing whitespace (changes sorting)
              # and whitespace around column separators
              $line =~ s/^\s+//;
              $line =~ s/\s+$//;
              $line =~ s/\|\s+/\|/gm;
              $line =~ s/\s+\|/\|/gm;

              $line .= "\n" # replace linefeed if necessary
                unless ($line =~ m/\n$/);

              push @ggg2, $line;
           } 
           @ggg= @ggg2;
        }

        if ($glob_orderwarn)
        {
            # if no ordering cols specified (no directive), and
            # SELECT has ORDER BY, see if number of order
            # by cols matches all cols in selected lists
            if (exists($directive->{sql_statement})
                && (defined($directive->{sql_statement}))
                && ($directive->{sql_statement} =~ m/select.*order.*by/is))
            {
               my $fl2 = $directive->{firstline};
               my $sql_statement = $directive->{sql_statement};
               $sql_statement =~ s/\n/ /gm;
               my @ocols = 
                   ($sql_statement =~ m/select.*order.*by\s+(.*)\;/is);

#               print Data::Dumper->Dump(\@ocols);

               # lines already have newline terminator, so just rejoin them.
               my $line2 = join ("", @{$outarr});

               my $ah2 = tablelizer($line2, $fl2);
               my @allcols2;

#               print Data::Dumper->Dump([$ah2]);

               @allcols2 = (keys(%{$ah2->[0]}))
                 if (defined($ah2) && scalar(@{$ah2}));

               # treat the order by cols as a column separated list,
               # and count them.  works ok for simple ORDER BY clauses
               if (scalar(@ocols))
               {
                  my $ocolstr = shift @ocols;
                  my @ocols2  = split (/\,/, $ocolstr);
                  
                  if (scalar(@ocols2) < scalar(@allcols2))
                  {
				     print "GP_IGNORE: ORDER_WARNING: OUTPUT ",
                     scalar(@allcols2), " columns, but ORDER BY on ",
                     scalar(@ocols2), " \n";
                  }
               }
            }
        } # end if $glob_orderwarn

        for my $line (@ggg)
        {
            print $dpref, $prefix, $line;
        }
    }
    else
    {
        my @ggg= sort @{$outarr};

        if ($glob_ignore_whitespace)
        {
           my @ggg2;

           for my $line (@ggg)
           {
              # remove all leading, trailing whitespace (changes sorting)
              # and whitespace around column separators
              $line =~ s/^\s+//;
              $line =~ s/\s+$//;
              $line =~ s/\|\s+/\|/gm;
              $line =~ s/\s+\|/\|/gm;

              $line .= "\n" # replace linefeed if necessary
                unless ($line =~ m/\n$/);

              push @ggg2, $line;
           } 
           @ggg= sort @ggg2;
        }
        for my $line (@ggg)
        {
            print $bpref, $prefix, $line;
        }
    }

	if ($glob_verbose)
	{
		print "GP_IGNORE: end fqo $fqostate->{count}\n";
	}
}


sub bigloop
{
    my $sql_statement = "";
    my @outarr;

    my $getrows = 0;
    my $getstatement = 0;
    my $has_order = 0;
    my $copy_select = 0;
    my $directive = {};
    my $big_ignore = 0;
    my $define_match_expression = undef;
    my $error_detail_exttab_trifecta_skip = 0; # don't ask!
    my $verzion = "unknown";

    if (q$Revision: #1 $ =~ /\d+/)
    {
        $verzion = do { my @r = (q$Revision: #1 $ =~ /\d+/g); sprintf "%d."."%02d" x $#r, @r }; # must be all one line, for MakeMaker
    }
        my $format_fix = << "EOF_formatfix";
                                ))}
EOF_formatfix
    # NOTE: define $format_fix with HERE document just to fix emacs
    # indenting due to comment char in Q expression...

    $verzion = $0 . " version " . $verzion;
    print "GP_IGNORE: formatted by $verzion\n";

    my $do_equiv = $glob_compare_equiv || $glob_make_equiv_expected;

  L_bigwhile:
    while (<>) # big while
    {
        my $ini = $_;

        if ($error_detail_exttab_trifecta_skip)
        {
            $error_detail_exttab_trifecta_skip = 0;
            next;
        }

        # look for match/substitution or match/ignore expressions
        if (defined($define_match_expression))
        {
            unless (($ini =~ m/\-\-\s*end\_match(subs|ignore)\s*$/i))
            {
                $define_match_expression .= $ini;
                goto L_push_outarr;
            }

            my @foo = split(/\n/, $define_match_expression, 2);

            unless (2 == scalar(@foo))
            {
                $ini .= "GP_IGNORE: bad match definition\n";
                undef $define_match_expression;
                goto L_push_outarr;
            }

            my $stat;

            my $doc1 = $foo[1];

            # strip off leading comment characters
            $doc1 =~ s/^\s*\-\-//gm;

            if ($foo[0] =~ m/subs/)
            {
                $stat = _build_match_subs($doc1, "USER");
            }
            else
            {
                $stat = _build_match_ignores($doc1, "USER");
            }

            if (scalar(@{$stat}) > 1)
            {
                my $outi = $stat->[1];

                # print a message showing the error
                $outi =~ s/^(.*)/GP_IGNORE: ($1)/gm;
                $ini .= $outi;
            }
            else
            {
                $ini .=  "GP_IGNORE: defined new match expression\n";
            }

            undef $define_match_expression;
            goto L_push_outarr;
        } # end defined match expression

        if ($big_ignore > 0)
        {
            if (($ini =~ m/\-\-\s*end\_equiv\s*$/i) && !($do_equiv))
            { 
                $big_ignore -= 1;
            }
            if ($ini =~ m/\-\-\s*end\_ignore\s*$/i)
            { 
                $big_ignore -= 1;
            }
            print "GP_IGNORE:", $ini;
            next;
        }
        elsif (($ini =~ m/\-\-\s*end\_equiv\s*$/i) && $do_equiv)
        {
            $equiv_expected_rows = undef;
        }

        if ($ini =~ m/\-\-\s*end\_head(er|ers|ing|ings)\_ignore\s*$/i)
        {
            $glob_ignore_headers = 0;
        }

        if ($getrows) # getting rows from SELECT output
        {
            # special case for copy select
            if ($copy_select &&
                ($ini =~ m/(\-\-)|(ERROR)/))
            {
                my @ggg= sort @outarr;
                for my $line (@ggg)
                {
                    print $bpref, $line;
                }

                @outarr = ();
                $getrows = 0;
                $has_order = 0;
                $copy_select = 0;
                next;
            }


            # regex example: (5 rows)
            if ($ini =~ m/^\s*\(\d+\s+row(s)*\)\s*$/)
            {


                format_query_output($glob_fqo,
									$has_order, \@outarr, $directive);
          
      
                # Always ignore the rowcount for explain plan out as the skeleton plans might be the 
                # same even if the row counts differ because of session level GUCs. 
                if (exists($directive->{explain}))
                {
                    $ini = "GP_IGNORE:" . $ini;
                }

                $directive = {};
                @outarr = ();
                $getrows = 0;
                $has_order = 0;
            }
        }
        else # finding SQL statement or start of SELECT output
        { 
            if (($ini =~ m/\-\-\s*start\_match(subs|ignore)\s*$/i))
            {
                $define_match_expression = $ini;
                goto L_push_outarr;
            }
            if (($ini =~ m/\-\-\s*start\_ignore\s*$/i) ||
                (($ini =~ m/\-\-\s*start\_equiv\s*$/i) && !($do_equiv)))
            { 
                $big_ignore += 1;

                for my $line (@outarr)
                {
                    print $apref, $line;
                }
                @outarr = ();

                print "GP_IGNORE:", $ini;
                next;
            }
            elsif (($ini =~ m/\-\-\s*start\_equiv\s*$/i) && 
                   $glob_make_equiv_expected)
            {
                $equiv_expected_rows = [];
                $directive->{make_equiv_expected} = 1;
            }

            if ($ini =~ m/\-\-\s*start\_head(er|ers|ing|ings)\_ignore\s*$/i)
            {
                $glob_ignore_headers = 1;
            }

            # Note: \d is for the psql "describe"
            if ($ini =~ m/(insert|update|delete|select|\\d|copy)/i)
            {                
                $copy_select = 0;
                $has_order = 0;
                $sql_statement = "";

                if ($ini =~ m/explain.*(insert|update|delete|select)/i)
                { 
                   $directive->{explain} = "normal";
                }

            }

            if ($ini =~ m/\-\-\s*force\_explain\s+operator.*$/i)
            {
                # ENGINF-137: force_explain 
                $directive->{explain} = "operator";
            }
            if ($ini =~ m/\-\-\s*force\_explain\s*$/i)
            {
                # ENGINF-137: force_explain 
                $directive->{explain} = "normal";
            }
            if ($ini =~ m/\-\-\s*ignore\s*$/i)
            { 
                $directive->{ignore} = "ignore";
            }
            if ($ini =~ m/\-\-\s*order\s+\d+.*$/i)
            { 
                my $olist = $ini;
                $olist =~ s/^.*\-\-\s*order//;
                $directive->{order} = $olist;
            }
            if ($ini =~ m/\-\-\s*mvd\s+\d+.*$/i)
            { 
                my $olist = $ini;
                $olist =~ s/^.*\-\-\s*mvd//;
                $directive->{mvd} = $olist;
            }

            if ($ini =~ m/select/i)
            {
                $getstatement = 1;
            }
            if ($getstatement)
            {
                $sql_statement .= $ini;
            }
            if ($ini =~ m/\;/) # statement terminator
            {
                $getstatement = 0;
            }

            # prune notices with segment info if they are duplicates
#            if ($ini =~ m/^\s*(NOTICE|ERROR|HINT|DETAIL|WARNING)\:.*\s+\(seg.*pid.*\)/)
            if ($ini =~ m/^\s*(NOTICE|ERROR|HINT|DETAIL|WARNING)\:/)
            {
                $ini =~ s/\s+(\W)?(\W)?\(seg.*pid.*\)//;

				# also remove line numbers from errors
				$ini =~ s/\s+(\W)?(\W)?\(\w+\.[ch]:\d+\)/ (SOMEFILE:SOMEFUNC)/;
                my $outsize = scalar(@outarr);

                my $lastguy = -1;

              L_checkfor:
                for my $jj (1..$outsize)
                {
                    my $checkstr = $outarr[$lastguy];

                    #remove trailing spaces for comparison
                    $checkstr =~ s/\s+$//;

                    my $skinny = $ini;
                    $skinny =~ s/\s+$//;

                    # stop when no more notices
                    last L_checkfor
                        if ($checkstr !~ m/^\s*(NOTICE|ERROR|HINT|DETAIL|WARNING)\:/);

                    # discard this line if matches a previous notice
                    if ($skinny eq $checkstr)
                    {
                        if (0) # debug code
                        {
                            $ini = "DUP: " . $ini;
                            last L_checkfor;
                        }
                        next L_bigwhile;
                    }
                    $lastguy--;
                } # end for



            } # end if pruning notices

            # MPP-1492 allow:
            #  copy (select ...) to stdout
            #  \copy (select ...) to stdout
            # and special case these guys:
            #  copy test1 to stdout
            #  \copy test1 to stdout
            #
            # ENGINF-129:
            # and "copy...;  -- copy_stdout " for copy.out
            my $copysel_regex = 
            '^\s*((.copy.*test1.*to stdout)|(copy.*test1.*to stdout\;)|(copy.*\;\s*\-\-\s*copy\_stdout))';

            # regex example: ---- or ---+---
            # need at least 3 dashes to avoid confusion with "--" comments
            if (($ini =~ m/^\s*((\-\-)(\-)+(\+(\-)+)*)+\s*$/)
                # special case for copy select
                || (($ini =~ m/$copysel_regex/i)
                    && ($ini !~ m/order\s+by/i)))
            { # sort this region

                $directive->{firstline} = $outarr[-1];

                if (exists($directive->{order}) ||
                    exists($directive->{mvd}))
                {
                    $directive->{sortlines} = $outarr[-1];
                }

                # special case for copy select
                if ($ini =~ m/$copysel_regex/i)
                {
#                    print "copy select: $ini\n";
                    $copy_select = 1;
                    $sql_statement = "";
                }
                # special case for explain
               if (exists($directive->{explain}) &&
                   ($ini =~ m/^\s*((\-\-)(\-)+(\+(\-)+)*)+\s*$/) &&
                   ($outarr[-1] =~ m/QUERY PLAN/))
                {
                   # ENGINF-88: fixup explain headers
                   $outarr[-1] = "QUERY PLAN\n";
                   $ini = ("_" x length($outarr[-1])) . "\n";

                   if ($glob_ignore_headers)
                   {
                      $ini = "GP_IGNORE:" . $ini;
                   }
                }

                $getstatement = 0;

                # ENGINF-180: ignore header formatting
                # the last line of the outarr is the first line of the header
                if ($glob_ignore_headers && $outarr[-1])
                {
                    $outarr[-1] = "GP_IGNORE:" . $outarr[-1];
                }

                for my $line (@outarr)
                {
                    print $apref, $line;
                }
                @outarr = ();

                # ENGINF-180: ignore header formatting
                # the current line is the last line of the header
                if ($glob_ignore_headers
                    && ($ini =~ m/^\s*((\-\-)(\-)+(\+(\-)+)*)+\s*$/))
                {
                    $ini = "GP_IGNORE:" . $ini;
                }
                
                print $apref, $ini;

                if (defined($sql_statement)
                    && length($sql_statement)
                    # multiline match 
                    && ($sql_statement =~ m/select.*order.*by/is))
                {
                    $has_order = 1; # so do *not* sort output

#                   $sql_statement =~ s/\n/ /gm;
#                   print "has order: ", $sql_statement, "\n";
                    $directive->{sql_statement} = $sql_statement;
                }
                else
                {
                    $has_order = 0; # need to sort query output

#                    $sql_statement =~ s/\n/ /gm;
#                    print "no order: ", $sql_statement, "\n";
                    $directive->{sql_statement} = $sql_statement;
                }
                $sql_statement = "";

                $getrows = 1;
                next;
            } # end sort this region
        } # end finding SQL


        # if MATCH then SUBSTITUTE
        # see HERE document for definitions
        $ini = match_then_subs($ini);

        if ($ini =~ m/External table .*line (\d)+/)
        {
            $ini =~ s/External table .*line (\d)+.*/External table DUMMY_EX, line DUMMY_LINE of DUMMY_LOCATION/;
              $ini =~ s/\s+/ /;
             # MPP-1557,AUTO-3: horrific ERROR DETAIL External Table trifecta
			if ($glob_verbose)
			{
				print "GP_IGNORE: External Table ERROR DETAIL fixup\n";
			}
             if ($ini !~ m/^DETAIL/)
             {
                # find a "blank" DETAIL tag preceding current line
                if (scalar(@outarr) && ($outarr[-1] =~ m/^DETAIL:\s+$/))
                {
                   pop @outarr;
                   $ini = "DETAIL: " . $ini;
                   $ini =~ s/\s+/ /;
                   # need to skip the next record
                   $error_detail_exttab_trifecta_skip = 1;
                }
             }
             if (scalar(@outarr) && 
                 ($outarr[-1] =~ m/^ERROR:\s+missing\s+data\s+for\s+column/))
             {
                $outarr[-1] = "ERROR:  missing data for column DUMMY_COL\n";
             }

        }

        # if MATCH then IGNORE
        # see HERE document for definitions
        if ( match_then_ignore($ini))
        {
           next; # ignore matching lines
        }

L_push_outarr:

        push @outarr, $ini;

    } # end big while

    for my $line (@outarr)
    {
        print $cpref, $line;
    }
} # end bigloop

if (1)
{
	goto l_big_bigloop
		unless (defined($glob_init) && scalar(@{$glob_init}));

	# ENGINF-200: allow multiple init files
	for my $init_file (@{$glob_init})
	{
		die "no such file: $init_file"
			unless (-e $init_file);

		# redirect stdin and stdout to perform initialization from
		# the init_file file

		open my $oldout, ">&STDOUT"     or die "Can't dup STDOUT: $!";
		
		close STDOUT;
		open (STDOUT, "> /dev/null" ) or die "can't open STDOUT: $!";
 
		open my $oldin, "<&STDIN"     or die "Can't dup STDIN: $!";
		close STDIN;
		open STDIN, "< $init_file" or die "could not open $init_file: $!";

		# run the standard loop

		bigloop();

		# reset stdin, stdout
		
		close STDIN;
		open STDIN, "<&", $oldin or die "Can't dup \$oldin: $!";

		close STDOUT;
		open STDOUT, ">&", $oldout or die "Can't dup \$oldout: $!";
	}

l_big_bigloop:
	# loop over existing stdin
	bigloop();
}


exit();
