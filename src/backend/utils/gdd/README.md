# Global Deadlock Detection

## Background

### Local Deadlock

Let's begin with a simple case using a similar syntax of isolation2 test
framework:

```sql
A: begin;
A: update foo set val=val where id=1;

B: begin;
B: update foo set val=val where id=2;
B&: update foo set val=val where id=1;

A&: update foo set val=val where id=2;
```

In this case there are two concurrent transactions, A and B. At first A holds
a Xid lock on the tuple with `id=1` and B holds a Xid lock with `id=2` without
blocking, but then B gets blocked by A when trying to update the tuple with
`id=1` and A gets blocked by B when trying to update the tuple with `id=2`, so
a deadlock is formed. And in this case the tuple `id=1` and the tuple `id=2` is
on the same segment, so the deadlock just happens locally.

### Local Deadlock Detection

On a single segment this deadlock can be automatically
detected. The detection is based on a cycle detection algorithm:

- each transaction is a vertex;
- each waiting relation is a directed edge;
- if there is a directed cycle in the graph then there is deadlock.

In above case there are two vertices, A and B, and there are two directed
edges, `A==>B` and `B==>A`, obviously these two edges form a directed cycle,
so the deadlock can be detected.

    +---------------------------+
    |           seg 0           |
    |                           |
    | +-----+   id=2    +-----+ |
    | |     | ========> |     | |
    | |  A  |           |  B  | |
    | |     | <======== |     | |
    | +-----+   id=1    +-----+ |
    |                           |
    +---------------------------+

### Global Deadlock

However on a distributed cluster (e.g. Greenplum DB) rows with different ids
are distributed to different segments, suppose rows with `id=1` are on seg 0
and rows with `id=2` are on seg 1, then each segment only see a part of the
graph.

    +---------------------------+    +---------------------------+
    |           seg 0           |    |           seg 1           |
    |                           |    |                           |
    | +-----+           +-----+ |    | +-----+           +-----+ |
    | |     |   id=1    |     | |    | |     |   id=2    |     | |
    | |  B  | ========> |  A  | |    | |  A  | ========> |  B  | |
    | |     |           |     | |    | |     |           |     | |
    | +-----+           +-----+ |    | +-----+           +-----+ |
    |                           |    |                           |
    +---------------------------+    +---------------------------+

This forms a global / distributed deadlock.

### Global Deadlock Prevention

To prevent global deadlock, in Greenplum DB an exclusive table lock is held
for `UPDATE` and `DELETE` commands, so concurrent updates are actually
disabled.

This policy does prevent the global deadlock, but the cost is bad performance
on concurrent updates.

## Global Deadlock Detection

