#!/usr/bin/env python
#
#
from gppylib.testold.testDriver import TestDriver
from gppylib.programs.clsSystemState import *
from gppylib.mainUtils import *
from gppylib.testold.testUtils import *
from gppylib.system import fileSystemImplTest, fileSystemInterface
from gppylib.gplog import get_default_logger

programName = sys.argv[0]
parserFn = GpSystemStateProgram.createParser
commandFn = GpSystemStateProgram.createProgram

driver = TestDriver()
logger = get_default_logger()

###############
#
# Now the tests:
#
####################################
#
#
def testGuessSpreadAndMultiHome(args):

    configStr = args[0]

    driver.setSegments(configStr)
    gpArray = configInterface.getConfigurationProvider().initializeProvider(5432).loadSystemConfig(useUtilityMode=False)

    testOutput("")
    testOutput("Mirroring: %s" % ("Spread" if gpArray.guessIsSpreadMirror() else "Group"))
    testOutput("Multi Home: %s" % ("Multi" if gpArray.guessIsMultiHome() else "NOT Multi"))
    testOutput("")

#
# This is a non-multi-home, group mirror
#
simple_test("testSpreadMultiGuess_1", testGuessSpreadAndMultiHome, [
"""
 dbid | content | role | preferred_role | mode | status |      hostname      |      address       | port  |                     datadir         | replication_port
------+---------+------+----------------+------+--------+--------------------+--------------------+-------+-------------------------------------+------------------
    1 |      -1 | p    | p              | s    | u      | master-host        | primary-host       |  5432 |/datadirpathdbmaster/gp-1            |
    2 |       0 | p    | p              | s    | u      | first-host         | first-host         | 50001 |/datadirpathdbfast1/gp0              |       55001
    7 |       0 | m    | m              | s    | u      | second-host        | second-host        | 40001 |/second/datadirpathdbfast3/gp0       |       45001
    3 |       1 | p    | p              | s    | u      | first-host         | first-host         | 50002 |/datadirpathdbfast2/gp1              |       55002
    9 |       1 | m    | m              | s    | u      | second-host        | second-host        | 40002 |/second/datadirpathdbfast4/gp1       |       45002
    4 |       2 | m    | m              | s    | u      | first-host         | first-host         | 60001 |/datadirpathdbfast3/gp0              |       65001
    6 |       2 | p    | p              | s    | u      | second-host        | second-host        | 30001 |/second/datadirpathdbfast1/gp0       |       35001
    5 |       3 | m    | m              | c    | d      | first-host         | first-host         | 60002 |/datadirpathdbfast4/gp1              |       65002
    8 |       3 | p    | p              | c    | u      | second-host        | second-host        | 30002 |/second/datadirpathdbfast2/gp1       |       35002

"""],
"""
Mirroring: Group
Multi Home: NOT Multi
""")

#
# This is a non-multi-home, spread mirror
#
simple_test("testSpreadMultiGuess_2", testGuessSpreadAndMultiHome, [
"""
 dbid | content | role | preferred_role | mode | status |      hostname      |      address       | port  |                     datadir         | replication_port
------+---------+------+----------------+------+--------+--------------------+--------------------+-------+-------------------------------------+------------------
    1 |      -1 | p    | p              | s    | u      | master-host        | primary-host       |  5432 |/datadirpathdbmaster/gp-1            |
    2 |       0 | p    | p              | s    | u      | first-host         | first-host         | 50001 |/datadirpathdbfast1/gp0              |       55001
    7 |       0 | m    | m              | s    | u      | second-host        | second-host        | 40001 |/second/datadirpathdbfast3/gp0       |       45001
    3 |       1 | p    | p              | s    | u      | third-host         | third-host         | 50002 |/datadirpathdbfast2/gp1              |       55002
    9 |       1 | m    | m              | s    | u      | fourth-host        | fourth-host        | 40002 |/second/datadirpathdbfast4/gp1       |       45002
    4 |       2 | m    | m              | s    | u      | first-host         | first-host         | 60001 |/datadirpathdbfast3/gp0              |       65001
    6 |       2 | p    | p              | s    | u      | second-host        | second-host        | 30001 |/second/datadirpathdbfast1/gp0       |       35001
    5 |       3 | m    | m              | c    | d      | third-host         | third-host         | 60002 |/datadirpathdbfast4/gp1              |       65002
    8 |       3 | p    | p              | c    | u      | fourth-host        | fourth-host        | 30002 |/second/datadirpathdbfast2/gp1       |       35002

"""],
"""
Mirroring: Spread
Multi Home: NOT Multi
""")

