#!/usr/bin/perl
#----------------------------------------------------------------------
#
# genbki.pl
#    Perl script that generates postgres.bki, postgres.description,
#    postgres.shdescription, and symbol definition headers from specially
#    formatted header files and data files.  The BKI files are used to
#    initialize the postgres template database.
#
# Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
# Portions Copyright (c) 1994, Regents of the University of California
#
# src/backend/catalog/genbki.pl
#
#----------------------------------------------------------------------

use strict;
use warnings;
use Getopt::Long;

use File::Basename;
use File::Spec;
BEGIN { use lib File::Spec->rel2abs(dirname(__FILE__)); }

use Catalog;

my $output_path = '';
my $major_version;
my $include_path;

GetOptions(
	'output:s'       => \$output_path,
	'set-version:s'  => \$major_version,
	'include-path:s' => \$include_path) || usage();

# Sanity check arguments.
die "No input files.\n"                  unless @ARGV;
die "--set-version must be specified.\n" unless $major_version;
die "Invalid version string: $major_version\n"
  unless $major_version =~ /^\d+$/;
die "--include-path must be specified.\n" unless $include_path;

# Make sure paths end with a slash.
if ($output_path ne '' && substr($output_path, -1) ne '/')
{
	$output_path .= '/';
}
if (substr($include_path, -1) ne '/')
{
	$include_path .= '/';
}

# Read all the files into internal data structures.
my @catnames;
my %catalogs;
my %catalog_data;
my @toast_decls;
my @index_decls;
my %oidcounts;

foreach my $header (@ARGV)
{
	$header =~ /(.+)\.h$/
	  or die "Input files need to be header files.\n";
	my $datfile = "$1.dat";

	my $catalog = Catalog::ParseHeader($header);
	my $catname = $catalog->{catname};
	my $schema  = $catalog->{columns};

	if (defined $catname)
	{
		push @catnames, $catname;
		$catalogs{$catname} = $catalog;
	}

	# While checking for duplicated OIDs, we ignore the pg_class OID and
	# rowtype OID of bootstrap catalogs, as those are expected to appear
	# in the initial data for pg_class and pg_type.  For regular catalogs,
	# include these OIDs.  (See also Catalog::FindAllOidsFromHeaders
	# if you change this logic.)
	if (!$catalog->{bootstrap})
	{
		$oidcounts{ $catalog->{relation_oid} }++
		  if ($catalog->{relation_oid});
		$oidcounts{ $catalog->{rowtype_oid} }++
		  if ($catalog->{rowtype_oid});
	}

	# Not all catalogs have a data file.
	if (-e $datfile)
	{
		my $data = Catalog::ParseData($datfile, $schema, 0);
		$catalog_data{$catname} = $data;

		# Check for duplicated OIDs while we're at it.
		foreach my $row (@$data)
		{
			$oidcounts{ $row->{oid} }++ if defined $row->{oid};
		}
	}

	# If the header file contained toast or index info, build BKI
	# commands for those, which we'll output later.
	foreach my $toast (@{ $catalog->{toasting} })
	{
		push @toast_decls,
		  sprintf "declare toast %s %s on %s\n",
		  $toast->{toast_oid}, $toast->{toast_index_oid},
		  $toast->{parent_table};
		$oidcounts{ $toast->{toast_oid} }++;
		$oidcounts{ $toast->{toast_index_oid} }++;
	}
	foreach my $index (@{ $catalog->{indexing} })
	{
		push @index_decls,
		  sprintf "declare %sindex %s %s %s\n",
		  $index->{is_unique} ? 'unique ' : '',
		  $index->{index_name}, $index->{index_oid},
		  $index->{index_decl};
		$oidcounts{ $index->{index_oid} }++;
	}
}

