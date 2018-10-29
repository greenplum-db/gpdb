#----------------------------------------------------------------------
#
# Catalog.pm
#    Perl module that extracts info from catalog headers into Perl
#    data structures
#
# Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
# Portions Copyright (c) 1994, Regents of the University of California
#
# src/backend/catalog/Catalog.pm
#
#----------------------------------------------------------------------


# In Greenplum, we have added extra columns to some catalog tables. To avoid
# having to change every single DATA row in those header files, which would
# create massive merge conflicts when merging with upstream, the extra
# columns are marked with GPDB_COLUMN_DEFAULT() lines in the header files,
# which provides a default for every data row that's missing the column. That
# way, new rows can have all the columns, but there's no need to modify rows
# inherited from upstream.
#
# The ProcessDataLine function modifies each DATA row, injecting the defaults
# and extra values.

package Catalog;

use strict;
use warnings;

require Exporter;
our @ISA       = qw(Exporter);
our @EXPORT    = ();
our @EXPORT_OK = qw(Catalogs RenameTempFile);

# For error reporting.
my $current_line;

# Call this function with an array of names of header files to parse.
# Returns a nested data structure describing the data in the headers.
sub Catalogs
{
	my (%catalogs, $catname, $declaring_attributes, $most_recent);
	$catalogs{names} = [];

	# There are a few types which are given one name in the C source, but a
	# different name at the SQL level.  These are enumerated here.
	my %RENAME_ATTTYPE = (
		'int16'         => 'int2',
		'int32'         => 'int4',
		'Oid'           => 'oid',
		'NameData'      => 'name',
		'TransactionId' => 'xid');

	foreach my $input_file (@_)
	{
		my %coldefaults;
		my %extra_values;
		my %catalog;
		$catalog{columns} = [];
		$catalog{data} = [];

		open(INPUT_FILE, '<', $input_file) || die "$input_file: $!";

		# Scan the input file.
		while (<INPUT_FILE>)
		{
			$current_line = $_;

			# Strip C-style comments.
			s;/\*(.|\n)*\*/;;g;
			if (m;/\*;)
			{
				# handle multi-line comments properly.
				my $next_line = <INPUT_FILE>;
				die "$input_file: ends within C-style comment\n"
				  if !defined $next_line;
				$_ .= $next_line;
				redo;
			}

			# Strip useless whitespace and trailing semicolons.
			chomp;
			s/^\s+//;
			s/;\s*$//;
			s/\s+/ /g;

			# Push the data into the appropriate data structure.
			if (/^DATA\(insert(\s+OID\s+=\s+(\d+))?\s+\(\s*(.*)\s*\)\s*\)$/)
			{
				my $bki_values = $3;
				$bki_values = ProcessDataLine(\%catalog, $bki_values, \%coldefaults, \%extra_values);

				push @{ $catalog{data} }, { oid => $2, bki_values => $bki_values };
			}
			elsif (/^DESCR\(\"(.*)\"\)$/)
			{
				$most_recent = $catalog{data}->[-1];

				# this tests if most recent line is not a DATA() statement
				if (ref $most_recent ne 'HASH')
				{
					die "DESCR() does not apply to any catalog ($input_file)";
				}
				if (!defined $most_recent->{oid})
				{
					die "DESCR() does not apply to any oid ($input_file)";
				}
				elsif ($1 ne '')
				{
					$most_recent->{descr} = $1;
				}
			}
			elsif (/^SHDESCR\(\"(.*)\"\)$/)
			{
				$most_recent = $catalog{data}->[-1];

				# this tests if most recent line is not a DATA() statement
				if (ref $most_recent ne 'HASH')
				{
					die
					  "SHDESCR() does not apply to any catalog ($input_file)";
				}
				if (!defined $most_recent->{oid})
				{
					die "SHDESCR() does not apply to any oid ($input_file)";
				}
				elsif ($1 ne '')
				{
					$most_recent->{shdescr} = $1;
				}
			}
			elsif (/^DECLARE_TOAST\(\s*(\w+),\s*(\d+),\s*(\d+)\)/)
			{
				$catname = 'toasting';
				my ($toast_name, $toast_oid, $index_oid) = ($1, $2, $3);
				push @{ $catalog{data} },
				  "declare toast $toast_oid $index_oid on $toast_name\n";
			}
			elsif (/^DECLARE_(UNIQUE_)?INDEX\(\s*(\w+),\s*(\d+),\s*(.+)\)/)
			{
				$catname = 'indexing';
				my ($is_unique, $index_name, $index_oid, $using) =
				  ($1, $2, $3, $4);
				push @{ $catalog{data} },
				  sprintf(
					"declare %sindex %s %s %s\n",
					$is_unique ? 'unique ' : '',
					$index_name, $index_oid, $using);
			}
			elsif (/^BUILD_INDICES/)
			{
				push @{ $catalog{data} }, "build indices\n";
			}
			elsif (/^CATALOG\(([^,]*),(\d+)\)/)
			{
				$catname = $1;
				$catalog{relation_oid} = $2;

				# Store pg_* catalog names in the same order we receive them
				push @{ $catalogs{names} }, $catname;

				$catalog{bootstrap} = /BKI_BOOTSTRAP/ ? ' bootstrap' : '';
				$catalog{shared_relation} =
				  /BKI_SHARED_RELATION/ ? ' shared_relation' : '';
				$catalog{without_oids} =
				  /BKI_WITHOUT_OIDS/ ? ' without_oids' : '';
				$catalog{rowtype_oid} =
				  /BKI_ROWTYPE_OID\((\d+)\)/ ? " rowtype_oid $1" : '';
				$catalog{schema_macro} = /BKI_SCHEMA_MACRO/ ? 'True' : '';
				$declaring_attributes = 1;
			}
			# GPDB_COLUMN_DEFAULT(<colname>, <default>)
			elsif (m/GPDB_COLUMN_DEFAULT\s*\(\s*([[:word:]]+),\s*(.*)\)/)
			{
				my $colname = $1;
				my $coldefault = $2;
				my $found = 0;

				foreach my $column ( @{ $catalog{columns} } )
				{
					my ($attname, $atttype) = %$column;

					if ($attname eq $colname)
					{
						$found = 1;
					}
				}
				die "unknown column $colname on line: $current_line" if !$found;

				$coldefaults{$colname} = $coldefault;
			}
			# GPDB_EXTRA_COL(colname = value)
			elsif (m/^GPDB_EXTRA_COL\(\s*(\w+)\s*=\s*(.+)\s*\)/)
			{
				my $colname = $1;
				my $val = $2;

				$extra_values{$colname} = $val;
			}
			elsif ($declaring_attributes)
			{
				next if (/^{|^$/);
				next if (/^#/);
				if (/^}/)
				{
					undef $declaring_attributes;
				}
				else
				{
					my ($atttype, $attname) = split /\s+/, $_;
					die "parse error ($input_file)" unless $attname;
					if (exists $RENAME_ATTTYPE{$atttype})
					{
						$atttype = $RENAME_ATTTYPE{$atttype};
					}
					if ($attname =~ /(.*)\[.*\]/)    # array attribute
					{
						$attname = $1;
						$atttype .= '[]';            # variable-length only
					}
					push @{ $catalog{columns} }, { $attname => $atttype };
				}
			}
		}
		$catalogs{$catname} = \%catalog;
		close INPUT_FILE;
	}
	return \%catalogs;
}

# Rename temporary files to final names.
# Call this function with the final file name and the .tmp extension
# Note: recommended extension is ".tmp$$", so that parallel make steps
# can't use the same temp files
sub RenameTempFile
{
	my $final_name = shift;
	my $extension  = shift;
	my $temp_name  = $final_name . $extension;
	print "Writing $final_name\n";
	rename($temp_name, $final_name) || die "rename: $temp_name: $!";
}


# Injects default values for missing columns, per GPDB_COLUMN_DEFAULT()
# and GPDB_EXTRA_COL() directives.
use Text::ParseWords;
sub ProcessDataLine
{
	my $catalog = shift;
	my $middle = shift;
	my $coldefaults = shift;
	my $extra_values = shift;

	my %colnums;

	my %coldefaults_by_colnum;

	my $numdefaults = keys %$coldefaults;

	my $i = 1;
	foreach my $column ( @{ $catalog->{columns} } )
	{
		my ($attname, $atttype) = %$column;
		$colnums{$attname} = $i;

		$i = $i + 1;
	}
	my $numcols = $i - 1;

	foreach my $colname ( keys %$coldefaults )
	{
		my $def = $coldefaults->{$colname};

		my $colnum = $colnums{$colname};
		if (!defined $colnum)
		{
			die "GPDB_COLUMN_DEFAULT line found for non-existent column \"$colname\"";
		}

		$coldefaults_by_colnum{$colnum} = $def;
	}


	# Trim whitespace from beginning and end of $middle.
	$middle =~ s/^\s+|\s+$//g;

	# Ok, $middle now contains the columns, separated by spaces.
	# We can't just use split() on it, because we mustn't split
	# at strings inside quotes, e.g. "two words". Text::ParseWords
	# can do that for us.
	my @cols = Text::ParseWords::parse_line('\s+', 1, $middle);

	my $n = scalar @cols;

	if ($n == $numcols)
	{
		# The lines has all columns, it's OK as it is.
	}
	elsif ($n == $numcols - $numdefaults)
	{
		# The line is missing the optional columns. Insert defaults for them.

		# Splice the defaults into the array.
		foreach my $colnum (sort { $a <=> $b } keys %coldefaults_by_colnum) {
			splice @cols, ($colnum - 1), 0, $coldefaults_by_colnum{$colnum};
		}

		# Apply any extra per-line values.
		foreach my $colname (keys %$extra_values) {
			my $colnum = $colnums{$colname};
			my $extra_value = $extra_values->{$colname};

			if (!defined $colnum)
			{
				die "GPDB_EXTRA_COL line found for non-existent column \"$colname\"";
			}

			$cols[$colnum - 1] = $extra_value;
		}

		$middle = join(' ', @cols);
	}
	else
	{
		die("invalid number of columns ($n) at line:\n$current_line");
	}

	return $middle;
}

1;
