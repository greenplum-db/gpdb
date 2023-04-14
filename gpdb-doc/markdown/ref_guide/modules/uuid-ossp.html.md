# uuid-ossp

The `uuid-ossp` module provides functions to generate universally unique identifiers (UUIDs) using one of several standard algorithms. The module also includes functions to produce certain special UUID constants.

The Greenplum Database `uuid-ossp` module is equivalent to the PostgreSQL `uuid-ossp` module. There are no Greenplum Database or MPP-specific considerations for the module.

## <a id="topic_reg"></a>Installing and Registering the Module

The `uuid-ossp` module is installed when you install Greenplum Database. Before you can use any of the functions defined in the module, you must register the `uuid-ossp` extension in each database in which you want to use the functions:

```
CREATE EXTENSION "uuid-ossp";
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## <a id="topic_info"></a>Module Documentation

See the PostgreSQL [uuid-ossp](https://www.postgresql.org/docs/12/uuid-ossp.html) documentation for detailed information about this module.

