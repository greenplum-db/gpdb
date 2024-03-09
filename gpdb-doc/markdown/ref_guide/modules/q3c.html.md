---
title: q3c 
---

The Q3C (Quad Three Cube) module enables fast cone, ellipse and polygonal searches and cross-matches between large astronomical catalogs inside a PostgreSQL or Greenplum database.

## <a id="topic_reg"></a>Installing and Registering the Module

The `q3c` module is installed when you install Greenplum Database. Before you can use it, you must register the `q3c` extension in each database in which you want to use these:

```
CREATE EXTENSION q3c;
```

Refer to [Installing Additional Supplied Modules](../../../install_guide/install_modules.html) for more information.

## <a id="topic_upgrading"></a>Upgrading the Module

The `q3c` module is installed when you install or upgrade Greenplum Database. A previous version of the extension will continue to work in existing databases after you upgrade Greenplum. To upgrade to the most recent version of the extension, you must:

```
ALTER EXTENSION q3c UPDATE TO '2.0.1';
```

in **every** database in which you registered/use the extension.

## <a id="topic_docs"></a>Module Documentation

Refer to the [q3c github documentation](https://github.com/segasai/q3c/blob/master/README.md) for detailed information about using the module.