# Complain and exit if we found any duplicate OIDs.
# While duplicate OIDs would only cause a failure if they appear in
# the same catalog, our project policy is that manually assigned OIDs
# should be globally unique, to avoid confusion.
my $found = 0;
foreach my $oid (keys %oidcounts)
{
	next unless $oidcounts{$oid} > 1;
	print STDERR "Duplicate OIDs detected:\n" if !$found;
	print STDERR "$oid\n";
	$found++;
}
die "found $found duplicate OID(s) in catalog data\n" if $found;


# Oids not specified in the input files are automatically assigned,
# starting at FirstGenbkiObjectId, extending up to FirstBootstrapObjectId.
my $FirstGenbkiObjectId =
  Catalog::FindDefinedSymbol('catalog/pg_magic_oid.h', $include_path,
	'FirstGenbkiObjectId');
my $FirstBootstrapObjectId =
  Catalog::FindDefinedSymbol('catalog/pg_magic_oid.h', $include_path,
	'FirstBootstrapObjectId');
my $GenbkiNextOid = $FirstGenbkiObjectId;


# Fetch some special data that we will substitute into the output file.
# CAUTION: be wary about what symbols you substitute into the .bki file here!
# It's okay to substitute things that are expected to be really constant
# within a given Postgres release, such as fixed OIDs.  Do not substitute
# anything that could depend on platform or configuration.  (The right place
# to handle those sorts of things is in initdb.c's bootstrap_template1().)
my $BOOTSTRAP_SUPERUSERID =
  Catalog::FindDefinedSymbolFromData($catalog_data{pg_authid},
	'BOOTSTRAP_SUPERUSERID');
my $C_COLLATION_OID =
  Catalog::FindDefinedSymbolFromData($catalog_data{pg_collation},
	'C_COLLATION_OID');
my $PG_CATALOG_NAMESPACE =
  Catalog::FindDefinedSymbolFromData($catalog_data{pg_namespace},
	'PG_CATALOG_NAMESPACE');


# Build lookup tables.

# access method OID lookup
my %amoids;
foreach my $row (@{ $catalog_data{pg_am} })
{
	$amoids{ $row->{amname} } = $row->{oid};
}

# class (relation) OID lookup (note this only covers bootstrap catalogs!)
my %classoids;
foreach my $row (@{ $catalog_data{pg_class} })
{
	$classoids{ $row->{relname} } = $row->{oid};
}

# collation OID lookup
my %collationoids;
foreach my $row (@{ $catalog_data{pg_collation} })
{
	$collationoids{ $row->{collname} } = $row->{oid};
}

# language OID lookup
my %langoids;
foreach my $row (@{ $catalog_data{pg_language} })
{
	$langoids{ $row->{lanname} } = $row->{oid};
}

# opclass OID lookup
my %opcoids;
foreach my $row (@{ $catalog_data{pg_opclass} })
{
	# There is no unique name, so we need to combine access method
	# and opclass name.
	my $key = sprintf "%s/%s", $row->{opcmethod}, $row->{opcname};
	$opcoids{$key} = $row->{oid};
}

# operator OID lookup
my %operoids;
foreach my $row (@{ $catalog_data{pg_operator} })
{
	# There is no unique name, so we need to invent one that contains
	# the relevant type names.
	my $key = sprintf "%s(%s,%s)",
	  $row->{oprname}, $row->{oprleft}, $row->{oprright};
	$operoids{$key} = $row->{oid};
}

# opfamily OID lookup
my %opfoids;
foreach my $row (@{ $catalog_data{pg_opfamily} })
{
	# There is no unique name, so we need to combine access method
	# and opfamily name.
	my $key = sprintf "%s/%s", $row->{opfmethod}, $row->{opfname};

	# GPDB: Normally, we assign OIDs only later, and we don't put
	# auto-assigned OIDs in the lookup tables, but auto-generated bitmap
	# opfamilies, with no hard-coded OIDs are referenced by name from the
	# other (auto-generated) entries in pg_class, pg_amop and pg_amproc.
	if (!($row->{oid}))
	{
		$row->{oid} = $GenbkiNextOid;
		$GenbkiNextOid++;
	}

	$opfoids{$key} = $row->{oid};
}

