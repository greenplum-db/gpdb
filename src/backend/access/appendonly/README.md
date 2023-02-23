For historical reasons, append-optimized tables are also called
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
push data to segment zero. Each table can have at most 128 segment files.

AOCS tables can similarly have at most 128 segment files for each column.
The range of segno is dependent on the filenum value in pg_attribute_encoding.
`segno 0,1-127 (filenum = 1), segno 128,129-255 (filenum = 2),...`
To find the file range for an AOCS table column, find the attnum of that column
and then find the corresponding filenum for that attnum from pg_attribute_encoding.

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
The aosegments table is one of them, which stores helpful attributes
such as `modcount`, which is a bearing on the number of DML operations
that the AO table has been subject to.

The aosegments table is initially named: "`pg_aoseg.pg_aoseg_<oid>`",
where `<oid>` is the oid of the AO table. However, if certain DDL statements 
such as ALTER, that involve a rewrite of the AO table on disk, are applied, 
this aosegments table is replaced by a new table with a changed `<oid>` suffix.
The complete process involves creation of a temporary table followed by an
update of the `relfilenode` and aosegments table oid associated with the AO table. 

In order to find the current aosegments table of an AO table, the 
"`pg_catalog.pg_appendonly`" catalog table must be queried. The `segrelid` column in 
this table yields the oid of the current aosegments table. If we now query
pg_class with this oid, we will get the name of the current aosegments table. 
Please note that the `segrelid` is not equal to the `<oid>` suffix of the current 
aosegments table.

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
field is used to implement update-chain-following when updates are done
in `READ COMMITTED` mode, but AO tables don't store that information, so
an update of a recently updated row in read committed mode behaves as if
the row was deleted.

The AO visibility map works as an overlay, over the data. When a row
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


# Locking and snapshots

Scans lock the table using the lockmode determined by normal PostgreSQL
rules. A scan uses its snapshot to read the pg_aoseg table, and then
proceeds to read the segfiles that were visible to the snapshot.

When an insert or update needs a target segfile to insert rows to, it
scans pg_aoseg table for a suitable segfile, and locks its pg_aoseg
tuple, using heap_lock_tuple(). This scan is made with a fresh catalog
snapshot (formerly SnapshotNow). To make this scan and locking free from
race conditions, it can be only performed by one segment at a time. For
that, the backend holds the relation extension lock while scanning
pg_aoseg for the target segment, but that lock is released as soon as
the pg_aoseg tuple is locked.

Vacuum truncation (to release space used by aborted transactions) also
scans pg_aoseg with a fresh catalog snapshot, and holds the relation
extension lock. It skips over tuples that are locked, to not interfere
with inserters.

Vacuum compaction locks both the segfile being compacted, and the
insertion target segment where the surviving rows are copied, the same
way that an insertion does.

Vacuum drop phase, to recycle segments that have been compacted,
checks the xmin of each AWAITING_DROP segment. If it's visible to
everyone, the segfile is recycled. It uses the relation extension lock
to protect the scan over pg_aoseg.


# Unique indexes

To answer uniqueness checks for AO/AOCO tables, we have a complication. Unlike
heap, in AO/CO we don't store the xmin/xmax fields in the tuples. So, we have to
rely on block directory rows that "cover" the data rows to satisfy index lookups.
Since the block directory is maintained as a heap table, visibility checks on it
are identical to any other heap table: the xmin/xmax of the block directory
row(s) will be leveraged. This means we don't have to write any special
visibility checking code ourselves, nor do we need to worry about transactions
vs subtransactions.

Since block directory rows are written usually much after the data row has been
inserted, there are windows in which there is no block  directory row on disk
for a given data row - a problem for concurrent unique index checks. So during
INSERT/COPY, at the beginning of the insertion operation, we insert a
placeholder block directory row to cover ALL future tuples going to the current
segment file for this command.

To answer unique index lookups, we don't have to physically fetch the tuple from
the table. This is key to answering unique index lookups against placeholder
rows which predate their corresponding data rows. We simply perform a sysscan of
the block directory, and if we have a visible entry that encompasses the rowNum
being looked up, we go on to the next check. Otherwise, we have no conflict and
return. The next check that we need to perform is against the visimap, to see if
the tuple is visible. If yes, then we have a conflict. Since the snapshot used
to perform uniqueness checks for AO/CO is SNAPSHOT_DIRTY (we currently don't
support SNAPSHOT_SELF used for CREATE UNIQUE INDEX CONCURRENTLY), it is possible
to detect if the block directory tuple (and by extension the data tuple) was
inserted by a concurrent in-progress transaction. In this case, we simply avoid
the visimap check and return true. The benefit of performing the sysscan on the
block directory is that HeapTupleSatisfiesDirty() is called, and in the process,
the snapshot's xmin and/or xmax fields are updated (see SNAPSHOT_DIRTY for
details on its special contract). Returning true in this situation will lead to
the unique index code's xwait mechanism to kick in (see _bt_check_unique()) and
the current transaction will wait for the one that inserted the tuple to commit
or abort.

Tableam changes: Since there is a lot of overhead (leads to ~20x performance
degradation in the worst case) in setting up and tearing down scan descriptors
for AO/CO tables, we avoid the scanbegin..fetch..scanend construct in
table_index_fetch_tuple_check().

So, a new tableam API index_fetch_tuple_exists() is used, which is implemented
only for AO/CO tables. Here, we fetch a UniqueCheckDesc, which stores all the
in-memory state to help us perform a unique index check. This descriptor is
attached to the DMLState structs. The descriptor holds a block directory struct
and a visimap struct. Furthermore, we initialize this struct on the first unique
index check performed, akin to how we initialize descriptors for insert and delete.

AO lazy VACUUM is different from heap vacuum in the sense that ctids of data
tuples change (and the index tuples need to be updated as a consequence). It
leverages the scan and insert code to scan live tuples from each segfile and to
move (insert) them in a target segfile. While moving tuples, we need to avoid
performing uniqueness checks from the insert machinery. This is to ensure that
we avoid spurious conflicts between the moved tuple and the original tuple. We
don't need to insert a placeholder row for the backend running vacuum as the old
index entries will still point to the segment being compacted. This will be the
case up until the index entries are bulk deleted, but by then the new index
entries along with new block directory rows would already have been written and
would be able to answer uniqueness checks.

Transaction isolation: Since uniqueness checks utilize the special dirty
snapshot, these checks can cross transaction isolation boundaries. For instance,
let us consider what will happen if we are in a repeatable read transaction and
we insert a key that was inserted by a concurrent transaction. Further let's say
that the repeatable read transaction's snapshot was taken before the concurrent
transaction started. This means that the repeatable read transaction won't be
able to see the conflicting key (for eg. with a SELECT). In spite of that
conflicts will still be detected. Depending on whether the concurrent
transaction committed or is still in progress, the repeatable read transaction
will raise a conflict or enter into xwait respectively. This behavior is table
AM agnostic.

Partial unique indexes: We don't have to do anything special for partial indexes.
Keys not satisfying the partial index predicate are never inserted into the
index, and hence there are no uniqueness checks triggered (see
ExecInsertIndexTuples()). Also during partial unique index builds, keys that
don't satisfy the partial index predicate are never inserted into the index
(see *_index_build_range_scan()).
