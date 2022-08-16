---
title: Creating and Managing Views 
---

Views enable you to save frequently used or complex queries, then access them in a `SELECT` statement as if they were a table. A view is not physically materialized on disk: the query runs as a subquery when you access the view.

These topics describe various aspects of creating and managing views:

-   [Best Practices when Creating Views](ddl-view-best-practices.html) outlines best practices when creating views.
-   [Working with View Dependencies](ddl-view-find-depend.html) contains examples of listing view information and determining what views depend on a certain object.
-   [About View Storage in Greenplum Database](ddl-view-storage.html) describes the mechanics behind view dependencies.

## <a id="topic101"></a>Creating Views 

The `CREATE VIEW`command defines a view of a query. For example:

```
CREATE VIEW comedies AS SELECT * FROM films WHERE kind = 'comedy';

```

Views ignore `ORDER BY` and `SORT` operations stored in the view.

## <a id="topic102"></a>Dropping Views 

The `DROP VIEW` command removes a view. For example:

```
DROP VIEW topten;

```

The `DROP VIEW...CASCADE` command also removes all dependent objects. As an example, if another view depends on the view which is about to be dropped, the other view will be dropped as well. Without the `CASCADE` option, the `DROP VIEW` command will fail.

