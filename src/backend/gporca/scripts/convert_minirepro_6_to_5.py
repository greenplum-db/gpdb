#!/usr/bin/env python

import argparse

_help = """
Converts a minirepro taken with GPDB 6 for use with GPDB 5
"""


# Description:
#
# Usage: ./convert_minirepro_6_to_5.py my_minirepro_6X
# creates my_minirepro file
#
# GPDB6 added a new field to pg_statistic, making minirepros taken with GPDB6
# or later incompatible with GPDB5 or earlier. This script removes the additional
# field so the minirepro can be loaded into a GPDB5 system for testing/verification.
# This script outputs a file without the additional field in the same directory as the
# original file.


def process_minirepro(input_filepath, output_filepath):
    with open(input_filepath, 'r') as infile, open(output_filepath, 'w+') as outfile:
        # We handle next line instead of the current so that we can process the line
        # before `relallvisible =`. Unfortunately, it does somewhat convolute the logic.
        next_line = infile.readline()
        line = next_line
        next_line = infile.readline()
        while line:
            # allow_system_table_mods GUC was changed to a boolean in GPDB6
            if 'set allow_system_table_mods=on;' in line:
                line = 'set allow_system_table_mods="DML";\n'
            if 'set allow_system_table_mods=true;' in line:
                line = 'set allow_system_table_mods="DML";\n'
            if next_line and 'relallvisible =' in next_line:
                line = line[:line.rfind(",")].rstrip() + '\n'
            if 'relallvisible =' not in line and 'INSERT INTO pg_statistic' not in line:
                outfile.write(line)
            if 'INSERT INTO pg_statistic' in next_line:
                # This handles the writing of the `INSERT INTO pg_statistic` line
                outfile.write(next_line)
                convert_insert_statement(infile, outfile)
            line = next_line
            next_line = infile.readline()

def convert_insert_statement(infile, outfile):
    for line_number in range(0, 26):
        line = infile.readline()
        if line_number not in [2, 10, 15, 20, 25]:
            if line_number == 24:
                line = line.replace(",", ");")
            outfile.write(line)

def parseargs():
    parser = argparse.ArgumentParser(description=_help)

    parser.add_argument('--version', action='version', version='1.0')
    parser.add_argument("filepath", help="Path to minirepro_6X file")

    args = parser.parse_args()
    return args


def main():
    args = parseargs()

    input_filepath = args.filepath
    output_filepath = input_filepath + "_5X"

    process_minirepro(input_filepath, output_filepath)


if __name__ == "__main__":
    main()
