# ip4r 

The `ip4r` module provides IPv4 and IPv6 data types, IPv4 and IPv6 range index data types, and related functions and operators.

The Greenplum Database `ip4r` module is equivalent to version 2.4.1 of the `ip4r` module used with PostgreSQL. There are no Greenplum Database or MPP-specific considerations for the module.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `ip4r` module is installed when you install Greenplum Database. Before you can use any of the data types defined in the module, you must register the `ip4r` extension in each database in which you want to use the types:

```
CREATE EXTENSION ip4r;
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## <a id="topic_info"></a>Module Documentation 

Refer to the [ip4r github documentation](https://github.com/RhodiumToad/ip4r) for detailed information about using the module.

