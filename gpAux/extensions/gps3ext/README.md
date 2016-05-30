## Coding Style

Major style will be C++ style:
	- No tab, 4 spaces only.
	- CamelCase for class, camelCase for methods and variables.
	- Avoid pointers, use references as much as possible, unless need to return NULL.
	- Use exception properly, instead of check result everywhere.

## Build

`make -B` to build the extension for GPDB.

`make -B -f Makefile.check` to build `gpcheckcloud`.

## Test

### Run Unit Tests

`make -f Makefile.others test`

### Test Code Coverage

`make -f Makefile.others coverage`
