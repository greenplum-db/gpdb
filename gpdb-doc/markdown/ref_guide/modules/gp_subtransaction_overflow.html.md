# gp_subtransaction_overflow

The `gp_subtransaction_overflow` module implements a Greenplum Database view and user-defined function for querying for backends experiencing 
subtransaction overflow; these are backends that have created more than 64 subtransactions, resulting in a high lookup cost for visibility checks.

The `gp_subtransaction_overflow` module is a Greenplum Database extension.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `gp_subtransaction_overflow` module is installed when you install Greenplum Database. Before you can use the view and user-defined function defined in the module, you must register the `gp_subtransaction_overflow` extension in each database where you want to use the function, using the following command:

```
CREATE EXTENSION gp_subtransaction_overflow;
```

For more information on how to use this module, see [Monitoring a Greenplum System](../../admin_guide/managing/monitor.html#checking-for-and-terminating-overflowed-backends).