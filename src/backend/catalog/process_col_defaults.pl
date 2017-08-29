#! /usr/bin/perl -w
#-------------------------------------------------------------------------
#
# process_col_defaults.pl
#
# Usage: cat <input headers> | perl process_col_defaults.pl > postgres_bki_srcs
#
# Reads catalog header files, and injects default values for missing
# columns, per GPDB_COLUMN_DEFAULT() and GPDB_EXTRA_COL() directives.
#
#
# Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
# Portions Copyright (c) 1994, Regents of the University of California
#
#
# IDENTIFICATION
#    src/backend/catalog/process_col_defaults.pl
#
#-------------------------------------------------------------------------

use Text::ParseWords;

use strict;
use warnings;

my $catalogname;

my %colnums;
my %coldefaults;

my $numcols;
my $numdefaults;

my $processing_data = 0;

my %extra_values;

while (<>)
{
    my $line = $_;

    if (m/^CATALOG\(([[:word:]]+)/) {
	# This is the beginning of a new catalog. Reset our variables.
	$catalogname = $1;
	$processing_data = 0;
	%colnums = ();
	%coldefaults = ();
    }
    if (!($processing_data))
    {
	# Process headers. We're looking for #define Anum_* and
	# GPDB_COLUMN_DEFAULT() lines to define the structure of the table.
	if (m/GPDB_COLUMN_DEFAULT\s*\(\s*([[:word:]]+),\s*(.*)\)/) {
	    my $colname = $1;
	    my $coldefault = $2;

	    my $colnum = $colnums{ $colname };
	    $coldefaults{$colnum} = $coldefault;
	}
	if (m/#define Anum_([[:word:]]+)\s+(\d+)/) {
	    my $colname = $1;
	    my $colnum = $2;

	    $colnums{ $colname } = $colnum;
	}
	if (m/^DATA/) {
	    $processing_data = 1;

	    # We now have all the information on the table structure that we
	    # need.
	    $numcols = scalar keys %colnums;
	    $numdefaults = scalar keys %coldefaults;
	    print "/* process_col_defaults.pl parsed $catalogname with $numcols columns, of which $numdefaults are optional */\n";
	}
    }

    if ($processing_data)
    {
	# Process DATA rows, and possible GPDB_EXTRA_COL rows.

	# GPDB_EXTRA_COL(colname = value)
	if (m/^GPDB_EXTRA_COL\(\s*(\w+)\s*=\s*(.+)\s*\)/) {
	    my $colname = $1;
	    my $val = $2;

	    die "unknown column $colname on line: $line" if (!defined($colnums{$colname}));

	    $extra_values{$colname} = $val;
	}

	# For each DATA row, check the number of columns. Each DATA row
	# should have either $numcols columns, or $numcols - $numdefaults.
	# In other words, either all of the extra columns with defaults need
	# to be present, or none of them. Otherwise it gets too complicated
	# to figure out which columns have values and which should use the
	# defaults.
	#
	# Parse the DATA line enough to extract the middle part containing
	# the columns. This leaves the "insert OID = part" in $begin, and
	# the "));"  in $end.
	elsif (m/^(DATA\(.*?\()(.*)(\)\s*\).*)/) {
	    my $begin = $1;
	    my $middle = $2;
	    my $end = $3;

	    # Trim whitespace from beginning and end of $middle.
	    $middle =~ s/^\s+|\s+$//g;

	    # Ok, $middle now contains the columns, separated by spaces.
	    # We can't just use split() on it, because we mustn't split
	    # at strings inside quotes, e.g. "two words". Text::ParseWords
	    # can do that for us.
	    my @cols = Text::ParseWords::parse_line('\s+', 1, $middle);

	    my $n = scalar @cols;

	    if ($n == $numcols) {
		# The lines has all columns, it's OK as it is.
	    }
	    elsif ($n == $numcols - $numdefaults) {
		# The line is missing the optional columns. Insert defaults for
		# them. But first print the original line in a comment, for
		# debugging.
		print "/* original: $middle */\n";

		# Splice the defaults into the array.
		foreach my $colnum (sort { $a <=> $b } keys %coldefaults) {
		    splice @cols, ($colnum - 1), 0, $coldefaults{$colnum};
		}

		# Apply any extra per-line values.
		foreach my $colname (keys %extra_values) {
		    my $colnum = $colnums{$colname};
		    my $extra_value = $extra_values{$colname};

		    $cols[$colnum - 1] = $extra_value;
		}
		%extra_values = ();

		$middle = join(' ', @cols);

		# Reconstruct the whole line.
		$line = $begin . $middle . $end . "\n";
	    }
	    else {
		die("invalid number of columns ($n) at line:\n$line");
	    }
	}
    }
    print $line;
}
