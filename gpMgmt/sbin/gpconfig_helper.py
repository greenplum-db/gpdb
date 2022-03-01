#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2009. All Rights Reserved.
#
#
# NOTE: The file semantics here are subtle.  In this file, we are reading or writing to filename, which is
#   presumed to be a postgresql.conf format file.  When reading a file, we read that file in a single open(file) call
#   When writing a file, we read that file, and write the new file contents to a temporary file created securely.
#   Then, we use os.rename() to overwrite that existing file.  These rules allow us to either get the OLD file or the
#   NEW file in any subsequent operation.  This is because os.rename() POSIX requires does not overwrite the existing
#   file but swaps the inodes of the files, so the os.rename() does NOT affect any current readers who have already
#   opened the OLD file.

try:
    import os
    import shutil
    import sys
    import tempfile

    from optparse import Option, OptionParser
    from gppylib.gpparseopts import OptParser, OptChecker
except ImportError as e:
    sys.exit('Cannot import modules.  Please check that you have sourced greenplum_path.sh.  Detail: ' + str(e))

_help = ["""This enables one to add, get and remove postgresql.conf configuration parameters.
The absolute path to the postgresql.conf file is required."""]


def parseargs():
    parser = OptParser(option_class=OptChecker)

    parser.setHelp(_help)

    parser.remove_option('-h')
    parser.add_option('-h', '-?', '--help', action='help', help='show this help message and exit')

    parser.add_option('--file', type='string', help='Required: The absolute path of postgresql.conf')
    parser.add_option('--add-parameter', type='string', help='The configuration parameter to add. --value is required.')
    parser.add_option('--value', type='string', help='The configuration value to add when using --add-parameter.')
    parser.add_option('--get-parameter', type='string', help='The configuration parameter value to return.')
    parser.add_option('--remove-parameter', type='string', help='The configuration parameter value to disable.')

    (options, args) = parser.parse_args()
    return validate_args(options)


def validate_args(options):
    if not options.file:
        sys.stderr.write("--file required")
        sys.exit(1)

    if options.add_parameter and not options.value:
        sys.stderr.write("Missing --value <value> when adding parameter '%s'." % options.add_parameter)
        sys.exit(1)

    if (options.get_parameter or options.remove_parameter) and options.value:
        sys.stderr.write("Cannot specify --value when using --get-parameter or --remove-parameter")
        sys.exit(1)

    if (options.add_parameter and options.get_parameter) or \
            (options.add_parameter and options.remove_parameter) or \
            (options.get_parameter and options.remove_parameter):
        sys.stderr.write("Can only specify one of --add-parameter, --get-parameter, or --remove-parameter")
        sys.exit(1)

    return options


def _read_from_file_and_get_empty_tempfile(filename):
    with open(filename, 'r') as infile:
        lines = infile.readlines()

    # TODO: does this work in the case of temp_conf_file containing spaces?
    (tempFD, temp_conf_path) = tempfile.mkstemp(prefix="postgresql_", suffix=".conf", dir=os.path.abspath(os.path.dirname(filename)))
    os.close(tempFD)
    os.chmod(os.path.abspath(temp_conf_path), os.stat(os.path.abspath(filename)).st_mode)
    return lines, temp_conf_path


def comment_parameter(filename, name):
    lines, temp_conf_path = _read_from_file_and_get_empty_tempfile(filename)

    new_lines = 0
    with open(os.path.abspath(temp_conf_path), 'w') as outfile:
        for line in lines:
            potential_match = line.split("=", 1)[0]
            if potential_match.strip() == name:
                outfile.write('#')
            outfile.write(line)
            new_lines = new_lines + 1

    if new_lines == len(lines):
        os.rename(os.path.abspath(temp_conf_path), filename)


def add_parameter(filename, name, value):
    lines, temp_conf_path = _read_from_file_and_get_empty_tempfile(filename)

    new_lines = 0
    with open(os.path.abspath(temp_conf_path), 'w') as outfile:
        for line in lines:
            outfile.write(line)
            new_lines = new_lines + 1
        outfile.write(name + '=' + value + os.linesep)
        new_lines = new_lines + 1

    if new_lines == len(lines) + 1:
        os.rename(os.path.abspath(temp_conf_path), filename)


# NOTE: though apparently not documented, postgresQL returns the last valid value
def get_parameter(filename, name):
    with open(filename, 'r') as f:
        for line in reversed(f.readlines()):
            parts = line.split("=", 1)
            if len(parts) > 1 and parts[0].lstrip().startswith(name):
                return parts[1].strip()


def main():
    options = parseargs()
    if options.get_parameter:
        try:
            value = get_parameter(options.file, options.get_parameter)
            sys.stdout.write(value)
            return
        except Exception as err:
            sys.stderr.write("Failed to get value for parameter '%s' in file %s due to: %s" % (
                options.get_parameter, options.file, err.message))
            sys.exit(1)

    if options.remove_parameter:
        try:
            comment_parameter(options.file, options.remove_parameter)
            return
        except Exception as err:
            sys.stderr.write("Failed to remove parameter '%s' in file %s due to: %s" %
                             (options.remove_parameter, options.file, err.message))
            sys.exit(1)

    if options.add_parameter:
        try:
            comment_parameter(options.file, options.add_parameter)
            add_parameter(options.file, options.add_parameter, options.value)
            return
        except Exception as err:
            sys.stderr.write("Failed to add parameter '%s' in file %s due to: %s" %
                             (options.add_parameter, options.file, err.message))
            sys.exit(1)


if __name__ == "__main__":
    main()
