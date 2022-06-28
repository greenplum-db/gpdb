# Cost-Based Vacuum Delay Parameters 

**Warning:** Using cost-based vacuum delay is discouraged because it runs asynchronously among the segment instances. The vacuum cost limit and delay is invoked at the segment level without taking into account the state of the entire Greenplum array.

You can configure the execution cost of `VACUUM` and `ANALYZE` commands to reduce the I/O impact on concurrent database activity. When the accumulated cost of I/O operations reaches the limit, the process performing the operation sleeps for a while, then resets the counter and continues execution.

- `vacuum_cost_delay`

- `vacuum_cost_limit`

- `vacuum_cost_page_dirty`

- `vacuum_cost_page_hit`

- `vacuum_cost_page_miss`<br/></br>

**Parent topic:** [System Resource Consumption Parameters](../topics/g-system-resource-consumption-parameters.html)

