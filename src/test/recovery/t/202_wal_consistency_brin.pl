# Copyright (c) 2021-2022, PostgreSQL Global Development Group

# Verify WAL consistency of BRIN indexes for GPDB. This is a replica of
# src/test/modules/brin/t/02_wal_consistency.pl, with added tests for AO/CO tables.
# It's added here, since we currently don't run src/test/modules in CI.

use strict;
use warnings;

use PostgreSQL::Test::Utils;
use Test::More;
use PostgreSQL::Test::Cluster;

# Set up primary
my $whiskey = PostgreSQL::Test::Cluster->new('whiskey');
$whiskey->init(allows_streaming => 1);
$whiskey->append_conf('postgresql.conf', 'wal_consistency_checking = brin');
$whiskey->start;
$whiskey->safe_psql('postgres', 'create extension pageinspect');
is( $whiskey->psql(
		'postgres',
		qq[SELECT pg_create_physical_replication_slot('standby_1');]),
	0,
	'physical slot created on primary');

# Take backup
my $backup_name = 'brinbkp';
$whiskey->backup($backup_name);

# Create streaming standby linking to primary
my $charlie = PostgreSQL::Test::Cluster->new('charlie');
$charlie->init_from_backup($whiskey, $backup_name, has_streaming => 1);
$charlie->append_conf('postgresql.conf', 'primary_slot_name = standby_1');
$charlie->start;

# Now write some WAL in the primary for a heap table

$whiskey->safe_psql(
	'postgres', qq{
create table tbl_timestamp0 (d1 timestamp(0) without time zone) with (fillfactor=10);
create index on tbl_timestamp0 using brin (d1) with (pages_per_range = 1, autosummarize=false);
});
# Run a loop that will end when the second revmap page is created
$whiskey->safe_psql(
	'postgres', q{
do
$$
declare
  current timestamp with time zone := '2019-03-27 08:14:01.123456789 UTC';
begin
  loop
    insert into tbl_timestamp0 select i from
      generate_series(current, current + interval '1 day', '28 seconds') i;
    perform brin_summarize_new_values('tbl_timestamp0_d1_idx');
    if (brin_metapage_info(get_raw_page('tbl_timestamp0_d1_idx', 0))).lastrevmappage > 1 then
      exit;
    end if;
    current := current + interval '1 day';
  end loop;
end
$$;
});

# Now write some WAL in the primary for an ao_row and an ao_column table.

# ao_row:
$whiskey->safe_psql(
   'postgres', qq{
-- Case 1 (Starting a revmap chain .. 1 revmap page)
CREATE TABLE tbl_ao_row1 (i int) USING ao_row;
INSERT INTO tbl_ao_row1 SELECT generate_series(1, 5);
CREATE INDEX ON tbl_ao_row1 using brin (i) with (pages_per_range = 1, autosummarize=false);

-- Case 2 (Extending a revmap chain .. 2 revmap pages)
CREATE TABLE tbl_ao_row2 (i int) USING ao_row;
insert into tbl_ao_row2 select generate_series(1, 5);
-- Bloat gp_fastsequence so that we will have to create 2 revmap pages.
-- REVMAP_PAGE_MAXITEMS = 5456. About 32768 ints fit in one logical heap block.
-- So we need at least 32768 * 5456 + 1 = 178782209 rows to have 2 revmap pages.
SET allow_system_table_mods TO ON;
UPDATE gp_fastsequence SET last_sequence = 180000000 WHERE
  objid = (SELECT segrelid FROM pg_appendonly WHERE relid='tbl_ao_row2'::regclass);
INSERT INTO tbl_ao_row2 SELECT generate_series(6, 10);
CREATE INDEX ON tbl_ao_row2 USING brin (i) WITH (pages_per_range = 1, autosummarize=false);
});

# ao_column:
$whiskey->safe_psql(
   'postgres', qq{
-- Case 1 (Starting a revmap chain .. 1 revmap page)
CREATE TABLE tbl_ao_column1 (i int) USING ao_column;
INSERT INTO tbl_ao_column1 SELECT generate_series(1, 5);
CREATE INDEX ON tbl_ao_column1 using brin (i) with (pages_per_range = 1, autosummarize=false);

-- Case 2 (Extending a revmap chain .. 2 revmap pages)
CREATE TABLE tbl_ao_column2 (i int) USING ao_column;
insert into tbl_ao_column2 select generate_series(1, 5);
-- Bloat gp_fastsequence so that we will have to create 2 revmap pages.
-- REVMAP_PAGE_MAXITEMS = 5456. About 32768 ints fit in one logical heap block.
-- So we need at least 32768 * 5456 + 1 = 178782209 rows to have 2 revmap pages.
SET allow_system_table_mods TO ON;
UPDATE gp_fastsequence SET last_sequence = 180000000 WHERE
  objid = (SELECT segrelid FROM pg_appendonly WHERE relid='tbl_ao_column2'::regclass);
INSERT INTO tbl_ao_column2 SELECT generate_series(6, 10);
CREATE INDEX ON tbl_ao_column2 USING brin (i) WITH (pages_per_range = 1, autosummarize=false);
});

$whiskey->wait_for_catchup($charlie, 'replay', $whiskey->lsn('insert'));

done_testing();
