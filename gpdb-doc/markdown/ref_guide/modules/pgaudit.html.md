---
title: pgaudit
---

The PostgreSQL Audit Extension, or `pgaudit`, provides detailed session and object audit logging via the standard logging facility provided by PostgreSQL. The goal of PostgreSQL Audit is to provide the tools needed to produce audit logs required to pass certain government, financial, or ISO certification audits.

## <a id="topic_reg"></a>Installing and Registering the Module

The `pgaudit` module is installed when you install Greenplum Database. To use it, enable the extension as a preloaded library and restart Greenplum Database.

First, check if there are any preloaded shared libraries by running the following command:

```
gpconfig -s shared_preload_libraries
```

Use the output of the above command to enable the `pgaudit` module, along any other shared libraries, and restart Greenplum Database:

```
gpconfig -c shared_preload_libraries -v '<other_libraries>,pgaudit'
gpstop -ar 
```

## <a id="topic_info"></a>Module Documentation

Refer to the [pgaudit github documentation](https://github.com/pgaudit/pgaudit/blob/REL_12_STABLE/README.md) for detailed information about using the module.

