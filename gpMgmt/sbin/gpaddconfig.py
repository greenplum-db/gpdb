#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2009. All Rights Reserved.
#
# This is a private script to be called by gpaddconfig
# The script is executed on a single machine and gets a list of data directories to modify from STDIN
# With the script you can either change the value of a setting (and comment out all other entries for that setting)
# or you can do remove only, to comment out all entries of a setting and not add an entry.
#
try:

    import sys, os
    from optparse import Option, OptionParser
    from gppylib.gpparseopts import OptParser, OptChecker
    from gppylib.gparray import *
    from gppylib.commands.gp import *
    from gppylib.db import dbconn

except ImportError, e:
    sys.exit('Cannot import modules.  Please check that you have sourced greenplum_path.sh.  Detail: ' + str(e))

_help = [""""""]


def parseargs():
    parser = OptParser(option_class=OptChecker)

    parser.setHelp(_help)

    parser.remove_option('-h')
    parser.add_option('-h', '-?', '--help', action='help', help='show this help message and exit')

    parser.add_option('--entry', type='string')
    parser.add_option('--value', type='string')
    parser.add_option('--removeonly', action='store_true')
    parser.set_defaults(removeonly=False)

    # Parse the command line arguments
    (options, args) = parser.parse_args()

    # sanity check
    if not options.entry:
        print "--entry is required"
        sys.exit(1)

    if (not options.value) and (not options.removeonly):
        print "Select either --value or --removeonly"
        sys.exit(1)

    return options


# ------------------------------- Mainline --------------------------------
options = parseargs()

files = list()

# get the files to edit from STDIN
line = sys.stdin.readline()
while line:

    directory = line.rstrip()

    filename = directory + "/postgresql.conf"
    if not os.path.exists(filename):
        raise Exception("path does not exist" + filename)

    files.append(filename)

    line = sys.stdin.readline()

fromString = "(^\s*" + options.entry + "\s*=.*$)"
toString = "#$1"
name = "mycmd"

# update all the files
for f in files:

    # comment out any existing entries for this setting
    cmd = InlinePerlReplace(name, fromString, toString, f)
    cmd.run(validateAfter=True)

    if options.removeonly:
        continue

    cmd = GpAppendGucToFile(name, f, options.entry, options.value)
    cmd.run(validateAfter=True)