# procedure OID lookup
my %procoids;
foreach my $row (@{ $catalog_data{pg_proc} })
{
	# Generate an entry under just the proname (corresponds to regproc lookup)
	my $prokey = $row->{proname};
	if (defined $procoids{$prokey})
	{
		$procoids{$prokey} = 'MULTIPLE';
	}
	else
	{
		$procoids{$prokey} = $row->{oid};
	}

	# Also generate an entry using proname(proargtypes).  This is not quite
	# identical to regprocedure lookup because we don't worry much about
	# special SQL names for types etc; we just use the names in the source
	# proargtypes field.  These *should* be unique, but do a multiplicity
	# check anyway.
	$prokey .= '(' . join(',', split(/\s+/, $row->{proargtypes})) . ')';
	if (defined $procoids{$prokey})
	{
		$procoids{$prokey} = 'MULTIPLE';
	}
	else
	{
		$procoids{$prokey} = $row->{oid};
	}
}

# tablespace OID lookup
my %tablespaceoids;
foreach my $row (@{ $catalog_data{pg_tablespace} })
{
	$tablespaceoids{ $row->{spcname} } = $row->{oid};
}

# text search configuration OID lookup
my %tsconfigoids;
foreach my $row (@{ $catalog_data{pg_ts_config} })
{
	$tsconfigoids{ $row->{cfgname} } = $row->{oid};
}

# text search dictionary OID lookup
my %tsdictoids;
foreach my $row (@{ $catalog_data{pg_ts_dict} })
{
	$tsdictoids{ $row->{dictname} } = $row->{oid};
}

# text search parser OID lookup
my %tsparseroids;
foreach my $row (@{ $catalog_data{pg_ts_parser} })
{
	$tsparseroids{ $row->{prsname} } = $row->{oid};
}

# text search template OID lookup
my %tstemplateoids;
foreach my $row (@{ $catalog_data{pg_ts_template} })
{
	$tstemplateoids{ $row->{tmplname} } = $row->{oid};
}

# type lookups
my %typeoids;
my %types;
foreach my $row (@{ $catalog_data{pg_type} })
{
	# for OID macro substitutions
	$typeoids{ $row->{typname} } = $row->{oid};

	# for pg_attribute copies of pg_type values
	$types{ $row->{typname} } = $row;
}

# Encoding identifier lookup.  This uses the same replacement machinery
# as for OIDs, but we have to dig the values out of pg_wchar.h.
my %encids;

my $encfile = $include_path . 'mb/pg_wchar.h';
open(my $ef, '<', $encfile) || die "$encfile: $!";

# We're parsing an enum, so start with 0 and increment
# every time we find an enum member.
my $encid             = 0;
my $collect_encodings = 0;
while (<$ef>)
{
	if (/typedef\s+enum\s+pg_enc/)
	{
		$collect_encodings = 1;
		next;
	}

	last if /_PG_LAST_ENCODING_/;

	if ($collect_encodings and /^\s+(PG_\w+)/)
	{
		$encids{$1} = $encid;
		$encid++;
	}
}

close $ef;

# Map lookup name to the corresponding hash table.
my %lookup_kind = (
	pg_am          => \%amoids,
	pg_class       => \%classoids,
	pg_collation   => \%collationoids,
	pg_language    => \%langoids,
	pg_opclass     => \%opcoids,
	pg_operator    => \%operoids,
	pg_opfamily    => \%opfoids,
	pg_proc        => \%procoids,
	pg_tablespace  => \%tablespaceoids,
	pg_ts_config   => \%tsconfigoids,
	pg_ts_dict     => \%tsdictoids,
	pg_ts_parser   => \%tsparseroids,
	pg_ts_template => \%tstemplateoids,
	pg_type        => \%typeoids,
	encoding       => \%encids);


