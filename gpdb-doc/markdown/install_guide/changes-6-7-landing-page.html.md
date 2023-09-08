---
title: Important Changes Between Greenplum 6 and Greenplum 7
---

There are a substantial number of changes between Greenplum 6 and Greenplum 7 that could potentially affect your existing 6 application when you move to 7. This topic provides information about these changes. 


## <a id="naming_changes"></a>Naming Changes

The following table summarizes naming changes in Greenplum 7:

|Old Name|New Name|
|-----------|-----|
|master|coordinator|
|pg_log|log|
|pg_xlog|pg_wal|
|pg_clog|pg_xact|
|xlog (includes SQL functions, tools and options)|wal|
|pg_xloadump|pg_waldump|
|pg_resetxlog|pg_resetwal|


## <a id="deprecated"></a>Deprecated Features

The following features have been deprecated in Greenplum 7:

- The `gpreload` utility. Instead, use `ALTER TABLE...REPACK BY`.

## <a id="removed"></a>Removed Features

The following features have been removed in Greenplum 7:

- Support for Quicklz compression. To avoid breaking applications that use Quicklz, set the `gp_quicklz_fallback` server configuration parameter to `true`.

- The `--skip_root_stats` option  of the `analyzedb` utility.

- The Greenplum R Client.

- Greenplum MapReduce.

- The `ARRAY_NAME` variable.

- The PL/Container 3.0 Beta extension.

- The `gp_percentil_agg` extension.

- The `checkpoint_segments` parameter in the `postgresql.conf` file.  Use the server configuration parameters `min_wal_size` and ` max_wal_size`, instead.

- The `createlang` and `droplang` utilties. Instead, use `CREATE EXTENSION` and `DROP EXTENSION` directly.

- 

## <a id="behavior"></a>Changes in Feature Behavior

The following feature behaviors have changed in Greenplum 7:

- Pattern matching behavior of the `substring()` function has changed. In cases where the pattern can be matched in more than one way, the initial subpattern is now treated as matching the least possible amount of text rather than the greatest; for example, a pattern such as `%#“aa*#”%` now selects the first group of `a`’s from the input, rather than the last group.

- Multi-dimensional arrays can now be passed into PL/Python functions, and returned as nested Python lists -  Previously, you could return an array of composite values by writing, for exmaple, `[[col1, col2], [col1, col2]]`. Now, this is interpreted as a two-dimensional array. Composite types in arrays must now be written as Python tuples, not lists, to resolve the ambiguity; that is, you must write `[(col1, col2), (col1, col2)]`, instead.

- When `x` is a table name or composite column, PostgreSQL has traditionally considered the syntactic forms `f(x)` and `x.f` to be equivalent, allowing tricks such as writing a function and then using it as though it were a computed-on-demand column. However, if both interpretations are feasible, the column interpretation was always chosen. Now, if there is ambiguity, the interpretation that matches the syntactic form is chosen.

## <a id="linked"></a>Other Important Changes in Greenplum 7

Greenplum 7 also:

- Removes some objects that were in Greeplum 6. See [Removed Objects](../ref_guide/removed-objects.html).

- Changes some server configuration parameters. See [Changed Server Configuration Parameters](../ref_guide/guc-changes-6to7.html).

- Makes changes to how partitioning works. See [Changes in Partitioning](../admin_guide/ddl/about-part-changes.html).

- Makes changes to external tables. See [Changes to External Tables](../admin_guide/external/about_exttab_7.html.md).

- Makes changes to resource groups. See [Changes to Resource Groups](../admin_guide/about-resgroups-changes.html).

- Makes changes to system views and system tables. See [Changes to System Views and Tables](../ref_guide/system-changes-6to7.html).



