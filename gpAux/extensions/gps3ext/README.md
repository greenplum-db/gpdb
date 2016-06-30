## Build

`make -B` to build the extension for GPDB.

`make -B gpcheckcloud` to build `gpcheckcloud`.

## Test

### Run Unit Tests

`make test`

### Test Code Coverage

`make coverage`

## Coding Style

Based on Google C++ style, especially:

- No tabs, 4 spaces only.
- CamelCase for classes, camelCase for methods and variables.
- Avoid pointers, use references as much as possible, unless returning NULL.
- Use exceptions properly, instead of checking returning values everywhere.
