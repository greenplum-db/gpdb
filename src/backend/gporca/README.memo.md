# Understanding Orca Memos

An Orca memo is a structure that enumerates different, but logically identical
plans, and then optimizes to find the most efficient one. Let's explore how to
read the memo.

### Setup

Apply following GUCs in psql session:
```sql
-- These GUCS print the memo structure to the console and provide the necessary
-- information to follow the logic to derive the optimal plan
SET client_min_messages=log;
SET optimizer_print_memo_after_optimization=on;
SET optimizer_print_optimization_context=on;
```

Additional GUCs may be used to see the memo at different stages of the
optimization process.
```sql
SET optimizer_print_memo_after_exploration=on;
SET optimizer_print_memo_after_implementation=on;
```

### How to traverse a memo

Let's start with simple case with multiple paths.
```sql
CREATE TABLE t(a int);
EXPLAIN SELECT * FROM t t1 JOIN t t2 ON t1.a=t2.a;
```

First find `ROOT` label. In this example `ROOT` began at Group 5:
```
ROOT
Group 5 (#GExprs: 8):
  0: CLogicalNAryJoin [ 0 1 4 ]
  1: CLogicalInnerJoin [ 0 1 4 ]
  2: CLogicalInnerJoin [ 1 0 4 ]
  3: CPhysicalInnerHashJoin (High) [ 1 0 4 ]
    Cost Ctxts:
      main ctxt (stage 0)1.0, child ctxts:[6, 6], ..., cost: 862.000429
      main ctxt (stage 0)1.2, child ctxts:[5, 5], ..., cost: 862.000643
      main ctxt (stage 0)1.3, child ctxts:[4, 4], ..., cost: 862.000537
      main ctxt (stage 0)0.3, child ctxts:[4, 4], ..., cost: 862.000537
  4: CPhysicalInnerNLJoin [ 1 0 4 ]
    Cost Ctxts:
      main ctxt (stage 0)1.3, cost lower bound: 1324031.092755   PRUNED
      main ctxt (stage 0)0.3, cost lower bound: 1324031.116949   PRUNED
  5: CPhysicalInnerHashJoin (High) [ 0 1 4 ]
    Cost Ctxts:
      main ctxt (stage 0)1.0, child ctxts:[3, 3], ..., cost: 862.000429
      main ctxt (stage 0)1.2, child ctxts:[2, 2], ..., cost: 862.000643
      main ctxt (stage 0)1.3, child ctxts:[0, 0], ..., cost: 862.000537
      main ctxt (stage 0)0.3, child ctxts:[0, 0], ..., cost: 862.000537
  6: CPhysicalInnerNLJoin [ 0 1 4 ]
    Cost Ctxts:
      main ctxt (stage 0)1.3, cost lower bound: 1324031.092755   PRUNED
      main ctxt (stage 0)0.3, cost lower bound: 1324031.116949   PRUNED
  7: CPhysicalMotionGather(coordinator) [ 5 ]
    Cost Ctxts:
      main ctxt (stage 0)0.0, child ctxts:[1], rows:1.000000 (group), cost: 862.000458
  Grp OptCtxts:
    0 (stage 0): (req CTEs: [], ...) => Best Expr:7
    1 (stage 0): (req CTEs: [], ...) => Best Expr:5
```
In `ROOT` group we begin at group optimization context (`Grp OptCtxts`) 0.

```
Group 5 (#GExprs: 8):
  ...
  Grp OptCtxts:
    0 (stage 0): (req CTEs: [], ...) => Best Expr:7
```

In this case, it picked best expression 7 (`Best Expr:7`). If we look at `Expr:7` of Group 5...

```
Group 5 (#GExprs: 8):
  ...
  7: CPhysicalMotionGather(coordinator) [ 5 ]
    Cost Ctxts:
      main ctxt (stage 0)0.0, child ctxts:[1], rows:1.000000 (group), cost: 862.000458
```

We see `CPhysicalMotionGather(coordinator) [ 5 ]`. This tells us that the next group
is again Group 5. There is also a single entry in cost context with cost
862.000458 and `child ctxts:[1]`.  This tells us that the next group
optimization context is 1.

