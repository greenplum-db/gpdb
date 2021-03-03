#!/usr/bin/env python3

import argparse
import re

_help = """
Converts a minirepro taken with GPDB6 for use with GPDB 7
"""


# Description:
#
# Usage: ./convert_minirepro_6_to_7.py my_minirepro
# creates my_minirepro_7X file
#
# GPDB7 added five new columns stacoll1 ... stacoll5 for the used collations.
# This program will set the collation to 0::oid, except when staopn is the
# operator for char (631), name (660), text (664), or bpchar (1058).
# Other types use these operators as well, for example varchar uses the text
# operator.
# This script outputs a file suffixed with "_7X" in the same directory as the
# original file.


def process_minirepro(input_filepath, output_filepath):
	with open(input_filepath, 'r') as infile, open(output_filepath, 'w+') as outfile:
		line = infile.readline()
		while line:
			# this program does not handle 5X minirepros
			if 'set allow_system_table_mods="DML";' in line:
				raise Exception('Minirepro is in GPDB5 format, run convert_minirepro_5_to_6.py first')
			outfile.write(line)
			if 'INSERT INTO pg_statistic' in line:
				convert_insert_statement(infile, outfile)
			line = infile.readline()


# This expects and converts an insert statement from the old,
# GPDB6 format (below) to the new GPDB7 format:
# INSERT INTO pg_statistic VALUES (
# 	'schema.table'::regclass,
# 	1::smallint,
# 	False::boolean,
# 	0.0::real,
# 	24::integer,
# 	-1.0::real,
# 	2::smallint,
# 	0::smallint,
# 	0::smallint,
# 	0::smallint,
# 	0::smallint,
# 	1058::oid,
# 	0::oid,
# 	0::oid,
# 	0::oid,
# 	0::oid,
# 	NULL::real[],
# 	NULL::real[],
# 	NULL::real[],
# 	NULL::real[],
# 	NULL::real[],
#	E'{1}::sometype[],
#	E'{1}::sometype[],
#	E'{1}::sometype[],
#	E'{1}::sometype[],
#	NULL::anyarray);

# we add a list of 5 collation oids after the 5 operator oids
# (starting with "1058::oid" in the example above)

# use collation 100 (default) for text-related comparison operators
collation_map = { 631: 100, 660: 100, 664: 100, 1058: 100 }

def convert_insert_statement(infile, outfile):
	coll_list = []
	for line_number in range(0,22):
		line = infile.readline()
		if line_number in range(11,16):
			# read the 5 operator oids and translate them into collation oids
			oid_match = re.search("([0-9]*)::oid", line)
			col_oid = int(oid_match.group(1))
			coll = collation_map.get(col_oid)
			if coll is None:
				coll = 0
			coll_list.append(coll)
		if line_number == 16:
			# write the additional 5 collation oids to the output file
			outfile.write("\t%d::oid,\n\t%d::oid,\n\t%d::oid,\n\t%d::oid,\n\t%d::oid,\n" % tuple(coll_list))
		outfile.write(line)


def parseargs():
	parser = argparse.ArgumentParser(description=_help)

	parser.add_argument("filepath", help="Path to minirepro file")

	args = parser.parse_args()
	return args


def main():
	args = parseargs()

	input_filepath = args.filepath
	output_filepath = input_filepath + "_7X"

	process_minirepro(input_filepath, output_filepath)


if __name__ == "__main__":
	main()
