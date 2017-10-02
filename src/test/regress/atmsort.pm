#
# This is the workhorse of atmsort.pl, extracted into a module so that it
# can be called more efficiently from other perl programs.
#
# Public interface:
#
#   atmsort_init(args in a hash)
#
# followed by:
#
#   run_fhs(input file handle, output file handle)
# or
#   run(input filename, output filename)
#
package atmsort;

#use Data::Dumper; # only used by commented-out debug statements.
use strict;
use warnings;
use File::Temp qw/ tempfile /;

# Load explain.pm from the same directory where atmsort.pm is.
use FindBin;
use lib "$FindBin::Bin";
use explain;

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
my $glob_ignore_plans;
my $glob_ignore_whitespace;
my @glob_init;

my $glob_orderwarn;
my $glob_verbose;
my $glob_fqo;

my $atmsort_outfh;

# array of "expected" rows from first query of equiv region
my $equiv_expected_rows;

sub atmsort_init
{
    my %args = (
        # defaults
        IGNORE_HEADERS  => 0,
        IGNORE_PLANS    => 0,
        INIT_FILES      => [],
        DO_EQUIV        => 'ignore',    # can be 'ignore', 'compare', or 'make'
        ORDER_WARN      => 0,
        VERBOSE         => 0,

        # override the defaults from argument list
        @_
    );

    $glob_compare_equiv       = 0;
    $glob_make_equiv_expected = 0;
    $glob_ignore_plans        = 0;
    $glob_ignore_whitespace   = 0;
    @glob_init                = ();

    $glob_orderwarn           = 0;
    $glob_verbose             = 0;
    $glob_fqo                 = {count => 0};

    my $compare_equiv = 0;
    my $make_equiv_expected = 0;
    my $do_equiv;
    my $ignore_headers;
    my $ignore_plans;
    my @init_file;
    my $verbose;
    my $orderwarn;

    if ($args{DO_EQUIV} =~ m/^ignore/i)
    {
        # ignore all - default
    }
    elsif ($args{DO_EQUIV} =~ m/^compare/i)
    {
        # compare equiv region
        $compare_equiv = 1;
    }
    elsif ($args{DO_EQUIV} =~ m/^make/i)
    {
        # make equiv expected output
        $make_equiv_expected = 1;
    }
    else
    {
        die "unknown do_equiv option: $do_equiv\nvalid options are:\n\tdo_equiv=compare\n\tdo_equiv=make";
    }
    $glob_compare_equiv       = $compare_equiv;
    $glob_make_equiv_expected = $make_equiv_expected;

    $glob_ignore_plans        = $args{IGNORE_PLANS};

    $glob_ignore_whitespace   = $ignore_headers; # XXX XXX: for now

    @glob_init = @{$args{INIT_FILES}};

    $glob_orderwarn           = $args{ORDER_WARN};
    $glob_verbose             = $args{VERBOSE};

    init_match_subs();
    init_matchignores();

    _process_init_files();
}

sub _process_init_files
{
    # allow multiple init files
    if (@glob_init)
    {
        my $devnullfh;
        my $init_file_fh;

        open $devnullfh, "> /dev/null" or die "can't open /dev/null: $!";

        for my $init_file (@glob_init)
        {
            die "no such file: $init_file"
            unless (-e $init_file);

            # Perform initialization from this init_file by passing it
            # to bigloop. Open the file, and pass that as the input file
            # handle, and redirect output to /dev/null.
            open $init_file_fh, "< $init_file" or die "could not open $init_file: $!";

            atmsort_bigloop($init_file_fh, $devnullfh);

            close $init_file_fh;
        }

        close $devnullfh;
    }
}

my $glob_match_then_sub_fnlist;

