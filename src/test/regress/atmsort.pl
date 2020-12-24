#!/usr/bin/env perl
#
# Portions Copyright (c) 2007, 2008, 2009 GreenPlum.  All rights reserved.
# Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
#
# Author: Jeffrey I Cohen
#
# Pod::Usage is loaded lazily when needed, if the --help or other such option
# is actually used. Loading the module takes some time, which adds up when
# running hundreds of regression tests, and gpdiff.pl calls this script twice
# for every test. See lazy_pod2usage().
#use Pod::Usage;

use Getopt::Long;
#use Data::Dumper; # only used by commented-out debug statements.
use strict;
use warnings;

use File::Spec;

use FindBin;
use lib "$FindBin::Bin";
use atmsort;
use GPTest qw(print_version);

=head1 NAME

B<atmsort.pl> - [A] [T]est [M]echanism Sort: sort the contents of SQL log files to aid diff comparison

=head1 SYNOPSIS

B<atmsort.pl> [options] logfile [logfile...]

Options:

    -help            brief help message
    -man             full documentation
    -ignore_plans    ignore explain plan content in query output
    -init <file>     load initialization file

=head1 OPTIONS

=over 8

=item B<-help>

    Print a brief help message and exits.

=item B<-man>
    
    Prints the manual page and exits.

=item B<-ignore_plans> 

Specify this option to ignore any explain plan diffs between the
input files. This will completely ignore any plan content in 
the input files thus masking differences in plans between the input files.

For example, for the following plan: 
explain select i from foo where i > 10;
                                 QUERY PLAN
-----------------------------------------------------------------------------
 Gather Motion 2:1  (slice1; segments: 2)  (cost=0.00..2.72 rows=45 width=4)
   ->  Seq Scan on foo  (cost=0.00..1.55 rows=45 width=4)
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
GP_IGNORE:      'short' => 'Seq Scan on foo'
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
PostgreSQL.

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
output in the standard PostgreSQL format, ie a set of named columns, a
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

=over 13

=item -- order column number[, column number...]

  The order directive is used to compare 
  "partially-ordered" query
  output.  The specified columns are assumed 
  to be ordered, and the  remaining columns are 
  sorted to allow for deterministic comparison.

=item -- order none

  The order none directive can be used to specify that the SELECT's
  output is not ordered. This can be necessary if the default
  heuristic that checks if there is an ORDER BY in the query gets
  fooled, e.g by an ORDER BY in a subquery that doesn't force the
  overall result to be ordered.

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

=item -- explain_processing_on

Enables the automated processing of the explain output, removing all
the variable fields using the `explain` perl module
This is the default.

=item -- explain_processing_off

Do not process the explain output. This might result in test failures
if the test does not replace the variable values itself.
An example of a test that uses this is the test that validates the
format of the explain output.

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

Address bug reports and comments to: bugs@greenplum.org


=cut

# Calls pod2usage, but loads the module first.
sub lazy_pod2usage
{
    require Pod::Usage;
    Pod::Usage::pod2usage(@_);
}

my $glob_id = "";

my $glob_init;

my $glob_orderwarn;
my $glob_verbose;
my $glob_fqo;

my $man  = 0;
my $help = 0;
my $ignore_plans;
my @init_file;
my $verbose;
my $orderwarn;

GetOptions(
    'help|?' => \$help, man => \$man, 
    'gpd_ignore_plans|gp_ignore_plans|ignore_plans' => \$ignore_plans,
    'gpd_init|gp_init|init:s' => \@init_file,
    'order_warn|orderwarn' => \$orderwarn,
    'verbose' => \$verbose,
    'version|v' => \&print_version
    )
    or lazy_pod2usage(2);

lazy_pod2usage(-msg => $glob_id, -exitstatus => 1) if $help;
lazy_pod2usage(-msg => $glob_id, -exitstatus => 0, -verbose => 2) if $man;

# ENGINF-200: allow multiple init files
push @{$glob_init}, @init_file;

my %args;

$args{IGNORE_PLANS} = $ignore_plans if (defined ($ignore_plans));
@{$args{INIT_FILES}} = @init_file if (scalar(@init_file));
$args{ORDER_WARN} = $orderwarn if (defined ($orderwarn));
$args{VERBOSE} = $verbose if (defined ($verbose));

atmsort::atmsort_init(%args);

atmsort::run_fhs(*STDIN, *STDOUT);

exit();
