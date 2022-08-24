# gp_pitr

The `gp_pitr` module supports implementing Point-in-Time Recovery for Greenplum Database 6. In service of this it creates a new view -- `gp_stat_archiver` -- as well as two user-defined functions that are called internally.

The `gp_pitr` module is a Greenplum Database extension.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `gp_pitr` module is installed when you install Greenplum Database. Before you can use the view defined in the module, you must register the `gp_pitr` extension in each database where you want to use the function, using the following command:

```
CREATE EXTENSION gp_pitr;
```