If we can gather all these edges to one segment then we can form the same
graph as the [local one](#local-deadlock-detection) and detect the deadlock.

However if we make the detection based on these directly gathered edges then
some fake deadlocks can be detected. A transaction can be waiting for at most
one other transaction on any single host, but it can be waiting for multiple
other transactions globally in a distributed system. Some waiting relations
are automatically changed if the holder object changed, but some are not. An
example of unchanged waiting relations is waiting for a Xid lock(it can be only
released after the holding transaction is over). An example of changed waiting
relations is waiting for a tuple lock(the tuple lock can be released before the
holding transaction is over). For concrete examples, please refer the test cases.

A proper global deadlock detector must be designed with these distributed
system specific characteristics in consideration. The basic requirements are
that if there is a deadlock then find it out and break it, and make sure don't
make false alarms.

Our global deadlock detection algorithm is based on the local version, and has
extra logic to distinguish the false deadlocks from the real ones.

### Abstraction and Definition

- **vertex**: each transaction is a vertex;
- **edge**: if transaction A is waiting for transaction B then there is a
  directed edge from vertex A to vertex B;
- **out edge**: an edge from A to B is an out edge of A;
- **in edge**: an edge from A to B is an in edge of B;
- **out degree**: the number of a vertex's out edges is its out degree;
- **in degree**: the number of a vertex's in edges is its in degree;
- **solid edge**: if an edge from A to B can not be released until B
  COMMIT/ABORT, then this edge is a solid edge;
- **dotted edge**: if an edge from A to B can be released when or before B's
  current query is done, then this edge is a dotted edge;
- **local graph**: each segment has a local graph formed by all the local
  edges on this segment;
- **global graph**: edges on all the segment form a global graph;

### Rules and Policies

The detection is based on one basic assumption:

- if some vertices (transactions) formed a cycle (deadlock), then they and
  their edges (waiting relations) will no longer change their status;

And we know some facts:

- an edge can be either a dotted edge or a solid edge, no other possibilities;

According to these, we have below deductions:

- if the status of a vertex or an edge is possible to change, then this
  vertex/edge is not part of a cycle (yet);
- if a vertex has no out edge on any segment then its status is possible to
  change;
  - --> `Rule 1: delete a vertex if its global out degree is 0`;
- if a vertex has no in edge on any segment then it's not part of a cycle;
  - --> `Rule 2: delete a vertex if its global in degree is 0`;
- if a vertex has no out edge on segment X but has an out edge on segment Y:
  - its solid in edges on X is not possible to change;
  - its dotted in edges on X are possible to change;
    - --> `Rule 3: delete a vertex's dotted in edges on a segment if its local
      out degree is 0`;

Rule 1 & 2 are straightforward as they are also the rules for local deadlock
detection. Rule 3 needs further deductions:

Suppose:
- vertex C has no out edge on segment X, but has out edge on segment Y;
- a solid edge from A to C on X;
- an dotted edge from B to C on X;

For A:
- A is waiting for a transaction level lock/object held by C on X, C will not
  release it until end of C's transaction;
- C's transaction will only end until all C's segments end;
- as we know C has an out edge on Y, we know C will not end on Y, so C's
  transaction will not end, so that lock/object held by C on X will not be
  released on X;
- in summary, if a vertex has global out edge(s), then all its solid in edges
  on any segment can't be removed;

For B:
- B is waiting for a query level lock/object held by C on X;
- as C has no out edge on X so C's status itself is possible to change;
- so C is possible to release any query level lock/object holding now;
- so this dotted edge from B to C is possible to change on X;
- in summary, if vertex has no local out edge, then all its dotted in edges on
  this segment should be removed;

### Algorithm

With all these 3 rules we can have an algorithm as this:

```python
# reduce edges and vertices
while true:
    for vertex in vertices_with_zero_global_out_degree():
        # delete vertex and its solid/dotted in edges on all segments
        delete_vertex(vertex)

    for vertex in vertices_with_zero_global_in_degree():
        # delete vertex and its solid/dotted out edges on all segments
        delete_vertex(vertex)

    for segment in all_segments():
        for vertex in vertices_with_zero_local_out_degree(segment):
            # delete vertex's dotted in edges on segment,
            # related vertices' global in/out degrees are also updated
            delete_dotted_in_edges_on_segment(segment, vertex)

    if nothing_deleted_in_current_loop():
        # no more vertex or edge to delete
        break

# detect for deadlock cycle
if count_vertices_not_deleted() > 0:
    # detected at least one cycle
    # the cycles are only reliable if all the contained vertices
    # are valid transactions on coordinator
    if all_vertices_are_valid():
        # choose a vertex and cancel it
```

In this algorithm we need to maintain below information:
- local in/out edges of all the vertices on each segment;
- global in/out degrees of all the vertices;

We maintain a local graph on each segment just like the local deadlock
detection algorithm, the difference is that we also introduce extra rules to
reduce edges/vertices based on global information. But there is no need to
also maintain a global graph.

### Edge Collection

Edges can be collected with the `pg_locks` view.

```sql
select a.*, b.*
from pg_locks a
join pg_locks b on
  a.locktype=b.locktype and
  a.pid<>b.pid and
  a.granted='f' and
  a.locktype in ('transactionid', 'relation', 'tuple') and
  ((a.locktype='transactionid' and
    a.transactionid=b.transactionid) or
   (a.locktype='relation' and
    a.database=b.database and
    a.relation=b.relation) or
   (a.locktype='tuple' and
    a.database=b.database and
    a.relation=b.relation and
    a.page=b.page and
    a.tuple=b.tuple));
```

However `pg_locks` lack some necessary information:
- the waiter xid;
- the edge type: dotted or solid;

So we have to create a new udf `gp_dist_wait_status` to collect all the
necessary information, this udf is similar to `pg_locks`, but with these
information included, an example output:

```sql
select * from gp_dist_wait_status();
 segid | waiter_dxid | holder_dxid | holdTillEndXact
-------+-------------+-------------+------------
    -1 |          29 |          28 | t
     0 |          28 |          26 | t
     0 |          27 |          29 | t
     1 |          26 |          27 | t
(4 rows)
```

### Impact of The Async Edge Collection

The local wait-relationship graphs collected from each segment are generated
asynchronously. We have to take the impact into account.

- if a transaction A is valid on coordinator then it's valid on all segments;
- transaction A's solid in edges are all valid as long as A is valid on coordinator;
- if there is a solid edge from B to A on a segment, then B is valid as long as
  A is valid;
- further more, if there is a solid edge from B to A on a segment, then B is
  valid on all segments as long as A is valid on coordinator;
- if there is a dotted edge from B to A on a segment, then this edge is valid
  unless A's status changed;
- if there is a solid edge from B to A then all the vertices wait for B
  directly or indirectly are valid on all segments;

Our algorithm reduces all the edges that are possible to change. On each
segment the reduced graph's end vertices (vertices whose local out degree is
0) must only have solid in edges. We can validate these transactions on coordinator,
if they are all valid then those solid edges to them are also valid, so the
transactions waiting for them directly or indirectly are valid, so the graphs
are valid and the detected deadlock cycles are true; if any of the
transactions is invalid on coordinator then the graphs are not reliable and we
should retry later.