# Open temp files
my $tmpext  = ".tmp$$";
my $bkifile = $output_path . 'postgres.bki';
open my $bki, '>', $bkifile . $tmpext
  or die "can't open $bkifile$tmpext: $!";
my $schemafile = $output_path . 'schemapg.h';
open my $schemapg, '>', $schemafile . $tmpext
  or die "can't open $schemafile$tmpext: $!";
my $descrfile = $output_path . 'postgres.description';
open my $descr, '>', $descrfile . $tmpext
  or die "can't open $descrfile$tmpext: $!";
my $shdescrfile = $output_path . 'postgres.shdescription';
open my $shdescr, '>', $shdescrfile . $tmpext
  or die "can't open $shdescrfile$tmpext: $!";

# Generate postgres.bki, postgres.description, postgres.shdescription,
# and pg_*_d.h headers.

# version marker for .bki file
print $bki "# PostgreSQL $major_version\n";

# vars to hold data needed for schemapg.h
my %schemapg_entries;
my @tables_needing_macros;

# produce output, one catalog at a time
foreach my $catname (@catnames)
{
	my $catalog = $catalogs{$catname};

	# Create one definition header with macro definitions for each catalog.
	my $def_file = $output_path . $catname . '_d.h';
	open my $def, '>', $def_file . $tmpext
	  or die "can't open $def_file$tmpext: $!";

	# Opening boilerplate for pg_*_d.h
	printf $def <<EOM, $catname, $catname, uc $catname, uc $catname;
/*-------------------------------------------------------------------------
 *
 * %s_d.h
 *    Macro definitions for %s
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * NOTES
 *  ******************************
 *  *** DO NOT EDIT THIS FILE! ***
 *  ******************************
 *
 *  It has been GENERATED by src/backend/catalog/genbki.pl
 *
 *-------------------------------------------------------------------------
 */
#ifndef %s_D_H
#define %s_D_H

EOM

	# Emit OID macros for catalog's OID and rowtype OID, if wanted
	printf $def "#define %s %s\n",
	  $catalog->{relation_oid_macro}, $catalog->{relation_oid}
	  if $catalog->{relation_oid_macro};
	printf $def "#define %s %s\n",
	  $catalog->{rowtype_oid_macro}, $catalog->{rowtype_oid}
	  if $catalog->{rowtype_oid_macro};
	print $def "\n";

	# .bki CREATE command for this catalog
	print $bki "create $catname $catalog->{relation_oid}"
	  . $catalog->{shared_relation}
	  . $catalog->{bootstrap}
	  . $catalog->{rowtype_oid_clause};

	my $first = 1;

	print $bki "\n (\n";
	my $schema = $catalog->{columns};
	my %attnames;
	my $attnum = 0;
	foreach my $column (@$schema)
	{
		$attnum++;
		my $attname = $column->{name};
		my $atttype = $column->{type};

		# Build hash of column names for use later
		$attnames{$attname} = 1;

		# Emit column definitions
		if (!$first)
		{
			print $bki " ,\n";
		}
		$first = 0;

		print $bki " $attname = $atttype";

		if (defined $column->{forcenotnull})
		{
			print $bki " FORCE NOT NULL";
		}
		elsif (defined $column->{forcenull})
		{
			print $bki " FORCE NULL";
		}

		# Emit Anum_* constants
		printf $def "#define Anum_%s_%s %s\n", $catname, $attname, $attnum;
	}
	print $bki "\n )\n";

	# Emit Natts_* constant
	print $def "\n#define Natts_$catname $attnum\n\n";

	# Emit client code copied from source header
	foreach my $line (@{ $catalog->{client_code} })
	{
		print $def $line;
	}

	# Open it, unless it's a bootstrap catalog (create bootstrap does this
	# automatically)
	if (!$catalog->{bootstrap})
	{
		print $bki "open $catname\n";
	}

	# For pg_attribute.h, we generate data entries ourselves.
	if ($catname eq 'pg_attribute')
	{
		gen_pg_attribute($schema);
	}

	# Ordinary catalog with a data file
	foreach my $row (@{ $catalog_data{$catname} })
	{
		my %bki_values = %$row;

		# Complain about unrecognized keys; they are presumably misspelled
		foreach my $key (keys %bki_values)
		{
			next
			  if $key eq "oid_symbol"
			  || $key eq "array_type_oid"
			  || $key eq "descr"
			  || $key eq "autogenerated"
			  || $key eq "line_number";
			die sprintf "unrecognized field name \"%s\" in %s.dat line %s\n",
			  $key, $catname, $bki_values{line_number}
			  if (!exists($attnames{$key}));
		}

		# Perform required substitutions on fields
		foreach my $column (@$schema)
		{
			my $attname = $column->{name};
			my $atttype = $column->{type};

			# Assign oid if oid column exists and no explicit assignment in row
			if ($attname eq "oid" and not defined $bki_values{$attname})
			{
				$bki_values{$attname} = $GenbkiNextOid;
				$GenbkiNextOid++;
			}

			# Substitute constant values we acquired above.
			# (It's intentional that this can apply to parts of a field).
			$bki_values{$attname} =~ s/\bPGUID\b/$BOOTSTRAP_SUPERUSERID/g;
			$bki_values{$attname} =~ s/\bPGNSP\b/$PG_CATALOG_NAMESPACE/g;

			# Replace OID synonyms with OIDs per the appropriate lookup rule.
			#
			# If the column type is oidvector or _oid, we have to replace
			# each element of the array as per the lookup rule.
			if ($column->{lookup})
			{
				my $lookup = $lookup_kind{ $column->{lookup} };
				my @lookupnames;
				my @lookupoids;

				die "unrecognized BKI_LOOKUP type " . $column->{lookup}
				  if !defined($lookup);

				if ($atttype eq 'oidvector')
				{
					@lookupnames = split /\s+/, $bki_values{$attname};
					@lookupoids = lookup_oids($lookup, $catname, \%bki_values,
						@lookupnames);
					$bki_values{$attname} = join(' ', @lookupoids);
				}
				elsif ($atttype eq '_oid')
				{
					if ($bki_values{$attname} ne '_null_')
					{
						$bki_values{$attname} =~ s/[{}]//g;
						@lookupnames = split /,/, $bki_values{$attname};
						@lookupoids =
						  lookup_oids($lookup, $catname, \%bki_values,
							@lookupnames);
						$bki_values{$attname} = sprintf "{%s}",
						  join(',', @lookupoids);
					}
				}
				else
				{
					$lookupnames[0] = $bki_values{$attname};
					@lookupoids = lookup_oids($lookup, $catname, \%bki_values,
						@lookupnames);
					$bki_values{$attname} = $lookupoids[0];
				}
			}
		}

		# Special hack to generate OID symbols for pg_type entries
		# that lack one.
		if ($catname eq 'pg_type' and !exists $bki_values{oid_symbol})
		{
			my $symbol = form_pg_type_symbol($bki_values{typname});
			$bki_values{oid_symbol} = $symbol
			  if defined $symbol;
		}

		# Write to postgres.bki
		print_bki_insert(\%bki_values, $schema);

		# Write comments to postgres.description and
		# postgres.shdescription
		if (defined $bki_values{descr})
		{
			if ($catalog->{shared_relation})
			{
				printf $shdescr "%s\t%s\t%s\n",
				  $bki_values{oid}, $catname, $bki_values{descr};
			}
			else
			{
				printf $descr "%s\t%s\t0\t%s\n",
				  $bki_values{oid}, $catname, $bki_values{descr};
			}
		}

		# Emit OID symbol
		if (defined $bki_values{oid_symbol})
		{
			printf $def "#define %s %s\n",
			  $bki_values{oid_symbol}, $bki_values{oid};
		}
	}

	print $bki "close $catname\n";
	printf $def "\n#endif\t\t\t\t\t\t\t/* %s_D_H */\n", uc $catname;

	# Close and rename definition header
	close $def;
	Catalog::RenameTempFile($def_file, $tmpext);
}

