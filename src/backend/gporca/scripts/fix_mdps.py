#!/usr/bin/env python
#
# updates plan and plan size in mdp files. Run ./fix_mdps.py --help for detailed description
# Should be run from within <orca_src>/build directory
#
# usage: fix_mdps.py input.txt
# where input.txt is a file containing failed tests, one per line (ctest output)
# in the form 56 - gporca_test_CIndexScanTest (Failed)
#
# or
# fix_mdps.py --logFile <path_to_ctest_output>
#

import sys
import subprocess
import re
import argparse

dryrun = False

def run_command(command):
    p = subprocess.Popen(command,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT)
    return iter(p.stdout.readline, b'')

def parseInputFile(inputFile):
    failed_tests = []
    with open(inputFile, "r") as fp:
        for line in fp:
            failed_test = re.search(r'.*gporca_test_(.*) .*', line)
            if failed_test:
                failed_tests.append(failed_test.group(1))
    return failed_tests

def replacePlanSize(filename, actual, expected):
    expectedLine = "SpaceSize=\"%s\"" % (expected)
    actualLine = "SpaceSize=\"%s\"" % (actual)

    with open(filename, "r") as fp:
        filedata = fp.read()
    filedata = filedata.replace(expectedLine, actualLine)
    with open(filename, 'w') as fp:
        fp.write(filedata)

def replacePlanInFile(filename, innerContent):
    with open(filename, "r") as fp:
        filedata = fp.read()
    filedata = re.sub('    <dxl:Plan.*</dxl:Plan>\n',innerContent,filedata,flags=re.DOTALL)
    with open(filename, 'w') as fp:
        fp.write(filedata)
    return

def processLogFile(logFileLines):
    current_file =""
    read_plan = 0
    actual_plan = ""
    errorfile = ""
    actualSize = 0

    for line in logFileLines:
        fileNameMatch = re.match(r'.*../data/dxl/minidump/*.', line)
        actualPlanMatch = re.match(r'Actual:', line)
        planBegin = re.match(r'.*<dxl:Plan*.', line)
        planEnd = re.match(r'.*</dxl:Plan>*.', line)
        actualSizeMatch = re.search(r'Actual size:', line)
        expectedSizeMatch = re.search(r'Expected size:', line)
        errorLogMatch = re.search(r',ERROR,', line)
        failureMatch = re.search(r'[*][*][*] FAILED [*][*][*]', line)

        if fileNameMatch:
            current_file = line.split()[-1]
            current_file = current_file.split('\"')[0]
            if read_plan > 0:
                print("Log file contains partial plans for %s, please update this file by hand" % (current_file))
                read_plan = 0
        elif actualPlanMatch:
            read_plan = 1
            actual_plan = ""
        elif planBegin and read_plan == 1:
            actual_plan = actual_plan + "    " + line
            read_plan = 2
        elif planEnd and read_plan > 1:
            read_plan = 0
            actual_plan = actual_plan + "    " + line
            if not dryrun:
                replacePlanInFile(current_file, actual_plan )
                print("Changed query plan in %s \n" % (current_file))
            else:
                print("Query plan is different in %s \n" % (current_file))
        elif read_plan > 1:
            actual_plan = actual_plan + "    " + line
            if re.search(r',TRACE,', line):
                # we reached the end of the plan section without finding the end plan tag
                print("Log file contains partial plans for %s, please update this file by hand" % (current_file))
                read_plan = 0
        elif actualSizeMatch:
            actualSize = re.search('Actual size: (\d+)', line).group(1)
        elif expectedSizeMatch:
            expectedSize = re.search('Expected size: (\d+)', line).group(1)
            if not dryrun:
                replacePlanSize(current_file, actualSize, expectedSize)
                print("Changed plan size in %s \n" % (current_file))
            else:
                print("Plan size is different in %s \n" % (current_file))
            failureMatch = 0
        elif errorLogMatch:
            print("Error in file %s: %s" % (current_file, line))
        elif failureMatch:
            if not re.search(r'Plan [a-z ]*comparison', line) and not re.search(r'Unittest', line):
                if errorfile != current_file:
                    print("File %s failed for some other reason\n" % (current_file))
                    errorfile = current_file

def runTests(testFileNames):
    tests_to_fix = parseInputFile(testFileNames)
    for test in tests_to_fix:
        command = "./server/gporca_test -U " + test
        print("Running test: " + test)
        command = command.split()
        all_lines = run_command(command)
        processLogFile(all_lines)

def processExistingLogFile(logFileName):
    with open(logFileName, "r") as fp:
        processLogFile(fp)

def main():
    parser = argparse.ArgumentParser(description='Fix test failures in MDP files')
    parser.add_argument('failed_tests_file', nargs = '?', help='a file containing failed tests, one per line (ctest output)')
    parser.add_argument('--dryRun', action='store_true',
                        help='dry run only, do not actually change MDP files')
    parser.add_argument('--logFile',
                        help='Log file created with ctest --output-on-failure command')

    args = parser.parse_args()

    dryrun = args.dryRun
    inputfile = args.failed_tests_file
    logfile = args.logFile

    if inputfile is None and logfile is None:
        print("Either a file with failed tests or a log file needs to be specified\n")
        exit(1)
    elif inputfile is not None and logfile is not None:
        print("If a file with failed tests is specified, the --logFile is not allowed\n")
        exit(1)

    print("File: %s, dryrun %s, logfile %s\n" % (inputfile, dryrun, logfile))

    if inputfile is not None:
        runTests(inputfile)
    else:
        processExistingLogFile(logfile)

if __name__== "__main__":
    main()
