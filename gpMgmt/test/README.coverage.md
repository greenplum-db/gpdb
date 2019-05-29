Here is a sample set of steps for using [`coverage.py`](https://coverage.readthedocs.io/en/latest/index.html)
for multi-process end-to-end testing (such as a behave run).  For development, the
intended use case is to use this tool to understand and then improve the code coverage 
for the files you are modifying.

1.  Install the `coverage` package:

        pip install coverage

    This package must be available to the Python installation running the
    utilities, so for a standard dev environment, `pip` should be fine for a local run.

2.  Set up `coverage` for [multi-process runs](https://coverage.readthedocs.io/en/latest/subprocess.html).
    There are many possible installation-dependent ways to do this, detailed in
    the link. I chose one of the hacky solutions and added a `sitecustomize.py`
    to my GPDB installation, because it was simple and it worked:

        $ cat > /usr/local/gpdb/lib/python/sitecustomize.py <<EOF
        import coverage
        coverage.process_startup()
        EOF

3.  Set up a [Coverage configuration file](https://coverage.readthedocs.io/en/latest/config.html)
    with your preferred options. See the `coveragerc` file in this directory.  Note that file
    excludes many non-development files(like testing source code files).  This file may be placed anywhere; 
    we'll tell Coverage where to find it in the next step.

4.  Enable coverage instrumentation with [the `COVERAGE_PROCESS_START` envvar](https://coverage.readthedocs.io/en/latest/subprocess.html)
    (which must point at the config file you created in the last step). The
    `coverage.process_startup()` call we added earlier is a no-op unless this
    envvar is provided.

        export COVERAGE_PROCESS_START=/<absolute_path_to_file>/<coverage configuration file>
        
        (e.g. export COVERAGE_PROCESS_START=~/workspace/gpdb/gpMgmt/test/coveragerc)

    This will instrument all Python subprocesses that are spawned, and write
    coverage data to the location specified in our config file.

5.  Now run whatever tests you want. For example, test the configuration by running an existing unit test:

        $ cd gpMgmt/bin
        $ python -m unittest gppylib.test.unit.test_unit_gpstop

6.  After you have run all the tests you want, [combine the data files](https://coverage.readthedocs.io/en/latest/cmd.html#combining-data-files)
    that were generated into a single `.coverage` file.  Note that you still have to `combine` the files if you only have a
    single coverage-data file; otherwise the `report` will fail.

        $ cd /tmp
        $ ls -a
        . .. coverage-data.mdw.10392.109492 coverage-data.mdw.10394.945371 coverage-data.mdw.10405.277583
        $ coverage combine coverage-data.*
        $ ls -a
        . .. .coverage

     You can combine files from multiple test runs as follows.  You generate the .coverage file for one run,
     and then run this step to merge the two results after the second run:

        $ coverage combine --append coverage-data.*
        
7.  Generate a report. [You have many options](https://coverage.readthedocs.io/en/latest/cmd.html#reporting);
    you can generate a stdout report and then a browseable html report.  Note you can click on each file to see
    the details of coverage for that file.

        $ coverage report
        $ coverage html -d /tmp/coverage-html
        $ open /tmp/coverage-html/index.html

    Your usage model might involve determining code coverage, adding tests, and then checking the resulting change
    in code coverage.  To do so, you can generate the `coverage report` for the before and after cases and diff those
    text files.  You can also use the browseable `coverage html` output to determine in detail changes to code coverage.

8.  WARNING: the steps described here will continue to instrument your python code until you unset the 
    environment variable, e.g.:

        $ unset COVERAGE_PROCESS_START


# Performance Impact
Read [how coverage works](https://coverage.readthedocs.io/en/latest/howitworks.html) to learn about the potential
performance impacts.  In short, each function is instrumented.  We ran `gpMgmt/bin> make unitdevel` and saw no performance
impact(the run time difference was less than 3 seconds for a 30 second runtime).  As you'd expect, if you construct a Python program
with a tight loop of a simple function call, you can get much worse performance.

    # Takes 45 seconds without coverage and 180 seconds with coverage(simply unset COVERAGE_PROCESS_START).
    class _Example:
        def __init__(self):
            pass

        def add_numbers(self, x, y):
            return x + y

    if __name__ == '__main__':
        for i in range(0,100000000):
            _Example().add_numbers(1,6)
            if i%10000000 == 0:
                print "."

In short, we'd expect the performance hit to be small for most test code, since that tends not to run in a tight loop.