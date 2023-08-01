#!/usr/bin/env python3

import base64
import os
import pickle
import subprocess
import sys

from gppylib.operations.validate_disk_space import FileSystem
from gppylib.gpparseopts import OptParser, OptChecker
from gppylib.mainUtils import addStandardLoggingAndHelpOptions
from gppylib.commands.unix import findCmdInPath

def calculate_disk_free(directories):
    """
    For each directory determine the filesystem and calculate the free disk space.
    input is list of directories 
    output is list FileSystem() objects for the queried directories.
    """
    filesystems = []
    for dir in directories:
        cmd = _disk_free(dir)
        if not cmd:
            #Return empty list since fetching disk free stats failed 
            return []

        # skip the first line which is the header
        for line in cmd.stdout.split('\n')[1:]:
            if line != "":
                parts = line.split()
                fs = FileSystem(parts[0], disk_free=int(parts[3]))
                fs.directories = [dir]
                filesystems.append(fs)

    return filesystems


# Since the input directory may not have been created df will fail. Thus, take
# each path element starting at the end and execute df until it succeeds in
# order to find the filesystem and free space.
def _disk_free(directory):
    """ 
    Fetch the disk free stats for the given directory
    input is a directory
    Output is the disk free command output
    """
    path = directory

    while(path!=os.sep and not os.path.exists(path)):
       path, last_element = os.path.split(path)

    if path == os.sep:
        sys.stderr.write("Failed to calculate free disk space: reached root dir \"%s\"."% path ) 
        return 

    # The -P flag is for POSIX formatting to prevent errors on lines that
    # would wrap.
    cmdResult = subprocess.run([findCmdInPath('df'), "-Pk", path],
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE,
                         universal_newlines=True)
    
    if cmdResult.returncode != 0:
        sys.stderr.write("Failed to calculate free disk space: %s." % cmdResult.stderr)
        return 

    return cmdResult


def create_parser():
    parser = OptParser(option_class=OptChecker,
                       description='Calculates the disk free for the filesystem given the input directory. '
                                   'Returns a list of base64 encoded pickled FileSystem objects.')

    addStandardLoggingAndHelpOptions(parser, includeNonInteractiveOption=True)

    parser.add_option('-d', '--directories',
                      help='list of directories to calculate the disk usage for the filesystem',
                      type='string',
                      action='callback',
                      callback=lambda option, opt, value, parser: setattr(parser.values, option.dest, value.split(',')),
                      dest='directories')

    return parser


# NOTE: The caller uses the Command framework such as CommandResult
# which assumes that the **only** thing written to stdout is the result. Thus,
# do not use a logger to print to stdout as that would affect the deserialization
# of the actual result.
def main():
    parser = create_parser()
    (options, args) = parser.parse_args()

    filesystems = calculate_disk_free(options.directories)
    sys.stdout.write(base64.urlsafe_b64encode(pickle.dumps(filesystems)).decode())
    return


if __name__ == "__main__":
    main()