```
Group 5 (#GExprs: 8):
  ...
  Grp OptCtxts:
    ...
    1 (stage 0): (req CTEs: [], ...) => Best Expr:5
```

Group optimization context 1 indicates that the next expression is `Expr:5` of Group 5.

```
Group 5 (#GExprs: 8):
  ...
  5: CPhysicalInnerHashJoin (High) [ 0 1 4 ]
    Cost Ctxts:
      main ctxt (stage 0)1.0, child ctxts:[3, 3], ..., cost: 862.000429
      main ctxt (stage 0)1.2, child ctxts:[2, 2], ..., cost: 862.000643
      main ctxt (stage 0)1.3, child ctxts:[0, 0], ..., cost: 862.000537
      main ctxt (stage 0)0.3, child ctxts:[0, 0], ..., cost: 862.000537
```

If we look at `Expr:5`, we notice that the memo selected an inner hash join.
`CPhysicalInnerHashJoin (High) [ 0 1 4 ]` is read as `[ outer, inner, scalar ]`.
There are 4 cost contexts and the lowest cost is `862.000429`.

```
Group 5 (#GExprs: 8):
  ...
  5: CPhysicalInnerHashJoin (High) [ 0 1 4 ]
    Cost Ctxts:
      main ctxt (stage 0)1.0, child ctxts:[3, 3], ..., cost: 862.000429
      ...
```

The lowest cost of the `CPhysicalInnerHashJoin` node has child contexts `[3, 3]`
which is read as `[ outer, inner ]`. Next if we stitch together
`CPhysicalInnerHashJoin [ 0 1 4 ]` with `child ctxts:[3, 3]` we get the
children of `CPhysicalInnerHashJoin` to be:
- outer child: group 0, optimization context 3
- inner child: group 1, optimization context 3
- scalar child: group 4, no optimization context.

Let's continue with the outer child (group 0 with context 3):
```
Group 0 (#GExprs: 4):
  0: CLogicalGet "t" ("t"), Columns: ["a" (0), "ctid" (1), "xmin" (2), "cmin" (3), "xmax" (4), "cmax" (5), "tableoid" (6), "gp_segment_id" (7)] Key sets: {[1,7]} [ ]
  1: CPhysicalTableScan "t" ("t") [ ]
    Cost Ctxts:
      main ctxt (stage 0)3.0, child ctxts:[], ..., cost: 431.000019
      main ctxt (stage 0)6.0, child ctxts:[], ..., cost: 431.000019
      main ctxt (stage 0)1.0, child ctxts:[], ..., cost: 431.000019
      main ctxt (stage 0)2.0, child ctxts:[], ..., cost: 431.000019
  2: CPhysicalMotionGather(coordinator) [ 0 ]
    Cost Ctxts:
      main ctxt (stage 0)0.0, child ctxts:[1], ..., cost: 431.000071
      main ctxt (stage 0)4.0, child ctxts:[1], ..., cost: 431.000071
  3: CPhysicalMotionBroadcast  [ 0 ]
    Cost Ctxts:
      main ctxt (stage 0)5.0, child ctxts:[1], rows:1.000000 (group), cost: 431.000241
  Grp OptCtxts:
    0 (stage 0): (req CTEs: [], ...) => Best Expr:2
    1 (stage 0): (req CTEs: [], ...) => Best Expr:1
    2 (stage 0): (req CTEs: [], ...) => Best Expr:1
    3 (stage 0): (req CTEs: [], ...) => Best Expr:1
    4 (stage 0): (req CTEs: [], ...) => Best Expr:2
    5 (stage 0): (req CTEs: [], ...) => Best Expr:3
    6 (stage 0): (req CTEs: [], ...) => Best Expr:1
```

Group 0 with group optimization context 3 tells us the best expression is 1.
```
Group 0 (#GExprs: 4):
  ...
  Grp OptCtxts:
    3 (stage 0): (req CTEs: [], ...) => Best Expr:1
```

In Group 0 with expression 1 we see a table scan:
```
Group 0 (#GExprs: 4):
  ...
  1: CPhysicalTableScan "t" ("t") [ ]
    Cost Ctxts:
      main ctxt (stage 0)3.0, child ctxts:[], ..., cost: 431.000019
      main ctxt (stage 0)6.0, child ctxts:[], ..., cost: 431.000019
      main ctxt (stage 0)1.0, child ctxts:[], ..., cost: 431.000019
      main ctxt (stage 0)2.0, child ctxts:[], ..., cost: 431.000019
```