#
# This is a non-multi-home, group mirror
#
simple_test("testSpreadMultiGuess_3", testGuessSpreadAndMultiHome, [
"""
 dbid | content | role | preferred_role | mode | status |      hostname      |      address         | port  |                     datadir         | replication_port
------+---------+------+----------------+------+--------+--------------------+----------------------+-------+-------------------------------------+------------------
    1 |      -1 | p    | p              | s    | u      | master-host        | primary-host         |  5432 |/datadirpathdbmaster/gp-1            |
    2 |       0 | p    | p              | s    | u      | first-host         | first-host-1         | 50001 |/datadirpathdbfast1/gp0              |       55001
    7 |       0 | m    | m              | s    | u      | second-host        | second-host-1        | 40001 |/second/datadirpathdbfast3/gp0       |       45001
    3 |       1 | p    | p              | s    | u      | first-host         | first-host-2         | 50002 |/datadirpathdbfast2/gp1              |       55002
    9 |       1 | m    | m              | s    | u      | second-host        | second-host-2        | 40002 |/second/datadirpathdbfast4/gp1       |       45002
    4 |       2 | m    | m              | s    | u      | first-host         | first-host-3         | 60001 |/datadirpathdbfast3/gp0              |       65001
    6 |       2 | p    | p              | s    | u      | second-host        | second-host-3        | 30001 |/second/datadirpathdbfast1/gp0       |       35001
    5 |       3 | m    | m              | c    | d      | first-host         | first-host-4         | 60002 |/datadirpathdbfast4/gp1              |       65002
    8 |       3 | p    | p              | c    | u      | second-host        | second-host-4        | 30002 |/second/datadirpathdbfast2/gp1       |       35002

"""],
"""
Mirroring: Group
Multi Home: Multi
""")

#
# This is a non-multi-home, spread mirror
#
simple_test("testSpreadMultiGuess_4", testGuessSpreadAndMultiHome, [
"""
 dbid | content | role | preferred_role | mode | status |      hostname      |      address         | port  |                     datadir         | replication_port
------+---------+------+----------------+------+--------+--------------------+----------------------+-------+-------------------------------------+------------------
    1 |      -1 | p    | p              | s    | u      | master-host        | primary-host         |  5432 |/datadirpathdbmaster/gp-1            |
    2 |       0 | p    | p              | s    | u      | first-host         | first-host-1         | 50001 |/datadirpathdbfast1/gp0              |       55001
    7 |       0 | m    | m              | s    | u      | second-host        | second-host-1        | 40001 |/second/datadirpathdbfast3/gp0       |       45001
    3 |       1 | p    | p              | s    | u      | third-host         | third-host-1         | 50002 |/datadirpathdbfast2/gp1              |       55002
    9 |       1 | m    | m              | s    | u      | fourth-host        | fourth-host-1        | 40002 |/second/datadirpathdbfast4/gp1       |       45002
    4 |       2 | m    | m              | s    | u      | first-host         | first-host-2         | 60001 |/datadirpathdbfast3/gp0              |       65001
    6 |       2 | p    | p              | s    | u      | second-host        | second-host-2        | 30001 |/second/datadirpathdbfast1/gp0       |       35001
    5 |       3 | m    | m              | c    | d      | third-host         | third-host-2         | 60002 |/datadirpathdbfast4/gp1              |       65002
    8 |       3 | p    | p              | c    | u      | fourth-host        | fourth-host-2        | 30002 |/second/datadirpathdbfast2/gp1       |       35002

"""],
"""
Mirroring: Spread
Multi Home: Multi
""")

# All done tests
printTestResults()