So our algorithm only requires the edges information to be consistent at
segment level, different segments can report their information at different
time.

### Edge Classifying

Edge types are detected locally by `gp_dist_wait_status` on each segment, the
policies are:
- xid waitings are solid edges;
- relation waitings that ever been closed in `NO_LOCK` mode at least once are
  solid edges, otherwise they are dotted edges;
- all other waitings are dotted edges;

For relation locks, there is ref counts in both `Relation` and `LOCK`, if a
relation lock is opened in non `NO_LOCK` mode but closed in `NO_LOCK` mode
then the ref count in `LOCK` can no longer reach 0, so the lock can no loner
be released until end of transaction, that's why we can take it as a solid
edge.

### Case Analysis

Let's run the new algorithm on an actual case to understand how does it work.

The testing cluster has 3 segments: seg -1, seg 0 and seg 1. Coordinator is on seg
-1.  A table `t1` is created as below:

```sql
create table t1 (id int, val int);
insert into t1 values (1,1), (2,2), (3,3), (4,4);
```

Its data distribution is as below:

```sql
select gp_segment_id, * from t1 order by id;
 gp_segment_id | id | val
---------------+----+-----
             0 |  1 |   1
             1 |  2 |   2
             0 |  3 |   3
             1 |  4 |   4
(4 rows)
```

The sessions are like below:

```sql
C: begin;
C: update t1 set val=30 where id=2;

A: begin;
A: update t1 set val=10 where val=3;

B: begin;
B: update t1 set val=20 where id=4;
B&: update t1 set val=20 where val=3 or id=2;
-- B ==> A: xid lock on seg 0
-- B ==> C: xid lock on seg 1

A&: update t1 set val=10 where val=2;
-- A ~~> B: tuple lock on seg 1

D: begin;
D&: update t1 set val=40 where id=4;
-- D ==> B: xid lock on seg 1
```

With the collected edges we form below original graphs on each segment:

    +-----------------------+    +-------------------------------------+
    |         seg 0         |    |                seg 1                |
    |                       |    | +-----+                             |
    | +-----+       +-----+ |    | |  A  | ~~~~> +-----+       +-----+ |
    | |     |       |     | |    | +-----+       |     |       |     | |
    | |  B  | ====> |  A  | |    |               |  B  | ====> |  C  | |
    | |     |       |     | |    | +-----+       |     |       |     | |
    | +-----+       +-----+ |    | |  D  | ====> +-----+       +-----+ |
    |                       |    | +-----+                             |
    +-----------------------+    +-------------------------------------+

Edges are reduced in this order:
- `B1 ==> C1`: as C's global out degree is 0 (Rule 1);
- `D1 ==> B1`: as D's global in degree is 0 (Rule 2);
- `A1 ~~> B1`: as B1's local out degree is 0 and it's a dotted edge (Rule 3);
- `B0 ==> A0`: as A' global out degree is 0 (Rule 1);

As all edges are removed, there is no deadlock cycle.

 vi:cc=80:tw=78:ai:et:
