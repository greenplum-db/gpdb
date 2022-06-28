# Legacy Query Optimizer Costing Parameters 

**Warning:** Do not adjust these query costing parameters. They are tuned to reflect Greenplum Database hardware configurations and typical workloads. All of these parameters are related. Changing one without changing the others can have adverse effects on performance.

- `cpu_index_tuple_cost`

- `cpu_operator_cost`

- `cpu_tuple_cost`

- `cursor_tuple_fraction`

- `effective_cache_size`

- `gp_motion_cost_per_row`

- `gp_segments_for_planner`

- `random_page_cost`

- `seq_page_cost`<br/></br>


**Parent topic:** [Query Tuning Parameters](../topics/g-query-tuning-parameters.html)