# Any information needed for the BKI that is not contained in a pg_*.h header
# (i.e., not contained in a header with a CATALOG() statement) comes here

# Write out declare toast/index statements
foreach my $declaration (@toast_decls)
{
	print $bki $declaration;
}

foreach my $declaration (@index_decls)
{
	print $bki $declaration;
}

# last command in the BKI file: build the indexes declared above
print $bki "build indices\n";

# check that we didn't overrun available OIDs
die
  "genbki OID counter reached $GenbkiNextOid, overrunning FirstBootstrapObjectId\n"
  if $GenbkiNextOid > $FirstBootstrapObjectId;


# Now generate schemapg.h

# Opening boilerplate for schemapg.h
print $schemapg <<EOM;
/*-------------------------------------------------------------------------
 *
 * schemapg.h
 *    Schema_pg_xxx macros for use by relcache.c
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * NOTES
 *  ******************************
 *  *** DO NOT EDIT THIS FILE! ***
 *  ******************************
 *
 *  It has been GENERATED by src/backend/catalog/genbki.pl
 *
 *-------------------------------------------------------------------------
 */
#ifndef SCHEMAPG_H
#define SCHEMAPG_H
EOM

# Emit schemapg declarations
foreach my $table_name (@tables_needing_macros)
{
	print $schemapg "\n#define Schema_$table_name \\\n";
	print $schemapg join ", \\\n", @{ $schemapg_entries{$table_name} };
	print $schemapg "\n";
}

