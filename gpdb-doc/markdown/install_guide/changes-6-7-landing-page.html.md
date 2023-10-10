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
|pg_xlogdump|pg_waldump|
|pg_resetxlog|pg_resetwal|


## <a id="deprecated"></a>Deprecated Features

The following features have been deprecated in Greenplum 7:

- The `gpreload` utility. Instead, use `ALTER TABLE...REPACK BY`.

## <a id="behavior"></a>Changes in Feature Behavior

The following feature behaviors have changed in Greenplum 7:

- The `checkpoint_segments` parameter in the `postgresql.conf` file has been removed.  Use the server configuration parameters `min_wal_size` and ` max_wal_size`, instead.

- Autovacuum is now enabled by default for all databases. 

- Pattern matching behavior of the `substring()` function has changed. In cases where the pattern can be matched in more than one way, the initial subpattern is now treated as matching the least possible amount of text rather than the greatest; for example, a pattern such as `%#“aa*#”%` now selects the first group of `a`’s from the input, rather than the last group.

- Multi-dimensional arrays can now be passed into PL/Python functions, and returned as nested Python lists -  Previously, you could return an array of composite values by writing, for exmaple, `[[col1, col2], [col1, col2]]`. Now, this is interpreted as a two-dimensional array. Composite types in arrays must now be written as Python tuples, not lists, to resolve the ambiguity; that is, you must write `[(col1, col2), (col1, col2)]`, instead.

- When `x` is a table name or composite column, PostgreSQL has traditionally considered the syntactic forms `f(x)` and `x.f` to be equivalent, allowing tricks such as writing a function and then using it as though it were a computed-on-demand column. However, if both interpretations are feasible, the column interpretation was always chosen. Now, if there is ambiguity, the interpretation that matches the syntactic form is chosen.

- Greenplum 7 now prevents the `to_number()` function from consuming characters when the template separator does not match​. For example, `SELECT to_number(‘1234’, ‘9,999’)` used to return `134`. It will now return `1234`, instead. L and TH now only consume characters that are not digits, positive/negative signs, decimal points, or commas.​

- The `fix to_date()`, `to_number()`, and `to_timestamp()` functions previously skipped one byte for each byte of template character, resulting in strange behavior if either string contained multibyte characters.​ Adjust the handling of backslashes inside double-quotes in template strings for to_char(), to_number(), and to_timestamp().​ Such a backslash now escapes the character after it, particularly a double-quote or another backslash.

## <a id="linked"></a>Other Important Changes in Greenplum 7

Greenplum 7 also:

- Removes some features and objects that were in Greeplum 6. See [Features and Objects Removed in Greenplum 7](../ref_guide/removed-features-objects.html).

- Changes some server configuration parameters. See [Server Configuration Parameter Changes from Greenplum 6 to Greenplum 7](../ref_guide/guc-changes-6to7.html).

- Changes the partitioning system catalogs. See [Changes in Partitioning](../admin_guide/ddl/about-part-changes.html) and [Migrating Partition Maintenance Scripts to the New Greenplum 7 Partitioning Catalogs](migrate-classic-partitioning.html).

- Makes changes to external tables. See [Changes to External Tables](../admin_guide/external/about_exttab_7.html.md).

- Makes changes to resource groups. See [Changes to Resource Groups](../admin_guide/about-resgroups-changes.html).

- Makes changes to system views and system tables. See [System Catalog Changes between Greenplum 6 and Greenplum 7](../ref_guide/system-changes-6to7.html).