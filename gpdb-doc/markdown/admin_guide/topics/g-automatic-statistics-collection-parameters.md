# Automatic Statistics Collection Parameters 

When automatic statistics collection is enabled, you can run `ANALYZE` automatically in the same transaction as an `INSERT`, `UPDATE`, `DELETE`, `COPY` or `CREATE TABLE...AS SELECT` statement when a certain threshold of rows is affected \(`on_change`\), or when a newly generated table has no statistics \(`on_no_stats`\). To enable this feature, set the following server configuration parameters in your Greenplum master `postgresql.conf` file and restart Greenplum Database:

-   `gp_autostats_mode`
-   `gp_autostats_mode_in_functions`
-   `log_autostats`
-   `gp_autostats_on_change_threshold`

**Warning:** Depending on the specific nature of your database operations, automatic statistics collection can have a negative performance impact. Carefully evaluate whether the default setting of `on_no_stats` is appropriate for your system.

**Parent topic:** [Configuration Parameter Categories](../topics/g-configuration-parameter-categories.html)