sub _build_match_subs
{
    my ($here_matchsubs, $whomatch) = @_;

    my $stat = [1];

    # filter out the comments and blank lines
    $here_matchsubs =~ s/^\s*(?:#.*)?(?:[\r\n]|\x0D\x0A)//gm;

    # split up the document into separate lines
    my @foo = split(/\n/, $here_matchsubs);

    my $ii = 0;

    my $matchsubs_arr = [];
    my $msa;

    # build an array of arrays of match/subs pairs
    while ($ii < scalar(@foo))
    {
        my $lin = $foo[$ii];

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

            if ($glob_verbose && defined $atmsort_outfh)
            {
                print $atmsort_outfh "GP_IGNORE: Defined $cmt\t$defi->[0]\t$defi->[1]\n"
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
sub init_match_subs
{
    my $here_matchsubs;


# construct a "HERE" document of match expressions followed by
# substitution expressions.  Embedded comments and blank lines are ok
# (they get filtered out).

    $here_matchsubs = << 'EOF_matchsubs';

# some cleanup of greenplum-specific messages
m/\s+\(seg.*pid.*\)/
s/\s+\(seg.*pid.*\)//

# distributed transactions
m/^(?:ERROR|WARNING|CONTEXT|NOTICE):.*gid\s+=\s+(?:\d+)/
s/gid.*/gid DUMMY/

m/^(?:ERROR|WARNING|CONTEXT|NOTICE):.*connection.*failed.*(?:http|gpfdist)/
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
                print $atmsort_outfh "GP_IGNORE: was: $ini";
                print $atmsort_outfh "GP_IGNORE: matched $ff->[-3]\t$ff->[-2]\t$ff->[-1]\n"
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
    $here_matchignores =~ s/^\s*(?:#.*)?(?:[\r\n]|\x0D\x0A)//gm;

    # split up the document into separate lines
    my @matchignores_arr = split(/\n/, $here_matchignores);

    my $bigdef;

    my $fn1;

    # build a lambda function for each expression, and load it into an
    # array
    my $mscount = 1;

    for my $defi (@matchignores_arr)
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
            if ($glob_verbose && defined $atmsort_outfh)
            {
                print $atmsort_outfh "GP_IGNORE: Defined $cmt\t$defi\n"
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
sub init_matchignores
{
    my $here_matchignores;

# construct a "HERE" document of match expressions to ignore in input.
# Embedded comments and blank lines are ok (they get filtered out).

    $here_matchignores = << 'EOF_matchignores';

m/^NOTICE:  Dropping a column that is part of the distribution policy/

m/^NOTICE:  Table has parent\, setting distribution columns to match parent table/

m/^WARNING:  Referential integrity \(.*\) constraints are not supported in Greenplum Database/

        # ignore notices for DROP sqlobject IF EXISTS "objectname"
        # eg NOTICE:  table "foo" does not exist, skipping
        #
        # the NOTICE is different from the ERROR case, which does not
        # end with "skipping"
m/^NOTICE:  \w+ \".*\" does not exist\, skipping\s*$/

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
                print $atmsort_outfh "GP_IGNORE: matched $ff->[-3]\t$ff->[-2]\t$ff->[-1]\n"
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

    $colheads[0] =~ s/^(\s+|\s+$)//;
    $colheads[-1] =~ s/^(\s+|\s+$)//;

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

            $rawcol =~ s/^(\s+|\s+$)//;

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

    $directive = {} unless (defined($directive));

    # Ignore plan content if its between start_ignore and end_ignore blocks
    # or if -ignore_plans is specified.
    $prefix = "GP_IGNORE:"
         if (exists($directive->{ignore})) || ($glob_ignore_plans);

    my @tmp_lines;
    my $sort = 0;

    if (scalar(@{$outarr}))
    {
        @tmp_lines = (
            "QUERY PLAN\n",
            ("-" x 71) . "\n",
            @{$outarr},
            "(111 rows)\n"
            );
    }

    if (exists($directive->{explain})
        && ($directive->{explain} =~ m/operator/i))
    {
        $xopt = 'operator';
        $sort = 1;
    }

    my $xplan = '';

    open(my $xplan_fh, ">", \$xplan)
        or die "Can't open in-memory file handle to variable: $!";

    explain::explain_init(OPERATION => $xopt,
                  PRUNE => 'heavily',
                  INPUT_LINES => \@tmp_lines,
                  OUTPUT_FH => $xplan_fh);

    explain::run();

    close $xplan_fh;

    my @lines = split /\n/, $xplan;

    if ($sort && scalar(@lines) > 0)
    {
        @lines = sort @lines;
    }

    # Apply prefix to each line, if requested.
    if (defined($prefix) && length($prefix))
    {
        foreach my $line (@lines)
        {
            $line = $prefix . $line;
        }
    }

    # Put back newlines and print
    foreach my $line (@lines)
    {
        $line .= "\n";
        print $atmsort_outfh $line;
    }

    return  \@lines;
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
        print $atmsort_outfh "GP_IGNORE: start fqo $fqostate->{count}\n";
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
                print $atmsort_outfh "GP_IGNORE: end fqo $fqostate->{count}\n";
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
           print $atmsort_outfh "GP_IGNORE: end fqo $fqostate->{count}\n";
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
                print $atmsort_outfh "GP_IGNORE: end fqo $fqostate->{count}\n";
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
                    print $atmsort_outfh "invalid colset for $item\n";
                    print STDERR "invalid colset for $item\n";
                    next;
                }
                # specification columns (LHS)
                my @scols = split(/\,/, $colset[0]);
                unless (scalar(@scols))
                {
                    print $atmsort_outfh "invalid dependency specification: $colset[0]\n";
                    print STDERR
                        "invalid dependency specification: $colset[0]\n";
                    next;
                }
                # dependent columns (RHS)
                my @dcols = split(/\,/, $colset[1]);
                unless (scalar(@dcols))
                {
                    print $atmsort_outfh "invalid specified dependency: $colset[1]\n";
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

        my %unsorth = map { $_ => 1 } @allcols;

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
                        print $atmsort_outfh $errstr;
                        print STDERR $errstr;
                        last;
                    }
                }
                print $atmsort_outfh $prefix, join(' | ', @collist), "\n";
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
                    print $atmsort_outfh $prefix, $line, "\n";
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
                print $atmsort_outfh $prefix, $line, "\n";
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
              $line =~ s/^(\s+|\s+$)//;
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
            # If no ordering cols specified (no directive), and SELECT has
            # ORDER BY, see if number of order by cols matches all cols in
            # selected lists. Treat the order by cols as a column separated
            # list and count them. Works ok for simple ORDER BY clauses
            if (defined($directive->{sql_statement}))
            {
                my @ocols = ($directive->{sql_statement} =~ m/select.*order\s+by\s+(.*)\;/ism);

                if (scalar(@ocols))
                {
                    my $fl2 = $directive->{firstline};
                    # lines already have newline terminator, so just rejoin them.
                    my $line2 = join ("", @{$outarr});

                    my $ah2 = tablelizer($line2, $fl2);
                    if (defined($ah2) && scalar(@{$ah2}))
                    {
                        my $allcol_count = scalar(keys(%{$ah2->[0]}));

                        # In order to count the number of ORDER BY columns we
                        # can transliterate over comma and increment by one to
                        # account for the last column not having a trailing
                        # comma. This is faster than splitting over the comma
                        # since we don't need to allocate the returned array.
                        my $ocol_count = ($ocols[0] =~ tr/,//) + 1;

                        if ($ocol_count < $allcol_count)
                        {
                            print "GP_IGNORE: ORDER_WARNING: OUTPUT ",
                            $allcol_count, " columns, but ORDER BY on ",
                            $ocol_count, " \n";
                        }
                    }
                }
            }
        } # end if $glob_orderwarn

        for my $line (@ggg)
        {
            print $atmsort_outfh $dpref, $prefix, $line;
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
              $line =~ s/^(\s+|\s+$)//;
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
            print $atmsort_outfh $bpref, $prefix, $line;
        }
    }

    if ($glob_verbose)
    {
        print "GP_IGNORE: end fqo $fqostate->{count}\n";
    }
}


# The caller should've opened ATMSORT_INFILE and ATMSORT_OUTFILE file handles.
sub atmsort_bigloop
{
    my $infh = shift;
    $atmsort_outfh = shift;

    my $sql_statement = "";
    my @outarr;

    my $getrows = 0;
    my $getstatement = 0;
    my $has_order = 0;
    my $copy_to_stdout_result = 0;
    my $directive = {};
    my $big_ignore = 0;
    my %define_match_expression;

    print $atmsort_outfh "GP_IGNORE: formatted by atmsort.pm\n";

    my $do_equiv = $glob_compare_equiv || $glob_make_equiv_expected;

  L_bigwhile:
    while (<$infh>) # big while
    {
      reprocess_row:
        my $ini = $_;

        # look for match/substitution or match/ignore expressions
        if (%define_match_expression)
        {
            if ($ini =~ m/\-\-\s*end\_match(subs|ignore)\s*$/)
            {
                if ($define_match_expression{"type"} ne $1)
                {
                    die "Non-matching operation end_match" . $1 . ", " .
                        "expected end_match" . $define_match_expression{"type"};
                }
            }
            else
            {
                $define_match_expression{"expr"} .= $ini;
                goto L_push_outarr;
            }

            my @foo = split(/\n/, $define_match_expression{"expr"}, 2);

            unless (2 == scalar(@foo))
            {
                $ini .= "GP_IGNORE: bad match definition\n";
                undef %define_match_expression;
                goto L_push_outarr;
            }

            my $stat;

            my $doc1 = $foo[1];

            # strip off leading comment characters
            $doc1 =~ s/^\s*\-\-//gm;

			if ($define_match_expression{"type"} eq 'subs')
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

            undef %define_match_expression;
            goto L_push_outarr;
        } # end defined match expression

        if ($big_ignore > 0)
        {
            if (!$do_equiv && $ini =~ m/\-\-\s*end\_equiv\s*$/)
            {
                $big_ignore--;
            }
            if ($ini =~ m/\-\-\s*end\_ignore\s*$/)
            {
                $big_ignore--;
            }
            print $atmsort_outfh "GP_IGNORE:", $ini;
            next;
        }
        elsif ($do_equiv && $ini =~ m/\-\-\s*end\_equiv\s*$/)
        {
            $equiv_expected_rows = undef;
        }

        if ($getrows) # getting rows from SELECT output
        {
            # The end of "result set" for a COPY TO STDOUT is a bit tricky
            # to find. There is no explicit marker for it. We look for a
            # line that looks like a SQL comment or a new query, or an ERROR.
            # This is not bullet-proof, but works for the current tests.
            if ($copy_to_stdout_result &&
                ($ini =~ m/(?:\-\-|ERROR|copy|create|drop|select|insert|update)/i))
            {
                my @ggg = sort @outarr;
                for my $line (@ggg)
                {
                    print $atmsort_outfh $bpref, $line;
                }

                @outarr = ();
                $getrows = 0;
                $has_order = 0;
                $copy_to_stdout_result = 0;

                # Process the row again, in case it begins another
                # COPY TO STDOUT statement, or another query.
                goto reprocess_row;
            }

            # regex example: (5 rows)
            if ($ini =~ m/^\s*\(\d+\s+row(?:s)*\)\s*$/)
            {
                format_query_output($glob_fqo,
                                    $has_order, \@outarr, $directive);

                # Always ignore the rowcount for explain plan out as the
                # skeleton plans might be the same even if the row counts
                # differ because of session level GUCs.
                if (exists($directive->{explain}))
                {
                    $ini = 'GP_IGNORE:' . $ini;
                }

                $directive = {};
                @outarr = ();
                $getrows = 0;
                $has_order = 0;
            }
        }
        else # finding SQL statement or start of SELECT output
        {
			# To avoid hunting for gpdiff commands which are contained inside
			# comments first establish if the line contains a comment with any
			# trailing characters at all.
			my $has_comment = ((m/\s*\-\-.+$/) ? 1 : 0);

            if ($has_comment && $ini =~ m/\-\-\s*start\_match(subs|ignore)\s*$/)
            {
                $define_match_expression{"type"} = $1;
                $define_match_expression{"expr"} = $ini;
                goto L_push_outarr;
            }
            if ($has_comment && (($ini =~ m/\-\-\s*start\_ignore\s*$/) ||
                (!$do_equiv && ($ini =~ m/\-\-\s*start\_equiv\s*$/))))
            {
                $big_ignore += 1;

                for my $line (@outarr)
                {
                    print $atmsort_outfh $apref, $line;
                }
                @outarr = ();

                print $atmsort_outfh 'GP_IGNORE:', $ini;
                next;
            }
            elsif ($has_comment && ($glob_make_equiv_expected && $ini =~ m/\-\-\s*start\_equiv\s*$/))
            {
                $equiv_expected_rows = [];
                $directive->{make_equiv_expected} = 1;
            }

            # Note: \d is for the psql "describe"
            if ($ini =~ m/(?:insert|update|delete|select|\\d|copy|execute)/i)
            {
                $copy_to_stdout_result = 0;
                $has_order = 0;
                $sql_statement = "";

                if ($ini =~ m/explain.*(?:insert|update|delete|select|execute)/i)
                {
                    $directive->{explain} = 'normal';
                }
            }

			# Catching multiple commands and capturing the parens matches
			# makes it possible to check just the first character since
			# each command has a unique first character. This allows us to
			# use fewer regular expression matches in this hot section.
			if ($has_comment &&
				$ini =~ m/\-\-\s*((force_explain)\s*(operator)?\s*$|(ignore)\s*$|(order)\s+\d+.*$|(mvd)\s+\d+.*$)/)
			{
				my $cmd = substr($1, 0, 1);
				if ($cmd eq 'i')
				{
					$directive->{ignore} = 'ignore';
				}
				elsif ($cmd eq 'o')
				{
					my $olist = $ini;
					$olist =~ s/^.*\-\-\s*order//;
					$directive->{order} = $olist;
				}
				elsif ($cmd eq 'f')
				{
					if (defined($3))
					{
						$directive->{explain} = 'operator';
					}
					else
					{
						$directive->{explain} = 'normal';
					}
				}
				else
				{
					my $olist = $ini;
					$olist =~ s/^.*\-\-\s*mvd//;
					$directive->{mvd} = $olist;
				}
			}

            if ($ini =~ m/select/i)
            {
                $getstatement = 1;
                $sql_statement .= $ini;
            }
			if (index($ini, ';') != -1)
            {
                $getstatement = 0;
            }

            # prune notices with segment info if they are duplicates
            if ($ini =~ m/^\s*(?:NOTICE|ERROR|HINT|DETAIL|WARNING)\:.*\(seg.*pid.*\)/)
            {
                $ini =~ s/\s+(?:\W)?(?:\W)?\(seg.*pid.*\)//;

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
                        if ($checkstr !~ m/^\s*(?:NOTICE|ERROR|HINT|DETAIL|WARNING)\:/);

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
            my $matches_copy_to_stdout = 0;
            if ($ini =~ m/^(?:\\)?copy\s+(?:(?:\(select.*\))|\w+)\s+to stdout.*$/i)
            {
                $matches_copy_to_stdout = 1;
            }
            # regex example: ---- or ---+---
            # need at least 3 dashes to avoid confusion with "--" comments
            if (($matches_copy_to_stdout && $ini !~ m/order by/i) ||
					$ini =~ m/^\s*(?:(?:\-\-)(?:\-)+(?:\+(?:\-)+)*)+\s*$/)
                # special case for copy select
            { # sort this region

                $directive->{firstline} = $outarr[-1];

                if (exists($directive->{order}) ||
                    exists($directive->{mvd}))
                {
                    $directive->{sortlines} = $outarr[-1];
                }

                # special case for copy select
                if ($matches_copy_to_stdout)
                {
                    $copy_to_stdout_result = 1;
                    $sql_statement = "";
                }
                # special case for explain
               if (exists($directive->{explain}) &&
                   ($ini =~ m/^\s*(?:(?:\-\-)(?:\-)+(?:\+(?:\-)+)*)+\s*$/) &&
                   (scalar(@outarr) && $outarr[-1] =~ m/QUERY PLAN/))
                {
                    # ENGINF-88: fixup explain headers
                    $outarr[-1] = "QUERY PLAN\n";
                    $ini = ("_" x length($outarr[-1])) . "\n";
                }

                $getstatement = 0;

                for my $line (@outarr)
                {
                    print $atmsort_outfh $apref, $line;
                }
                @outarr = ();

                print $atmsort_outfh $apref, $ini;

                if (defined($sql_statement)
                    && length($sql_statement)
                    # multiline match
                    && ($sql_statement =~ m/select.*order.*by/is))
                {
                    $has_order = 1; # so do *not* sort output
                    $directive->{sql_statement} = $sql_statement;
                }
                else
                {
                    $has_order = 0; # need to sort query output
                    $directive->{sql_statement} = $sql_statement;
                }
                $sql_statement = '';

                $getrows = 1;
                next;
            } # end sort this region
        } # end finding SQL

        # if MATCH then SUBSTITUTE
        # see HERE document for definitions
        $ini = match_then_subs($ini);

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
        print $atmsort_outfh $cpref, $line;
    }
} # end bigloop


# The arguments is the input filename. The output filename is returned as it
# is generated in this function to avoid races around the temporary filename
# creation.
sub run
{
    my $infname = shift;

    open my $infh,  '<', $infname  or die "could not open $infname: $!";
    my ($outfh, $outfname) = tempfile();

    run_fhs($infh, $outfh);

    close $infh;
    close $outfh;
    return $outfname;
}

# The arguments are input and output file handles
sub run_fhs
{
    my $infh = shift;
    my $outfh = shift;


    # loop over input file.
    atmsort_bigloop($infh, $outfh);
}

1;
