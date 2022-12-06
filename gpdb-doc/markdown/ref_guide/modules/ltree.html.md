# ltree

The `ltree` module implements a data type named `ltree` that you can use to represent labels of data stored in a hierarchical tree-like structure. The module also provides extensive facilities for searching through label trees.

The Greenplum Database `ltree` module is based on the `ltree` module used with PostgreSQL. The Greenplum version of the module differs as described in the [Greenplum Database Considerations](#topic_gp) topic.

## <a id="topic_reg"></a>Installing and Registering the Module

The `ltree` module is installed when you install Greenplum Database. Before you can use any of the data types, functions, or operators defined in the module, you must register the `ltree` extension in each database in which you want to use the objects:

```
CREATE EXTENSION ltree;
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## <a id="topic_info"></a>Module Documentation

Refer to the [ltree](https://www.postgresql.org/docs/9.4/ltree.html) PostgreSQL documentation for detailed information about the data types, functions, and operators defined in this module.

## <a id="topic_gp"></a>Greenplum Database Considerations

Because this extension does not provide a hash operator class, columns defined with the data type `ltree` can not be used as the distribution key for a Greenplum Database table.

