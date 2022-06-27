# Disabling EXECUTE for Web or Writable External Tables 

There is a security risk associated with allowing external tables to execute OS commands or scripts. To disable the use of `EXECUTE` in web and writable external table definitions, set the `gp_external_enable_exec server` configuration parameter to off in your master postgresql.conf file:

```
gp_external_enable_exec = off

```

**Parent topic:** [Defining a Command-Based Writable External Web Table](../../load/topics/g-defining-a-command-based-writable-external-web-table.html)

