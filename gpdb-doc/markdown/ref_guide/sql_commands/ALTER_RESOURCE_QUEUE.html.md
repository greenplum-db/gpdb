# ALTER RESOURCE QUEUE 

Changes the limits of a resource queue.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER RESOURCE QUEUE <name> WITH ( <queue_attribute>=<value> [, ... ] ) 
```

where queue\_attribute is:

```
   ACTIVE_STATEMENTS=<integer>
   MEMORY_LIMIT='<memory_units>'
   MAX_COST=<float>
   COST_OVERCOMMIT={TRUE|FALSE}
   MIN_COST=<float>
   PRIORITY={MIN|LOW|MEDIUM|HIGH|MAX}
```

```
ALTER RESOURCE QUEUE <name> WITHOUT ( <queue_attribute> [, ... ] )
```

where queue\_attribute is:

```
   ACTIVE_STATEMENTS
   MEMORY_LIMIT
   MAX_COST
   COST_OVERCOMMIT
   MIN_COST
```

> **Note** A resource queue must have either an `ACTIVE_STATEMENTS` or a `MAX_COST` value. Do not remove both these `queue_attributes` from a resource queue.

## <a id="section3"></a>Description 

`ALTER RESOURCE QUEUE` changes the limits of a resource queue. Only a superuser can alter a resource queue. A resource queue must have either an `ACTIVE_STATEMENTS` or a `MAX_COST` value \(or it can have both\). You can also set or reset priority for a resource queue to control the relative share of available CPU resources used by queries associated with the queue, or memory limit of a resource queue to control the amount of memory that all queries submitted through the queue can consume on a segment host.

`ALTER RESOURCE QUEUE WITHOUT` removes the specified limits on a resource that were previously set. A resource queue must have either an `ACTIVE_STATEMENTS` or a `MAX_COST` value. Do not remove both these `queue_attributes` from a resource queue.

## <a id="section4"></a>Parameters 

name
:   The name of the resource queue whose limits are to be altered.

ACTIVE\_STATEMENTS integer
:   The number of active statements submitted from users in this resource queue allowed on the system at any one time. The value for `ACTIVE_STATEMENTS` should be an integer greater than 0. To reset `ACTIVE_STATEMENTS` to have no limit, enter a value of `-1`.

MEMORY\_LIMIT 'memory\_units'
:   Sets the total memory quota for all statements submitted from users in this resource queue. Memory units can be specified in kB, MB or GB. The minimum memory quota for a resource queue is 10MB. There is no maximum; however the upper boundary at query execution time is limited by the physical memory of a segment host. The default value is no limit \(`-1`\).

MAX\_COST float
:   The total query optimizer cost of statements submitted from users in this resource queue allowed on the system at any one time. The value for `MAX_COST` is specified as a floating point number \(for example 100.0\) or can also be specified as an exponent \(for example 1e+2\). To reset `MAX_COST` to have no limit, enter a value of `-1.0`.

COST\_OVERCOMMIT boolean
:   If a resource queue is limited based on query cost, then the administrator can allow cost overcommit \(`COST_OVERCOMMIT=TRUE`, the default\). This means that a query that exceeds the allowed cost threshold will be allowed to run but only when the system is idle. If `COST_OVERCOMMIT=FALSE` is specified, queries that exceed the cost limit will always be rejected and never allowed to run.

MIN\_COST float
:   Queries with a cost under this limit will not be queued and run immediately. Cost is measured in units of disk page fetches; 1.0 equals one sequential disk page read. The value for `MIN_COST` is specified as a floating point number \(for example 100.0\) or can also be specified as an exponent \(for example 1e+2\). To reset `MIN_COST` to have no limit, enter a value of `-1.0`.

PRIORITY=\{MIN\|LOW\|MEDIUM\|HIGH\|MAX\}
:   Sets the priority of queries associated with a resource queue. Queries or statements in queues with higher priority levels will receive a larger share of available CPU resources in case of contention. Queries in low-priority queues may be delayed while higher priority queries are run.

## <a id="section5"></a>Notes 

GPORCA and the Postgres planner utilize different query costing models and may compute different costs for the same query. The Greenplum Database resource queue resource management scheme neither differentiates nor aligns costs between GPORCA and the Postgres Planner; it uses the literal cost value returned from the optimizer to throttle queries.

When resource queue-based resource management is active, use the `MEMORY_LIMIT` and `ACTIVE_STATEMENTS` limits for resource queues rather than configuring cost-based limits. Even when using GPORCA, Greenplum Database may fall back to using the Postgres Planner for certain queries, so using cost-based limits can lead to unexpected results.

## <a id="section6"></a>Examples 

Change the active query limit for a resource queue:

```
ALTER RESOURCE QUEUE myqueue WITH (ACTIVE_STATEMENTS=20);
```

Change the memory limit for a resource queue:

```
ALTER RESOURCE QUEUE myqueue WITH (MEMORY_LIMIT='2GB');
```

Reset the maximum and minimum query cost limit for a resource queue to no limit:

```
ALTER RESOURCE QUEUE myqueue WITH (MAX_COST=-1.0, 
  MIN_COST= -1.0);
```

Reset the query cost limit for a resource queue to 310 \(or 30000000000.0\) and do not allow overcommit:

```
ALTER RESOURCE QUEUE myqueue WITH (MAX_COST=3e+10, 
  COST_OVERCOMMIT=FALSE);
```

Reset the priority of queries associated with a resource queue to the minimum level:

```
ALTER RESOURCE QUEUE myqueue WITH (PRIORITY=MIN);
```

Remove the `MAX_COST` and `MEMORY_LIMIT` limits from a resource queue:

```
ALTER RESOURCE QUEUE myqueue WITHOUT (MAX_COST, MEMORY_LIMIT);
```

## <a id="section7"></a>Compatibility 

The `ALTER RESOURCE QUEUE` statement is a Greenplum Database extension. This command does not exist in standard PostgreSQL.

## <a id="section8"></a>See Also 

[CREATE RESOURCE QUEUE](CREATE_RESOURCE_QUEUE.html), [DROP RESOURCE QUEUE](DROP_RESOURCE_QUEUE.html), [CREATE ROLE](CREATE_ROLE.html), [ALTER ROLE](ALTER_ROLE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