Now let's continue with the inner child (group 1 with context 3):
```
Group 1 (#GExprs: 4):
  0: CLogicalGet "t" ("t"), Columns: ["a" (8), "ctid" (9), "xmin" (10), "cmin" (11), "xmax" (12), "cmax" (13), "tableoid" (14), "gp_segment_id" (15)] Key sets: {[1,7]} [ ]
  1: CPhysicalTableScan "t" ("t") [ ]
    Cost Ctxts:
      main ctxt (stage 0)6.0, child ctxts:[], ..., cost: 431.000019
      main ctxt (stage 0)5.0, child ctxts:[], ..., cost: 431.000019
      main ctxt (stage 0)3.0, child ctxts:[], ..., cost: 431.000019
      main ctxt (stage 0)1.0, child ctxts:[], ..., cost: 431.000019
  2: CPhysicalMotionGather(coordinator) [ 1 ]
    Cost Ctxts:
      main ctxt (stage 0)0.0, child ctxts:[1], ..., cost: 431.000071
      main ctxt (stage 0)4.0, child ctxts:[1], ..., cost: 431.000071
  3: CPhysicalMotionBroadcast  [ 1 ]
    Cost Ctxts:
      main ctxt (stage 0)2.0, child ctxts:[1], rows:1.000000 (group), cost: 431.000241
  Grp OptCtxts:
    0 (stage 0): (req CTEs: [], ...) => Best Expr:2
    1 (stage 0): (req CTEs: [], ...) => Best Expr:1
    2 (stage 0): (req CTEs: [], ...) => Best Expr:3
    3 (stage 0): (req CTEs: [], ...) => Best Expr:1
    4 (stage 0): (req CTEs: [], ...) => Best Expr:2
    5 (stage 0): (req CTEs: [], ...) => Best Expr:1
    6 (stage 0): (req CTEs: [], ...) => Best Expr:1
```

Group 1 with group optimization context 3 tells us the best expression is 1.
```
Group 1 (#GExprs: 4):
  ...
  Grp OptCtxts:
    3 (stage 0): (req CTEs: [], ...) => Best Expr:1
```

In Group 1 with expression 1 we see a table scan:
```
Group 1 (#GExprs: 4):
  ...
  1: CPhysicalTableScan "t" ("t") [ ]
    Cost Ctxts:
      main ctxt (stage 0)6.0, child ctxts:[], ..., cost: 431.000019
      main ctxt (stage 0)5.0, child ctxts:[], ..., cost: 431.000019
      main ctxt (stage 0)3.0, child ctxts:[], ..., cost: 431.000019
      main ctxt (stage 0)1.0, child ctxts:[], ..., cost: 431.000019
  ...
```

Finally let's look at the scalar child of the inner hash join node (group 4 with no context)
```
Group 4 ():
  0: CScalarCmp (=) [ 2 3 ]
Group 3 ():
  0: CScalarIdent "a" (8) [ ]
Group 2 ():
  0: CScalarIdent "a" (0) [ ]
```

Here we see that the scalar is a compare on identifier "a" (8) and identifier
"a" (0). The 8 and 0 come from the CLogicalGet on the outer and inner children
of the inner hash join node. We already stitched these together to be group 0
and group 1 respectively.

```
Group 0 (#GExprs: 4):
  0: CLogicalGet "t" ("t"), Columns: ["a" (0), ...] Key sets: {[1,7]} [ ]
  ...

Group 1 (#GExprs: 4):
  0: CLogicalGet "t" ("t"), Columns: ["a" (8), ...] Key sets: {[1,7]} [ ]
  ...
```

At this point we now know the plan optimized based on the nodes traversed in
the memo:
```
CPhysicalMotionGather(coordinator)
+--CPhysicalInnerHashJoin
   |--CPhysicalTableScan "t" ("t")
   |--CPhysicalTableScan "t" ("t")
   +--CScalarCmp (=)
      |--CScalarIdent "a" (0)
      +--CScalarIdent "a" (8)
```
