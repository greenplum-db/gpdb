---
title: pgsphere 
---

The phsphere module provides spherical data types, functions, operators, and indexing for PostgreSQL or Greenplum database.

## <a id="topic_reg"></a>Installing and Registering the Module

The `pgsphere` module is installed when you install Greenplum Database. Before you can use it, you must register the `pgsphere` extension in each database in which you want to use these:

```
CREATE EXTENSION pgsphere;
```

Refer to [Installing Additional Supplied Modules](../../../install_guide/install_modules.html) for more information.

## <a id="topic_upgrading"></a>Upgrading the Module

The `pgsphere` module is installed when you install or upgrade Greenplum Database. A previous version of the extension will continue to work in existing databases after you upgrade Greenplum. To upgrade to the most recent version of the extension, you must:

```
ALTER EXTENSION pgsphere UPDATE TO '1.5.0';
```

in **every** database in which you registered/use the extension.

## <a id="topic_docs"></a>Module Documentation

Refer to the [pgsphere github documentation](https://github.com/postgrespro/pgsphere/blob/master/README.pg_sphere) for detailed information about using the module.
