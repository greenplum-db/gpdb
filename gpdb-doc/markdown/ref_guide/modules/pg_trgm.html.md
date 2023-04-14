# pg\_trgm 

The `pg_trgm` module provides functions and operators for determining the similarity of alphanumeric text based on trigram matching. The module also provides index operator classes that support fast searching for similar strings.

The Greenplum Database `pg_trgm` module is equivalent to the PostgreSQL `pg_trgm` module. There are no Greenplum Database or MPP-specific considerations for the module.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `pg_trgm` module is installed when you install Greenplum Database. Before you can use any of the functions defined in the module, you must register the `pg_trgm` extension in each database in which you want to use the functions:

```
CREATE EXTENSION pg_trgm;
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## <a id="topic_info"></a>Module Documentation 

See [pg\_trgm](https://www.postgresql.org/docs/12/pgtrgm.html) in the PostgreSQL documentation for detailed information about the individual functions in this module.

