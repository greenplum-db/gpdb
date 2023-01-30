---
title: About View Storage in Greenplum Database 
---

A view is similar to a table, both are relations - that is "something with columns". All such objects are stored in the catalog table [`pg_class`](../../ref_guide/system_catalogs/pg_class.html). These are the general differences:

-   A view has no data files \(because it holds no data\).
-   The value of `pg_class.relkind` for a view is `v` rather than `r`.
-   A view has an `ON SELECT` query rewrite rule called `_RETURN`.

    The rewrite rule contains the definition of the view and is stored in the `ev_action` column of the [`pg_rewrite`](../../ref_guide/system_catalogs/pg_rewrite.html) catalog table.


For more technical information about views, see the PostgreSQL documentation about [Views and the Rule System](https://www.postgresql.org/docs/12/rules-views.html).

Also, a view definition is *not* stored as a string, but in the form of a query parse tree. Views are parsed when they are created, which has several consequences:

-   Object names are resolved during `CREATE VIEW`, so the current setting of `search_path` affects the view definition.
-   Objects are referred to by their internal immutable object ID rather than by their name. Consequently, renaming an object or column referenced in a view definition can be performed without dropping the view.
-   Greenplum Database can determine exactly which objects are used in the view definition, so it can add dependencies on them.

Note that the way Greenplum Database handles views is quite different from the way Greenplum Database handles functions: function bodies are stored as strings and are not parsed when they are created. Consequently, Greenplum Database does not know on which objects a given function depends.

## <a id="whereview"></a>Where View Dependency Information is Stored 

These system catalog tables contain the information used to determine the tables on which a view depends.

-   [`pg_class`](../../ref_guide/system_catalogs/pg_class.html) - object information including tables and views. The `relkind` column describes the type of object.
-   [`pg_depend`](../../ref_guide/system_catalogs/pg_depend.html) - object dependency information for database-specific \(non-shared\) objects.
-   [`pg_rewrite`](../../ref_guide/system_catalogs/pg_rewrite.html) - rewrite rules for tables and views.
-   [`pg_attribute`](../../ref_guide/system_catalogs/pg_attribute.html) - information about table columns.
-   [`pg_namespace`](../../ref_guide/system_catalogs/pg_namespace.html) - information about schemas \(namespaces\).

It is important to note that there is no direct dependency of a view on the objects it uses: the dependent object is actually the view's rewrite rule. That adds another layer of indirection to view dependency information.

