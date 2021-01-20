# gpexpand

gpexpand is Greenplum cluster expansion tool which expands an existing Greenplum
Database by adding new hosts to the cluster.

gpexpand has two phases:

* initialization phase: add new nodes to the cluster, initialize new nodes by
populating catalog to the new nodes, and get visible in cluster.
* redistribution phase: redistribute user data to new nodes.

## 1. Initialization phase

gpexpand initialization phase adds new nodes to the cluster, populate catalog to the new nodes,
and get ready for user data redistribution. During this phase, DDL operations
are disabled to avoid catalog inconsistency between existing segments and new segments.

### 1.1 Major workflow during initialization phase

	- Install binaries (e.g. the `postgres` command) on new nodes manually.

	- Prepare new nodes. After this, new segments are startable, while not
	  visible yet in the cluster.

		- lock catalog to prevent catalog modifications on coordinator (details explained in the
		  later section)
		- copy base files (e.g. catalog tables, xlogs, configuration files)
		  from coordinator to new segments
		- update configurations on new segments, such as dbid, port number in
		  `postgresql.conf`
		- update `pg_hba.conf` for the new segments to be accessible

	- Make the new nodes visible to the cluster. After this, new segments are visible
	  to the cluster, while contains no user data.

		- add the new segments into table `gp_segment_configuration` with status 'u'
		- bump the expand version so other backends can get newest `gp_segment_configuration`
		- unlock catalog updates on coordinator

### 1.2 Preventing catalog inconsistency

To avoid catalog inconsistency across existing segments and new segments,
catalog change is blocked during initialization phase, thus DDL operations (on most
tables) are disallowed.

Once gpexpand starts to copy base files from coordinator to new segments, gpexpand have to
prevent concurrent catalog changes on coordinator. To do so, a new catalog lock is added.
On coordinator all changes to catalog tables acquire shared access on
this lock, while gpexpand acquires access exclusive mode on the same lock. If
there are existing uncommitted catalog changes, gpexpand must wait for all of
them to commit or rollback. Once gpexpand has held the lock, all other concurrent
catalog changes will fail immediately, otherwise, those transactions cannot apply
catalog changes to new segments, and result in catalog inconsistency.

Updates on some of the catalog tables are always allowed as they are
considered __local__ or __coordinator only__ catalog tables, their contents will be
truncated anyway after copying to new segments. These catalog tables are:

    gp_configuration_history
    gp_segment_configuration
    pg_auth_time_constraint
    pg_description
    pg_shdescription
    pg_stat_last_operation
    pg_stat_last_shoperation
    pg_statistic

## 2. Redistribution phase

gpexpand redistribution phase redistributes user's data across the whole cluster
one table by one table. User could adjust table redistribution order.

During this phase, table currently under expansion is not accessible. gpexpand
acquires `Access Exclusive Lock` on redistributing table to prevent concurrent.

User data are redistributed in a CTAS-like style, id redistributes all data to
a new table, swaps old and new table's relfilenode, and finally drop old data.
CTAS has following characteristics:

* CTAS needs no VACUUM to reclaim moved tuples.
* CTAS rebuilds the index from scratch instead of tuple by tuple.

## 3. Performance

gpexpand might take days to expand big clusters, so it is important to reduce its
impact to workload during expansion, and to improve gpexpand's performance.

gpexpand in 5.x firstly changes all hash distributed tables to randomly distributed
tables, then expands those tables one by one to new nodes. Thus data locality is
lost during expansion, query performance is bad, and lots of tuples needs to move.
Greenplum 6 improves both of them.

### 3.1 Keep data locality

To keep data locality and avoid unnecessary motion for queries during expansion,
`gp_distribution_policy.numsegments` is used to record how many nodes are used by
each table. Optimizer uses this info to generate optimized plan, and the performance
impact due to expansion is minimum.

### 3.1 Minimize tuples moved across cluster

Below is how 5.x determine tuple's location for hash distributed table:

1. calculate hash value (which is an unsigned int) of a tuple according to the
hash-distributed columns' value and type. Call this hash policy as modulo hash
in this document.
2. reduce the hash value to segment id (unsigned int -> [0~N-1], N is the cluster
size).

To reduce the number of tuples moved across cluster, jump consistent hash is
introduced. For more info, refer to https://arxiv.org/pdf/1406.2294.pdf.

With jump consistent hash, the number of moved tuples could be calculated as below:

* Before redistribution, each segment(seg0, ..., segN-1) contains 1/N of the
whole data (N is the total number of segments).
* After redistribution, each segment(seg0, ..., segN+M-1) contains 1/M+N of the
whole data (M is the number of new segments).
* So each old segment should send out (1/N - 1/M+N = M/((M+N)N)) of the whole data,
which is M/M+N of its own data. The number of moved tuples is much less than old
modulo hash (Old modulo hash might move almost every tuple during expansion).
* Each new segment will receive and insert 1/M+N of the whole data. This is
always the same no matter what reduce-hash method is used.

GUC `gp_use_legacy_hashops` controls which hash policy to use when creating table:
jump consistent hash or modulo hash. By default Greenplum 6 uses jump consistent
hash. (Refer to PR #6327 for more details about this GUC).

## 4. Integration

### 4.1 Upgrade

Upgrade by design uses utility mode to create schema. During utility mode, Greenplum
could not determine cluster size, so `gp_distribution_policy.numsegments` will be
-1 for all tables. Upgrade updates numsegments to cluster size explicitly.

During expansion, upgrade is not supported. During upgrade, expansion is not supported.

#### 5 to 6 upgrade

Table's hash policy needs to match with data's actual distribution after upgrade.
Greenplum 5 uses modulo hash. Greenplum 6 uses jump consistent hash by default
for all tables. upgrade uses `gp_use_legacy_hashops` GUC to keep backward
compatibility.

To get the benefit of jump consistent hash when expanding, it is recommended to
transfer database to jump consistent hash after upgrade. Tools will be provided
to do so.

gpexpand expands cluster and transfer modulo hash distributed data to jump
consistent hash altogether.

#### 6 to 6 upgrade

If database uses modulo hash policy, `gp_use_legacy_hashops` GUC is needed.

#### 6 to 7 upgrade

Greenplum 7 plans to remove modulo hash. User needs to transfer their modulo based
databases to jump consistent hash before upgrade. Tools will be provided to do so.

### 4.2 Backup and restore

Greenplum 6 has two hash policies: modulo and jump consistent hash. Jump consistent
hash is the default policy.

When restoring modulo based backup data to Greenplum 6, set GUC
`gp_use_legacy_hashops` to use modulo hash policy. This is needed when backup is
taken on Greenplum 5 cluster, or when backup is taken on 6 cluster which is
upgraded from 5.

Backup could be taken at any time, and restore could be applied any time later.
If table's hash policy does not match with actual data, restore fails with error
message. For example:

- backup before expanding, restore after expanding: as segment number changed,
restore will fail;
- backup data is modulo based, restore to jump consistent hash based database tables.
- backup data is jump consistent hash based, restore to modulo based database tables.

If do want to restore for above situation, change table to use random distribution,
restore and then change back to original distribution using CTAS.

During expansion, backup and restore is not supported.

### 4.3 Utilities

During initialization phase, utilities like gpconfig, gppkg does not work.

gpstate shows expansion status if there are expanding in progress.
