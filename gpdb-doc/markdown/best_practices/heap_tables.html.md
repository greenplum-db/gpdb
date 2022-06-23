---
title: Identifying and Mitigating Heap Table Performance Issues
---

## <a id="bulkloadslow"></a>Slow or Hanging Jobs

**Symptom**:

Bulk data load or modification jobs on heap tables are running slow or hanging.

**Potential Cause**:

When a workload involves a bulk load or modification of data in a heap table, the first `SELECT` queries post-operation may generate a large amount of WAL data when both checksums are enabled and hint bits are logged, leading to slow or hung jobs.

Affected workloads include: restoring from a backup, loading data with `gpcopy` or `COPY`, cluster expansion, `CTAS`/`INSERT`/`UPDATE`/`DELETE` operations, and `ALTER TABLE` operations that modify tuples.

**Explanation**:

Greenplum Database uses hint bits to mark tuples as created and/or deleted by transactions. Hint bits are checked and set on a per-tuple basis and can result in heavy writes to a database table even when you are just reading from it. When data checksums are enabled for heap tables (the default), hint bit updates are always WAL-logged.

**Solution**:

If you have restored or loaded a complete database, you may choose to run `VACUUM` against the entire database.

Alternatively, if you can identify the individual tables affected, you have two options:

1. Schedule and take a maintenance window and run `VACUUM` and `ANALYZE` on the specific tables that have been loaded or updated. These operations should scan all of the tuples and set and WAL-log the hint bits, taking the performance hit up-front.

2. Run `SELECT count(*) FROM <table-name>` on each table. This operation similarly scans all of the tuples and sets and WAL-logs the hint bits.

All subsequent `SELECT` operations as part of regular workloads on the tables should not be required to generate hints or their accompanying full page image WAL records.

