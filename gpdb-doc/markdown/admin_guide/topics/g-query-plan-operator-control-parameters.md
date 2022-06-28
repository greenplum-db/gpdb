# Query Plan Operator Control Parameters 

The following parameters control the types of plan operations the legacy query optimizer can use. Enable or disable plan operations to force the legacy query optimizer to choose a different plan. This is useful for testing and comparing query performance using different plan types.

- `enable_bitmapscan`

- `enable_groupagg`

- b`enable_hashagg`

- `enable_hashjoin`

- `enable_indexscan`

- `enable_mergejoin`

- `enable_nestloop`

- `enable_seqscan`

- `enable_sort`

- `enable_tidscan`

- `gp_enable_agg_distinct`

- `gp_enable_agg_distinct_pruning`

- `gp_enable_direct_dispatch`

- `gp_enable_fallback_plan`

- `gp_enable_fast_sri`

- `gp_enable_groupext_distinct_ gather`

- `gp_enable_groupext_distinct_ pruning`

- `gp_enable_multiphase_agg`

- `gp_enable_predicate_ propagation`

- `gp_enable_preunique`

- `gp_enable_sequential_window_ plans`

- `gp_enable_sort_distinct`

- `gp_enable_sort_limit`<br/></br>


**Parent topic:** [Query Tuning Parameters](../topics/g-query-tuning-parameters.html)

