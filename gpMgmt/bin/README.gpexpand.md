# Gpexpand

## Expansion
New segments are added to the cluster without stopping the cluster, most
concurrent queries are still allowed during the expansion:
- read/insert/update/delete data tables: allowed;
- read/insert/update/delete catalog tables on segments: allowed;
- read catalog tables on master: allowed;
- insert/update/delete catalog tables on master: disallowed;
  - Reason: The expansion will copy catalog files from master to new segments
    to build new segments, and old transactions are still running on old
    segments. So any catalog changes will not dispatch to new segments, this
    will cause catalog inconsistency.

The general tasks are as below:
- creation of new segments;
- add the new segments into the cluster;
- data reorganization;

### Creation of new segments
The general workflow is as below:
- install binaries (e.g. the `postgres` command) from master to new segments;
- lock catalog updates on master;
- copy base files (e.g. catalog tables, xlogs, configuration files) from
  master to new segments;
- update configurations on new segments:
  - port number in `postgresql.conf`;
  - dbid;
- update `pg_hba.conf` for the new segments to be accessable;

The catalog lock will be explained later.

After the creation we could now launch the new segments, but they are not yet
added to the cluster.

### Add the new segments into the cluster:
The general workflow is as below:
- add the new segments into `gp_segment_configuration` with status 'u';
- bump the expand version so other backends can get newest `gp_segment_
  confiuration`;
- unlock catalog updates on master;

New segments can't and shouldn't be visible to other processes (user
connections and auxiliary processes) until they are added to `gp_segment_
configuration` so others can know about their existance and information;

Each global transaction will fetch or update a snapshot of `gp_segment_
configuration` at the beginning of transaction, so new segments can be used in
any newly created transactions, but for transactions that have got snapshot
of `gp_segment_configuration` they will still work on old segments. In other
words, if a transaction begins with N segments then it will only see these N
segments during the transaction.

### Data reorganization

#### Obsolete implementation

If a table T is created when cluster size is N, can we get correct
query/insert/update/delete results after expanding the cluster size to M?
This depends on T's distribution policy:

- replicated: incorrect, the table's data do not exist on the new segments;
- hash distributed: incorrect, the map from hash code to segment id depends on
  the cluster size;
- randomly distributed: correct, the table is always scanned on all the
  segments anyway;

To get correct results on replicated and hash distributed tables we should
reorganize them from N segments to M segments, but this is a slow process, and
need to block updates to them during the process, it is not always expected or
tolerable.

To improve the user experience the replicated and hash distributed tables are
all altered to randomly distributed temporarily, then we could get correct
query/insert/delete/update results even without data reorganization.  This
allow data reorganization to be postponed.  Later we could alter them back to
their original distribution policy, which also triggers data reorganization,
to complete the expansion process eventually.  However there are known issues
on this solution:

1. queries on randomly distributed tables are slow as data always need to be
   broadcasted on join;
2. there is a small time window new segments are added but old tables are not
   altered to randomly yet, queries on them will get wrong results;
3. newly created tables might not be altered to randomly;

#### New implementation

In fact it is possible to get correct query/insert/update/detele results
immediately after cluster expansion without data reorganization and still keep
tables' original distribution policies.

We just need to record the number of segments N on which the old tables are
created.  Now we could correctly query/insert/update/delete on N segments no
matter cluster size is N or M, there is no performance loss, which leaves more
time for the user to reorganize the tables.

### Catalog lock

Data query/insert/delete/update are not blocked during cluster expansion.
Queries on catalog tables are neither blocked, but updates on most of catalog
tables are not allowed.

Once we begin to copy base files from master to new segments we have to
prevent concurrent catalog changes on master, this is done via a catalog lock.
On master all insert/delete/update to catalog tables acquire shared access on
this lock, while gpexpand acquire exclusive access on the same lock.  When
there are already uncommitted catalog changes gpexpand has to wait for all of
them to commit or rollback.  Once gpexpand acquired the lock all concurrent
catalog changes will fail immediately. For catalog changes should be
dispatched to new segments, but they are invisible for running transactions.

Even after gpexpand released the lock the catalog updates in old transactions
(those began when cluster size was still N) will still fail immediately
because these changes will not be dispatched to the new segments.

Updates on some of the catalog tables are always allowed as they are
considered __local__ or __master only__ catalog tables, their content will be
truncated anyway after copying to new segments.  These catalog tables are:

    gp_segment_configuration
    gp_configuration_history
    gp_segment_configuration
    pg_description
    pg_partition
    pg_partition_rule
    pg_shdescription
    pg_stat_last_operation
    pg_stat_last_shoperation
    pg_statistic
    pg_partition_encoding
    pg_auth_time_constraint

 vi:filetype=markdown:cc=80:tw=78:ai:et:
