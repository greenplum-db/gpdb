# postgresql\-hll

The `postgresql-hll` module provides native HyperLogLog data types and relation functions, operators, and aggregates.

The Greenplum Database `postgresql-hll` module is equivalent to version 2.16 of the `postgresql-hll` used with PostgreSQL. There are no Greenplum Database or MPP-specific considerations for the module.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `postgresql-hll` module is installed when you install Greenplum Database. Before you can use the data types defined in the module, you must register the `hll` extension in each database in which you want to use the types:

```
CREATE EXTENSION hll;
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## <a id="topic_info"></a>Module Documentation 

Refer to the [postgresql-hll github documentation](https://github.com/citusdata/postgresql-hll) for detailed information about using the module.

