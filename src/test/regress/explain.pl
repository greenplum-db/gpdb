#!/usr/bin/env perl
#
# Portions Copyright (c) 2006, 2007, 2008, 2009 Greenplum Inc
# Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
#
# Author: Jeffrey I Cohen
#
#
use Pod::Usage;
use Getopt::Long;
use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin";
use explain;
use GPTest qw(print_version);

=head1 NAME

B<explain.pl> - parse and reformat Postgres EXPLAIN output

=head1 SYNOPSIS

B<explain> [options] filename

Options:

    -help            brief help message
    -man             full documentation
    -option          formatting option: perl, yaml, dot, query, jpg, json
    -querylist       list of queries
    -direction       direction of query plan graph: LR, RL, TB or BT.
    -colorscheme     graph color scheme
    -timeline        rank nodes by start offset time (experimental)
    -prune           prune tree attributes
    -output          output filename (else output to STDOUT).
    -statcolor       statistics coloring (experimental)
    -edge            edge decorations

=head1 OPTIONS

=over 8

=item B<-help>

    Print a brief help message and exits.

=item B<-man>
    
    Prints the manual page and exits.

=item B<-option>

    Choose the output format option.  Several formats are supported.
    
=over 12

=item perl:  output in perl L<Data::Dumper> format.

=item yaml:  output in L<yaml.org> machine and human-readable format

=item dot:   output in dot graphical language for L<graphiz.org> graphing tool.

=item querytext: output the text of the query (only useful for TPC-H)

=item jpg:   pipe the dot output thru the dot formatter (if it is installed) to get jpg output directly.  (May also support bmp, ps, pdf, png)
    
=item json:  output in L<json.org> machine and human-readable format

=back


=item B<-querylist>

   A list of queries to process.  The query numbering is 1-based. Some
   valid forms are:

   -querylist 1 
   -querylist=2 
   --ql=3,4,5
   --ql=6-9

    or some combination. By default, all queries are processed.


=item B<-direction>

   Direction of data flow in query plan graph.  Valid entries are:

=over 12

=item BT (default): bottom to top

=item TB: top to bottom

=item LR: left to right

=item RL: right to left
    
=back

=item B<-colorscheme>

   One of the supported ColorBrewer(TM) color schemes.  Use 
   -color ? 
   to get a list of the valid schemes, and
   -color dump
   to output a dot file displaying all the valid schemes.

    Colors from www.ColorBrewer.org by Cynthia A. Brewer, Geography, 
    Pennsylvania State University.
    
=item B<-prune>

   Prune tree attributes.  The only supported option is "stats" to
   remove the to_end and to_first timing information.

=item B<-output>

   Output file name.  If multiple queries are processed, the filename
   is used as a template to generate multiple files.  If the filename
   has an extension, it is preserved, else an extension is supplied
   based upon the formatting option.  The filename template inserts
   the query number before the "dot" (.) if more than one query was
   processed.

=item B<-statcolor>

    For an EXPLAIN ANALYZE plan, color according to the time spent in
    node. Red is greatest, and blue is least.  For statcolor=ts (default), 
    the node edge is colored by time, and the node interior is filled 
    by slice color.  For statcolor=st, the color scheme is reversed.  
    For statcolor=t (timing only), the entire node is colored according 
    to the time spent.

=item B<-edge>

    Decorate graph edges with row count if available. Valid entries are:

=over 12

=item long   - print average rows and number of workers

=item medium - print average rows and number of workers compactly

=item short  - print total row counts

=back


=back

=head1 DESCRIPTION

explain.pl reads EXPLAIN output from a text file (or standard
input) and formats it in several ways.  The text file must contain
output in one of the following formats.  The first is a regular
EXPLAIN format, starting the QUERY PLAN header and ending with the
number of rows in parentheses.  Indenting must be on:

                                                QUERY PLAN                                                
 ----------------------------------------------------------------------------------------------------------
  Gather Motion 64:1  (slice2)  (cost=6007722.78..6007722.79 rows=6 width=51)
    Merge Key: partial_aggregation.l_returnflag, partial_aggregation.l_linestatus
    ->  Sort  (cost=6007722.78..6007722.79 rows=6 width=51)
          Sort Key: partial_aggregation.l_returnflag, partial_aggregation.l_linestatus
          ->  HashAggregate  (cost=6007722.52..6007722.70 rows=6 width=51)
                Group By: lineitem.l_returnflag, lineitem.l_linestatus
                ->  Redistribute Motion 64:64  (slice1)  (cost=6007721.92..6007722.31 rows=6 width=51)
                      Hash Key: lineitem.l_returnflag, lineitem.l_linestatus
                      ->  HashAggregate  (cost=6007721.92..6007722.19 rows=6 width=51)
                            Group By: lineitem.l_returnflag, lineitem.l_linestatus
                            ->  Seq Scan on lineitem  (cost=0.00..3693046.50 rows=92587017 width=51)
                                  Filter: l_shipdate <= '1998-09-08 00:00:00'::timestamp without time zone
 (12 rows)
 

