For historical reasons, append-optimized are also called
"append-only". They used to be truly append-only in previous versions
of Greenplum, but these days they can in fact be updated and deleted
from. The segment files that store the tuples at the storage level are
append-only - data is never in-place updated or overwritten physically -
but from a user's point of view, the table can be updated and deleted
from like a heap table. The AO visibility map and MVCC on the metadata
tables make updates and deletes to work.

Append-Optimized, Column Storage tables (AOCS), follow a similar
layout, but there is some extra metadata to track which segment file
corresponds to which column.  The source code lives in a separate
directory, but was originally copy-pasted from append-only code, and
the high-level layout of the files is the same.


# Segment file format

The tuples in an append-optimized table are stored in a number of
"segment files", or "segfiles" for short. The segment files are stored
in the data directory, in files name "`<relfilenode>.<segno>`", just like
heap tables.  Unlike heap tables, each segment in an append-only table
can have different size, and some of them can even be missing. They can
also be larger than 1 GB, and are not expanded in BLCKSZ-sized blocks,
like heap segments are. Segment zero is empty unless data has been
inserted during utility mode, in which case it's inserted into segment
zero. Extending the table by adding attributes via ALTER TABLE will also
push data to segment zero. Each table can have at most 127 segment files.

An append-only segfile consists of a number of variable-sized blocks
("varblocks"), one after another. The varblocks are aligned to 4 bytes.
Each block starts with a header. There are multiple kinds of blocks, with
different header information. Each block type begins with common "basic"
header information, however. The different header types are defined in
cdbappendonlystorage_int.h. The header also contains two checksums,
one for the header, and another for the whole block.


# pg_appendonly table

The "`pg_appendonly`" catalog table stores extra information for each AO
table.  You can think of it as an extension of `pg_class`.


# Aosegments table

In addition to the segment files that store the user data, there are
three auxiliary heap tables for each AO table, which store metadata.
The aosegments table is always named as "`pg_aoseg.pg_aoseg_<oid>`",
where `<oid>` is the initial Oid with which the table was created. This
is not guaranteed to match the current Oid of the aosegments table,
as some ALTER TABLE operations will rewrite the table causing the
Oid to change. In order to find the aosegments table of an AO table,
the "`pg_catalog.pg_appendonly`" catalog relation must be queried.
Aosegment tables are similar to the TOAST tables, for heaps. An
append-only table can have a TOAST table, too, in addition to the
AO-specific auxiliary tables.

The segment table lists all the segment files that exist. For each
segfile, it stores the current length of the table. This is used to
make MVCC work, and to make AO tables crash-safe. When an AO table
is loaded, the loading process simply appends new blocks to the end of
one of the segment files.  Once the loading statement has completed,
it updates the segment table with the new EOF of that segment file. If
the transaction aborts, or the server crashes before end-of-transaction,
the EOF stored in the aosegment table stays unchanged, and readers
will ignore any data that was appended by the aborted transaction. The
segments table is a regular heap table, and reading and updating it
follow normal the MVCC rules. Therefore, when a transaction has a
snapshot that was taken before the inserting transaction committed,
that transaction will still see the old EOF value, and therefore also
ignore the newly-inserted data.


# Visibility map table

The visibility map for each AO table is stored in a heap table named
"`pg_aoseg.pg_aovisimap_<oid>`".  It is not to be confused with
the visibility map used for heaps in PostgreSQL 8.4 and above!

The AO visibility map is used to implement `DELETEs` and `UPDATEs`. An
`UPDATE` in PostgreSQL is like `DELETE+INSERT`. In heap tables, the ctid
field is used to implement update-chain-following when updates are
done in in `READ COMMITTED` mode, but AO tables don't store that
information, so an update of a recently updated row in read committed
mode behaves as if the row was deleted.

The AO visiblity map works as an overlay, over the data. When a row
is `DELETEd` from an AO table, the original tuple is not modified. Instead,
the tuple is marked as dead in the visibility map.

The tuples in the visibility map follow the normal MVCC rules. When
a tuple is deleted, the visibility map tuple that covers that row
is updated, creating a new tuple in visibility map table, and
setting the old tuple's xmax. This implements the MVCC visibility
for deleted AO tables. Readers of the AO table read the visibility
map using the same snapshot as it would for a heap table. If a
deleted AO tuple is supposed to still be visible to the reader, the
reader will find and use the old visibility map tuple to determine
visibility, and it will therefore still see the old AO tuple as
visible.


# Block directory table

The block directory is used to enable random access to an append-only
table. Normally, the blocks in an append-only table are read
sequentially, from beginning to end. To make index scans possible, a
map of blocks is stored in the block directory. The map can be used
to find the physical segment file and offset of the block, that contains
the AO tuple with given TID. So there is one extra level of indirection
with index scans on an AO table, compared to a heap table.

The block directory is only created if it's needed, by the first
`CREATE INDEX` command on an AO table.


# TIDs and indexes

Indexes, and much of the rest of the system, expect every tuple to
have a unique physical identifier, the Tuple Identifier, or `TID`. In a
heap table, it consists of the heap block number, and item number in
the page. An AO table uses a very different physical layout,
however. A row can be located by the combination of the segfile number
its in, and its row number within the segfile. Those two numbers are
mapped to the heap-style block+offset number, so that they look like
regular heap `TIDs`, and can be passed around the rest of the system,
and stored in indexes. See appendonlytid.h for details.


# Vacuum

Append-only segment files are read-only after they're written, so
Vacuum cannot modify them either. Vacuum reads tuples from an existing
segment file, leaves out those tuples that are dead, and writes other
tuples back to a different segment file. Finally, the old segment file
is deleted. This is like `VACUUM FULL` on heap tables: new index entries
are created for every moved tuple.
