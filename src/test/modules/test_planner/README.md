# test_planner extension

`test_planner` is for creating expectations about the planner from the perspective
of downstream components (for example: the executor).

It defines an extension that can be installed into a running instance. The extension
defines a function that runs a suite of expectations about the planner.

## Run tests

### Run the regress suite

`make installcheck`

### During Development

During development, you can get feedback on success and failure

`make test`

Also, the extension can be installed and run as a query:

`make clean install`

```
create extension test_planner;
select test_planner();
```

