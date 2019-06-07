This directory contains a custom formatter, for reading and writing
fixed-width data files.

The regression tests here are all on readable external tables, because
writable external tables require a running gpfdist server. There
are tests on writable external tables, with this fixedwidth formatter, in
the gpfdist regression suite, however.
