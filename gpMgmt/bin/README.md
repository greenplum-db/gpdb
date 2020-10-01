# For Developers

To install the libraries necessary for running scripts or testing, a system python of 2.7 must be available, the version of gcc and g++ used to compile python must be available.
On most distributions, python will compiled with the same gcc and g++ verion available from the base packages "gcc" and "gcc-c++".

The command `python -VV` will show the compiler used to compile the version of python being used.
A `make` in from gpMgmt will install the proper libraries provided a gcc and gcc-c++ are present.

To run any of these python scripts, necessary libraries must be installed, and PYTHONPATH must be modified to use the libraries in this path.

```
PYTHONPATH="\$GPHOME/lib/python:${PYTHONPATH}"
```

This will be set automatically with a `source $GPHOME/greenplum_path.sh`


## Python Version

System python 2.7 is currently required.


Where Things Go
---------------

* If you are adding a GP module, please add it to the gppylib dir.
* If you are adding a 3rd party module, please add it to the ext dir.

List of Management Scripts Written in Bash
------------------------------------------
bin/gpinitsystem        -  Creates a new Greenplum Database
bin/gpload              -  Sets env variables and calls gpload.py


List of Management Scripts Written in Python (no libraries)
-----------------------------------------------------------
bin/gpload.py           -  Loads data into a Greenplum Database


List of Management Scripts Written in Python (gpmlib - old libraries)
---------------------------------------------------------------------
bin/gpaddmirrors        -  Adds mirrors to an array (needs rewrite)
bin/gprecoverseg        -  Recovers a failed segment (needs rewrite)
bin/gpcheckperf         -  Checks the hardware for Greenplum Database
bin/gpscp               -  Copies files to many hosts
bin/gpssh               -  Remote shell to many hosts
bin/gpssh-exkeys        -  Exchange ssh keys between many hosts


List of Management Scripts Written in Python (gppylib - current libraries)
--------------------------------------------------------------------------
bin/gpactivatestandby   -  Activates the Standby Master
bin/gpconfig_helper     -  Edits postgresql.conf file for all segments
bin/gpdeletesystem      -  Deletes a Greenplum Database
bin/gpexpand            -  Adds additional segments to a Greenplum Database
bin/gpinitstandby       -  Initializes standby master
bin/gplogfilter         -  Filters log files
bin/gpstart             -  Start a Greenplum Database
bin/gpstop              -  Stop a Greenplum Database

sbin/gpconfig_helper.py -  Helper script for gpconfig
sbin/gpsegcopy          -  Helper script for gpexpand
sbin/gpsegstart.py      -  Helper script for gpstart
sbin/gpsegstop.py       -  Helper script for gpstop


Overview of gppylib
-------------------

dattimeutils.py  -  Several utility functions for dealing with date/time data

gparray.py
   |
   +-  Segment      - Configuration information for a single dbid
   |
   +-  SegmentPair - Configuration information for a single content id
   |     \-  Contains multiple Segment objects
   |
   +-  GpArray   - Configuration information for a Greenplum Database
         \-  Contains multiple SegmentPair objects

gplog.py         - Utility functions to assist in Greenplum standard logging

gpparseopts.py   - Wrapper around optparse library to aid in locating help files

gpsubprocess.py  - Wrapper around python subprocess (?) 
   \- Used by commands/base.py   
    - Should move to the commands submodule? 

logfilter.py     - Contains numerous odd utility functions mostly not specific to logfilter

pgconf.py        - Contains helper functions for reading postgresql.conf files
  |
  +- gucdict      - dictionary of guc->value pairs
  |    \- Contains setting objects
  |
  +- setting      - the setting of a single guc and some type coercion funcs
  |
  +- ConfigurationError - subclass of EnvironmentError, raised by type coercion functions

segcopy.py        - code for copying a segment from one location to another
    \- should be subclass of command ???

userinput.py      - wrapper functions around raw_input

commands/base.py  - Core of commands submodule  (could use some work)
  |
  +- WorkerPool    - Multithreading to execute multiple Command objects
  |     \- Spawns multiple Worker objects
  |
  +- Worker        - A single thread used to execute Command objects
  |
  +- CommandResult - Packages results of a Command object
  |
  +- ExecutionError - subclass of Exception
  |
  +- ExecutionContext - Abstract class
  |    |
  |    +- LocalExecutionContext - execute a command locally
  |    |
  |    +- RemoteExecutionContext - execute a command remotely
  |
  +- Command       - abstract class for executing (shell level) commands
  |
  +- SQLCommand    - abstract class for executing SQL commands

commands/gp.py     - Implements lots of subclasses of Command for various tasks
commands/pg.py     - Like gp.py, not clear what the separation is, if any.
commands/unix.py   - Platform information + more subclasses of Command
commands/test_pg   - some tests for commands/pg.py
  
db/catalog.py      - Wrappers for executing certain queries
   \- also contains some goofy wrappers for catalog tables
db/dbconn.py       - Connections to the database
  |
  +- ConnectionError - subclass of a StandardError (unused?)
  |
  +- Pgpass        - wrapper for handling a .pgpass file
  |
  +- DbURL         - descriptor of how to connect to a database
  |
  +- functions for returning a pygresql.connection object
  |
  +- Should have a wrapper class around a pygresql connection object!

util/gp_utils.py     - Greenplum related utility functions that are not Commands
util/ssh_session.py  - SSH and SCP related utility functions brought in from gpmlib.py/gplib.py
                       that are used by gpssh, gpscp and gpssh-exkeys


## Testing Management Scripts (unit tests)

This directory contains the unit tests for the management scripts. These tests
require the following Python modules to be installed: mock and pygresql.
These modules can be installed by running "git submodule update --init --recursive"
if they are not already installed on your machine.

If you installed the dependencies using the above git command, you can run the tests with
make, using the following commands in the current directory:

"make check" will run all of the unit tests, some of which require a GPDB cluster to
be installed and currently running.

"make unitdevel" will run only the unit tests that do not require a running cluster.


If you did not install the dependencies using the git submodule, use the following commands in
place of the above make commands, still in the current directory:

"python -m unittest discover --verbose -s gppylib -p 'test_unit*.py' -p 'test_cluster*.py'" will
run all of the unit tests.

"python -m unittest discover --verbose -s gppylib -p 'test_unit*.py'" will run only the unit
tests that do not require a running cluster.

## Testing Management Scripts (behave tests)

Behave tests require a running Greenplum cluster, and additional python libraries for testing, available to gpadmin.

Thus, you can install these additional python libraries using any of the following methods:

1. As root user, to be available globally:

```
sudo pip install -r gpMgmt/requirements-dev.txt
```

2. As gpadmin user, to be available only to gpadmin user, but overriding any overlapping libraries with the specific verions in this requirements file:

```
pip install --user -r gpMgmt/requirements-dev.txt
```

3. As gpadmin, using a virtual env - see additional documentation on using a virtual env on python.org
