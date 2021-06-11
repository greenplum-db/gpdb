#!/usr/bin/env python3

import base64
import os
import pickle
import subprocess
import sys

from gppylib.operations.validate_disk_space import FileSystem
from gppylib.gpparseopts import OptParser, OptChecker
from gppylib.mainUtils import addStandardLoggingAndHelpOptions


# For each directory determine the filesystem and calculate the free disk space.
# Returns a list of FileSystem() objects.
def calculate_disk_free(directories):
    filesystem_to_dirs = {}  # map of FileSystem() to list of directories
    for dir in directories:
        cmd = _disk_free(dir)
        if cmd.returncode < 0:
            sys.stderr.write("Failed to calculate free disk space: %s" % cmd.stderr)
            return []

        # skip the first line which is the header
        for line in cmd.stdout.split('\n')[1:]:
            parts = line.split()
            fs = FileSystem(parts[0], disk_free=parts[3])
            filesystem_to_dirs.setdefault(fs, []).append(dir)

    filesystems = []  # list of FileSystem()
    for fs, directories in filesystem_to_dirs.items():
        fs.directories = set(directories)
        filesystems.append(fs)

    return filesystems


# Since the input directory may not have been created df will fail. Thus, take
# each path element starting at the end and execute df until it succeeds in
# order to find the filesystem and free space.
def _disk_free(directory):
    # The -P flag is for POSIX formatting to prevent errors on lines that
    # would wrap.
    cmd = subprocess.run(["df", "-Pk", directory],
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE,
                         universal_newlines=True)

    if directory == os.sep:
        return cmd

    if cmd.returncode < 0:
        path, last_element = os.path.split(directory)
        return _disk_free(path)

    return cmd


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
    sys.stdout.write(base64.urlsafe_b64encode(pickle.dumps(filesystems)).decode('UTF-8'))
    return


if __name__ == "__main__":
    main()