# Closing boilerplate for schemapg.h
print $schemapg "\n#endif\t\t\t\t\t\t\t/* SCHEMAPG_H */\n";

# We're done emitting data
close $bki;
close $schemapg;
close $descr;
close $shdescr;

# Finally, rename the completed files into place.
Catalog::RenameTempFile($bkifile,     $tmpext);
Catalog::RenameTempFile($schemafile,  $tmpext);
Catalog::RenameTempFile($descrfile,   $tmpext);
Catalog::RenameTempFile($shdescrfile, $tmpext);

exit 0;

#################### Subroutines ########################


# For each catalog marked as needing a schema macro, generate the
# per-user-attribute data to be incorporated into schemapg.h.  Also, for
# bootstrap catalogs, emit pg_attribute entries into the .bki file
# for both user and system attributes.
sub gen_pg_attribute
{
	my $schema = shift;

	my @attnames;
	foreach my $column (@$schema)
	{
		push @attnames, $column->{name};
	}

	foreach my $table_name (@catnames)
	{
		my $table = $catalogs{$table_name};

		# Currently, all bootstrap catalogs also need schemapg.h
		# entries, so skip if it isn't to be in schemapg.h.
		next if !$table->{schema_macro};

		$schemapg_entries{$table_name} = [];
		push @tables_needing_macros, $table_name;

		# Generate entries for user attributes.
		my $attnum       = 0;
		my $priornotnull = 1;
		foreach my $attr (@{ $table->{columns} })
		{
			$attnum++;
			my %row;
			$row{attnum}   = $attnum;
			$row{attrelid} = $table->{relation_oid};

			morph_row_for_pgattr(\%row, $schema, $attr, $priornotnull);
			$priornotnull &= ($row{attnotnull} eq 't');

			# If it's bootstrapped, put an entry in postgres.bki.
			print_bki_insert(\%row, $schema) if $table->{bootstrap};

			# Store schemapg entries for later.
			morph_row_for_schemapg(\%row, $schema);
			push @{ $schemapg_entries{$table_name} },
			  sprintf "{ %s }",
			  join(', ', grep { defined $_ } @row{@attnames});
		}

		# Generate entries for system attributes.
		# We only need postgres.bki entries, not schemapg.h entries.
		if ($table->{bootstrap})
		{
			$attnum = 0;
			my @SYS_ATTRS = (
				{ name => 'ctid',     type => 'tid' },
				{ name => 'xmin',     type => 'xid' },
				{ name => 'cmin',     type => 'cid' },
				{ name => 'xmax',     type => 'xid' },
				{ name => 'cmax',     type => 'cid' },
				{ name => 'tableoid', type => 'oid' },
				{ name => 'gp_segment_id', type => 'int4' });
			foreach my $attr (@SYS_ATTRS)
			{
				$attnum--;
				my %row;
				$row{attnum}        = $attnum;
				$row{attrelid}      = $table->{relation_oid};
				$row{attstattarget} = '0';

				morph_row_for_pgattr(\%row, $schema, $attr, 1);
				print_bki_insert(\%row, $schema);
			}
		}
	}
	return;
}