The second acceptable format is the TPC-H EXPLAIN ANALYZE, listing
each query followed by the EXPLAIN output delineated by vertical bars
('|', e.g. |QUERY PLAN| ):

 EXPLAIN ANALYZE 
 
 
 select
     l_returnflag,
     l_linestatus,
     sum(l_quantity) as sum_qty,
     sum(l_extendedprice) as sum_base_price,
     sum(l_extendedprice * (1 - l_discount)) as sum_disc_price,
     sum(l_extendedprice * (1 - l_discount) * (1 + l_tax)) as sum_charge,
     avg(l_quantity) as avg_qty,
     avg(l_extendedprice) as avg_price,
     avg(l_discount) as avg_disc,
     count(*) as count_order
 from
     lineitem
 where
     l_shipdate <= date '1998-12-01' - interval '106 day'
 group by
     l_returnflag,
     l_linestatus
 order by
     l_returnflag,
     l_linestatus;
 
 
  Query 1 complete, 19 rows returned
 
 
 |QUERY PLAN|
 |Gather Motion 64:1  (slice2)  (cost=5990545.19..5990545.21 rows=6 width=51)|
 |  recv:  Total 4 rows with 1294937 ms to end.|
 |  Merge Key: partial_aggregation.junk_attr_1, partial_aggregation.junk_attr_2|
 |  ->  Sort  (cost=5990545.19..5990545.21 rows=6 width=51)|
 |        Avg 1.00 rows x 4 workers.  Max 1 rows (seg49) with 1294938 ms to end.|
 |        Sort Key: partial_aggregation.junk_attr_1, partial_aggregation.junk_attr_2|
 |        ->  HashAggregate  (cost=5990544.94..5990545.12 rows=6 width=51)|
 |              Avg 1.00 rows x 4 workers.  Max 1 rows (seg49) with 1294933 ms to end.|
 |              Group By: lineitem.l_returnflag, lineitem.l_linestatus|
 |              ->  Redistribute Motion 64:64  (slice1)  (cost=5990544.34..5990544.73 rows=6 width=51)|
 |                    recv:  Avg 64.00 rows x 4 workers.  Max 64 rows (seg49) with 1277197 ms to first row, 1294424 ms to end.|
 |                    Hash Key: lineitem.l_returnflag, lineitem.l_linestatus|
 |                    ->  HashAggregate  (cost=5990544.34..5990544.61 rows=6 width=51)|
 |                          Avg 4.00 rows x 64 workers.  Max 4 rows (seg44) with 1292222 ms to end.|
 |                          Group By: lineitem.l_returnflag, lineitem.l_linestatus|
 |                          ->  Seq Scan on lineitem  (cost=0.00..3693046.50 rows=91899913 width=51)|
 |                                Avg 91914578.95 rows x 64 workers.  Max 91914598 rows (seg13) with 14.694 ms to first row, 258614 ms to end.|
 |                                Filter: l_shipdate <= '1998-08-17 00:00:00'::timestamp without time zone|
 |1295317.560 ms elapsed|
 
 Time was 1295.33 seconds.  Query ended at Thu Oct 12 12:09:27 2006
 
=head1 CAVEATS/LIMITATIONS

If explain.pl uses Graphviz to graph the query plan, it may flip the
left and right children of a join to obtain a more balanced pictorial
representation.  Use the -edge option to label graph edges to
correctly identify the left and right children.

=head1 AUTHORS

Jeffrey I Cohen

Portions Copyright (c) 2006, 2007, 2008, 2009 GreenPlum.  All rights reserved.
Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.

Address bug reports and comments to: bugs@greenplum.org


=cut

my $glob_id = "";

my $man  = 0;
my $help = 0;
my $optn = "YAML";
my $dir  = "BT";
my $DEFAULT_COLOR = "set28";
my $colorscheme = $DEFAULT_COLOR;
my $timeline = '';
my $prune;
my $outfile;
my $statcol;
my $edgescheme; 

my @qlst;

GetOptions(
    'help|?' => \$help, man => \$man, 
    "querylist|ql|list:s" => \@qlst,
    "option|operation=s" => \$optn,
    "direction:s" => \$dir,
    "colorscheme:s" => \$colorscheme,
    "timeline" => \$timeline,
    "prune:s" => \$prune,
    "output:s" => \$outfile,
    "statcolor:s" => \$statcol,
    "edge:s" => \$edgescheme,
    'version|v' => \&print_version)
    or pod2usage(2);

    
pod2usage(-msg => $glob_id, -exitstatus => 1) if $help;
pod2usage(-msg => $glob_id, -exitstatus => 0, -verbose => 2) if $man;

my %args;

@{$args{QUERY_LIST}} = @qlst		if (scalar(@qlst));
$args{OPERATION} = $optn			if (defined($optn));
$args{DIRECTION} = $dir				if (defined($dir));
$args{COLOR_SCHEME} = $colorscheme	if (defined($colorscheme));
$args{TIMELINE} = $timeline			if (defined($timeline));
$args{PRUNE} = $prune				if (defined($prune));
$args{OUTPUT} = $outfile			if (defined($outfile));
$args{STATCOLOR} = $statcol			if (defined($statcol));
$args{EDGE_SCHEME} = $edgescheme	if (defined($edgescheme));
$args{INPUT_FH} = \*STDIN;

explain::explain_init(%args);

explain::run();
