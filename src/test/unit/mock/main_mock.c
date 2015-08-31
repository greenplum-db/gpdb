/*
 * This is a mock version of src/backend/main/main.c. In a unit test, the test
 * program contains the real main() function, so we don't want to link the
 * postgres backend's main() function into the test program. (Alternatively,
 * we could use ld's --wrap option and call the test program's main()
 * __wrap_main(), but this seems nicer.)
 */

/* The only thing we need from main.c is this global variable */
const char *progname;
