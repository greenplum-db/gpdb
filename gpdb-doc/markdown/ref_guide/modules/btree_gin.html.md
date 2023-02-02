# btree\_gin 

The `btree_gin` module provides sample generalized inverted index \(GIN\) operator classes that implement B-tree equivalent behavior for certain data types.

The Greenplum Database `btree_gin` module is equivalent to the PostgreSQL `btree_gin` module. There are no Greenplum Database or MPP-specific considerations for the module.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `btree_gin` module is installed when you install Greenplum Database. Before you can use any of the functions defined in the module, you must register the `btree_gin` extension in each database in which you want to use the functions:

```
CREATE EXTENSION btree_gin;
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## <a id="topic_info"></a>Module Documentation 

See [btree\_gin](https://www.postgresql.org/docs/12/btree-gin.html) in the PostgreSQL documentation for detailed information about the individual functions in this module.

