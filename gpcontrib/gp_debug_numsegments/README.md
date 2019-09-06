By default all tables are created on all the segments, with this extension we
could check or change the default behavior.

How does it work?  Each table has a property numsegments to record how many
segments its data is distributed one, the value is decided on `CREATE TABLE`
according to an internal variable `gp_create_table_default_numsegments`, its
value can be an integer number between 1 (the minimal segment count) and
`getgpsegmentCount()` (all the primary segments in the cluster), or one of
below magic numbsers (policies):

- `GP_DEFAULT_NUMSEGMENTS_FULL`: all the segments;
- `GP_DEFAULT_NUMSEGMENTS_MINIMAL`: the minimal set of segments;
- `GP_DEFAULT_NUMSEGMENTS_RANDOM`: pick a random set of segments each time;

This extension provides functions to get, set and reset this internal
variable:

- `gp_debug_set_create_table_default_numsegments(integer)`:
  set the default numsegments to an integer number between 1 and
  `gp_num_contents_in_cluster`;

- `gp_debug_set_create_table_default_numsegments(text)`:
  set the default numsegments to one of the policies: `FULL`, `MINIMAL` and
  `RANDOM`;

- `gp_debug_reset_create_table_default_numsegments(integer)` and
  `gp_debug_reset_create_table_default_numsegments(text)`:
  reset the default numsegments to the specified number or policy, the value
  can be reused later;

- `gp_debug_reset_create_table_default_numsegments()`:
  reset the default numsegments to the last value passed in above two forms,
  if there is no previous call to it the value is `FULL`;

- `gp_debug_get_create_table_default_numsegments()`:
  get the current number or policy of the default numsegments;

The changes are effective within the session.

As the name suggests, this extension should only be used for debugging
purpose.
