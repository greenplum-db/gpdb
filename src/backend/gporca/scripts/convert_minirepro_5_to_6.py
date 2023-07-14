#!/usr/local/bin/python

import argparse

_help = """
Converts a minirepro taken with GPDB5 or earlier for use with GPDB 6 and later
"""


# Description:
#
# Usage: ./convert_minirepro_5_to_6.py my_minirepro
# creates my_minirepro_6X file
#
# GPDB6 added a new field to pg_statistic, making minirepros taken with GPDB5
# or earlier incompatible with GPDB6+. This script adds "dummy" fields so the
# minirepro can be loaded into a GPDB6+ system for testing/verification. ORCA
# does not use this field currently, so the plan generated should be identical.
# This script outputs a file suffixed with "_6X" in the same directory as the
# original file.


def process_minirepro(input_filepath, output_filepath):
	with open(input_filepath, 'r') as infile, open(output_filepath, 'w+') as outfile:
		line = infile.readline()
		while line:
			if 'allow_system_table_mods=on' in line:
				raise Exception('Minirepro already in GPDB6 format')
			# allow_system_table_mods GUC was changed to a boolean
			if 'set allow_system_table_mods="DML";' in line:
				line = 'set allow_system_table_mods=on;\n'
			outfile.write(line)
			if 'INSERT INTO pg_statistic' in line:
				convert_insert_statement(infile, outfile)
			line = infile.readline()


# This expects and converts an insert statement from the old,
# GPDB5 format (below) to the new GPDB6+ format:
# INSERT INTO pg_statistic VALUES (
# 	'mysch.mytbl'::regclass,
# 	2::smallint,
# 	0.0::real,
# 	3::integer,
# 	1.0::real,
# 	1::smallint,
# 	2::smallint,
# 	0::smallint,
# 	0::smallint,
# 	23::oid,
# 	42::oid,
# 	0::oid,
# 	0::oid,
# 	E'{1}'::real[],
# 	E'{1}'::real[],
# 	NULL::real[],
# 	NULL::real[],
# 	E'{-1}'::int8[],
# 	NULL::int8[],
# 	NULL::int8[],
# 	NULL::int8[]);

# The following map describes the additional lines that must be added to the
# existing insert statement. These lines are added at the line_number key,
# and assume 0 indexing
line_map = {2: "\tFalse::boolean,\n",
			9: "\t0::smallint,\n",
			13: "\t0::oid,\n",
			17: "\tNULL::real[],\n",
			21: "\tNULL::anyarray);\n",
			}


def convert_insert_statement(infile, outfile):
	for line_number in range(0, 22):
		line_to_insert = line_map.get(line_number)
		if line_to_insert:
			outfile.write(line_to_insert)
		line = infile.readline()
		# Remove the parenthesis and semicolon from the last line,
		# since we're adding another line to the end
		if line_number == 20:
			line = line.replace(");", ",")
		outfile.write(line)


def parseargs():
	parser = argparse.ArgumentParser(description=_help)

	parser.add_argument('--version', action='version', version='1.0')
	parser.add_argument("filepath", help="Path to minirepro file")

	args = parser.parse_args()
	return args


def main():
	args = parseargs()

	input_filepath = args.filepath
	output_filepath = input_filepath + "_6X"

	process_minirepro(input_filepath, output_filepath)


if __name__ == "__main__":
	main()
