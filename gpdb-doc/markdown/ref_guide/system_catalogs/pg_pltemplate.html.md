# pg_pltemplate 

The `pg_pltemplate` system catalog table stores template information for procedural languages. A template for a language allows the language to be created in a particular database by a simple `CREATE LANGUAGE` command, with no need to specify implementation details. Unlike most system catalogs, `pg_pltemplate` is shared across all databases of Greenplum system: there is only one copy of `pg_pltemplate` per system, not one per database. This allows the information to be accessible in each database as it is needed.

There are not currently any commands that manipulate procedural language templates; to change the built-in information, a superuser must modify the table using ordinary `INSERT`, `DELETE`, or `UPDATE` commands.

|column|type|references|description|
|------|----|----------|-----------|
|`tmplname`|name| |Name of the language this template is for|
|`tmpltrusted`|boolean| |True if language is considered trusted|
|`tmpldbacreate`|boolean| |True if language may be created by a database owner|
|`tmplhandler`|text| |Name of call handler function|
|`tmplinline`|text| |Name of anonymous-block handler function, or null if none|
|`tmplvalidator`|text| |Name of validator function, or NULL if none|
|`tmpllibrary`|text| |Path of shared library that implements language|
|`tmplacl`|aclitem\[\]| |Access privileges for template \(not yet implemented\).|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

