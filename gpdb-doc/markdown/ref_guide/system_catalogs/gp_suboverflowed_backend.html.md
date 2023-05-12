# gp_suboverflowed_backend 

The `gp_suboverflowed_backend` view allows administrators to identify sessions in which a backend has subtransaction overflows, 
which can cause query performance degradation in the system, including catalog queries.

|column|type|description|
|------|----|----------|
|`segid`|integer|The id of the segment containing the suboverflowed backend.|
|`pids`|integer[]|A list of the pids of all suboverflowed backends on this segment.|

> **Note** As this is not a `pg_catalog` view, you must run the following command to make this view available:

```
CREATE EXTENSION gp_subtransaction_overflow;
```

For more information on handling suboverflowed backends to prevent performance issues, see [Checking for and Terminating Overflowed Backends](../../admin_guide/managing/monitor.html#overflowed_backends).

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