# Given $pgattr_schema (the pg_attribute schema for a catalog sufficient for
# AddDefaultValues), $attr (the description of a catalog row), and
# $priornotnull (whether all prior attributes in this catalog are not null),
# modify the $row hashref for print_bki_insert.  This includes setting data
# from the corresponding pg_type element and filling in any default values.
# Any value not handled here must be supplied by caller.
sub morph_row_for_pgattr
{
	my ($row, $pgattr_schema, $attr, $priornotnull) = @_;
	my $attname = $attr->{name};
	my $atttype = $attr->{type};

	$row->{attname} = $attname;

	# Copy the type data from pg_type, and add some type-dependent items
	my $type = $types{$atttype};

	$row->{atttypid}   = $type->{oid};
	$row->{attlen}     = $type->{typlen};
	$row->{attbyval}   = $type->{typbyval};
	$row->{attstorage} = $type->{typstorage};
	$row->{attalign}   = $type->{typalign};

	# set attndims if it's an array type
	$row->{attndims} = $type->{typcategory} eq 'A' ? '1' : '0';

	# collation-aware catalog columns must use C collation
	$row->{attcollation} =
	  $type->{typcollation} ne '0' ? $C_COLLATION_OID : 0;

	if (defined $attr->{forcenotnull})
	{
		$row->{attnotnull} = 't';
	}
	elsif (defined $attr->{forcenull})
	{
		$row->{attnotnull} = 'f';
	}
	elsif ($priornotnull)
	{

		# attnotnull will automatically be set if the type is
		# fixed-width and prior columns are all NOT NULL ---
		# compare DefineAttr in bootstrap.c. oidvector and
		# int2vector are also treated as not-nullable.
		$row->{attnotnull} =
		    $type->{typname} eq 'oidvector'  ? 't'
		  : $type->{typname} eq 'int2vector' ? 't'
		  : $type->{typlen} eq 'NAMEDATALEN' ? 't'
		  : $type->{typlen} > 0              ? 't'
		  :                                    'f';
	}
	else
	{
		$row->{attnotnull} = 'f';
	}

	Catalog::AddDefaultValues($row, $pgattr_schema, 'pg_attribute');
	return;
}

# Write an entry to postgres.bki.
sub print_bki_insert
{
	my $row    = shift;
	my $schema = shift;

	my @bki_values;

	foreach my $column (@$schema)
	{
		my $attname   = $column->{name};
		my $atttype   = $column->{type};
		my $bki_value = $row->{$attname};

		# Fold backslash-zero to empty string if it's the entire string,
		# since that represents a NUL char in C code.
		$bki_value = '' if $bki_value eq '\0';

		# Handle single quotes by doubling them, and double quotes by
		# converting them to octal escapes, because that's what the
		# bootstrap scanner requires.  We do not process backslashes
		# specially; this allows escape-string-style backslash escapes
		# to be used in catalog data.
		$bki_value =~ s/'/''/g;
		$bki_value =~ s/"/\\042/g;

		# Quote value if needed.  We need not quote values that satisfy
		# the "id" pattern in bootscanner.l, currently "[-A-Za-z0-9_]+".
		$bki_value = sprintf(qq'"%s"', $bki_value)
		  if length($bki_value) == 0
		  or $bki_value =~ /[^-A-Za-z0-9_]/;

		push @bki_values, $bki_value;
	}
	printf $bki "insert ( %s )\n", join(' ', @bki_values);
	return;
}

