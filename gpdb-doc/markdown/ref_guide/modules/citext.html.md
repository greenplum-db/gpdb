# citext 

The citext module provides a case-insensitive character string data type, `citext`. Essentially, it internally calls the `lower()` function when comparing values. Otherwise, it behaves almost exactly like the `text` data type.

The Greenplum Database `citext` module is equivalent to the PostgreSQL `citext` module. There are no Greenplum Database or MPP-specific considerations for the module.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `citext` module is installed when you install Greenplum Database. Before you can use any of the data types, operators, or functions defined in the module, you must register the `citext` extension in each database in which you want to use the objects. Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## <a id="topic_info"></a>Module Documentation 

See [citext](https://www.postgresql.org/docs/12/citext.html) in the PostgreSQL documentation for detailed information about the data types, operators, and functions defined in this module.