# Given a row reference, modify it so that it becomes a valid entry for
# a catalog schema declaration in schemapg.h.
#
# The field values of a Schema_pg_xxx declaration are similar, but not
# quite identical, to the corresponding values in postgres.bki.
sub morph_row_for_schemapg
{
	my $row           = shift;
	my $pgattr_schema = shift;

	foreach my $column (@$pgattr_schema)
	{
		my $attname = $column->{name};
		my $atttype = $column->{type};

		# Some data types have special formatting rules.
		if ($atttype eq 'name')
		{
			# add {" ... "} quoting
			$row->{$attname} = sprintf(qq'{"%s"}', $row->{$attname});
		}
		elsif ($atttype eq 'char')
		{
			# Add single quotes
			$row->{$attname} = sprintf("'%s'", $row->{$attname});
		}

		# Expand booleans from 'f'/'t' to 'false'/'true'.
		# Some values might be other macros (eg FLOAT4PASSBYVAL),
		# don't change.
		elsif ($atttype eq 'bool')
		{
			$row->{$attname} = 'true'  if $row->{$attname} eq 't';
			$row->{$attname} = 'false' if $row->{$attname} eq 'f';
		}

		# We don't emit initializers for the variable length fields at all.
		# Only the fixed-size portions of the descriptors are ever used.
		delete $row->{$attname} if $column->{is_varlen};
	}
	return;
}

# Perform OID lookups on an array of OID names.
# If we don't have a unique value to substitute, warn and
# leave the entry unchanged.
# (A warning seems sufficient because the bootstrap backend will reject
# non-numeric values anyway.  So we might as well detect multiple problems
# within this genbki.pl run.)
sub lookup_oids
{
	my ($lookup, $catname, $bki_values, @lookupnames) = @_;

	my @lookupoids;
	foreach my $lookupname (@lookupnames)
	{
		my $lookupoid = $lookup->{$lookupname};
		if (defined($lookupoid) and $lookupoid ne 'MULTIPLE')
		{
			push @lookupoids, $lookupoid;
		}
		else
		{
			push @lookupoids, $lookupname;
			warn sprintf
			  "unresolved OID reference \"%s\" in %s.dat line %s\n",
			  $lookupname, $catname, $bki_values->{line_number}
			  if $lookupname ne '-' and $lookupname ne '0';
		}
	}
	return @lookupoids;
}

# Determine canonical pg_type OID #define symbol from the type name.
sub form_pg_type_symbol
{
	my $typename = shift;

	# Skip for rowtypes of bootstrap catalogs, since they have their
	# own naming convention defined elsewhere.
	return
	     if $typename eq 'pg_type'
	  or $typename eq 'pg_proc'
	  or $typename eq 'pg_attribute'
	  or $typename eq 'pg_class';

	# Transform like so:
	#  foo_bar  ->  FOO_BAROID
	# _foo_bar  ->  FOO_BARARRAYOID
	$typename =~ /(_)?(.+)/;
	my $arraystr = $1 ? 'ARRAY' : '';
	my $name = uc $2;
	return $name . $arraystr . 'OID';
}

sub usage
{
	die <<EOM;
Usage: perl -I [directory of Catalog.pm] genbki.pl [--output/-o <path>] [--include-path/-i <path>] header...

Options:
    --output         Output directory (default '.')
    --set-version    PostgreSQL version number for initdb cross-check
    --include-path   Include path in source tree

genbki.pl generates BKI files and symbol definition
headers from specially formatted header files and .dat
files.  The BKI files are used to initialize the
postgres template database.

Report bugs to <pgsql-bugs\@lists.postgresql.org>.
EOM
}
