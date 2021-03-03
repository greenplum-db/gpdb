#! /usr/bin/env python3

import argparse
import xml.etree.ElementTree as et
import copy
import tempfile
import re
import os
import sys
import datetime
import base64
import hashlib

# the DXL namespace used
ns = {'dxl': 'http://greenplum.com/dxl/2010/12/'}

# GPDBScalarOp tags that this program knows how to fix (Mdid is used without such a tag in the MDP)
# TODO: Add many more such MDIds
scalar_op_dict = {
    # oper. mdid  name ComparisonType leftType   rightType     resultType  opFunc        commutator    inverseOp
    "0.96.1.0":   ("=",    "Eq",   "0.23.1.0",   "0.23.1.0",   "0.16.1.0", "0.65.1.0",   "0.96.1.0",   "0.518.1.0"),
    "0.97.1.0":   ("<",    "LT",   "0.23.1.0",   "0.23.1.0",   "0.16.1.0", "0.66.1.0",   "0.521.1.0",  "0.525.1.0"),
    "0.525.1.0":  (">=",   "GEq",  "0.23.1.0",   "0.23.1.0",   "0.16.1.0", "0.150.1.0",  "0.523.1.0",  "0.97.1.0"),
    "0.1098.1.0": (">=",   "GEq",  "0.1082.1.0", "0.1082.1.0", "0.16.1.0", "0.1090.1.0", "0.1096.1.0", "0.1095.1.0"),
    "0.1095.1.0": ("<",    "LT",   "0.1082.1.0", "0.1082.1.0", "0.16.1.0", "0.1087.1.0", "0.1097.1.0", "0.1098.1.0")
}

# indexes into the tuple stored in scalar_op_dict
op_name_ix = 0
op_comp_type_ix = 1
op_left_type_ix = 2
op_right_type_ix = 3
op_result_type_ix = 4
op_func_ix = 5
op_commutator_ix = 6
op_inverse_op_ix = 7

# MDCast tags that this program knows how to fix
# TODO: Add many more such MDIds
cast_dict = {
    # Mdid                Name BinaryCoercible  SourceTypeId  DestinationTypeId CastFuncId   CoercePathType
    "3.1043.1.0;25.1.0": ("text",       "true", "0.1043.1.0", "0.25.1.0",       "0.0.0.0",   "0")
}

cast_name_ix = 0
cast_coercible_ix = 1
cast_source_ix = 2
cast_dest_ix = 3
cast_func_id_ix = 4
cast_coerce_path_type_ix = 5

# other global variables
glob_mdp_name = ""
glob_header_to_thread = ""
glob_xml_comment = ""

def convert_one_mdp(file_path, mdid):
    et.register_namespace('dxl', 'http://greenplum.com/dxl/2010/12/')
    xml_parser = et.XMLParser(target=et.TreeBuilder(insert_comments=True))
    tree = et.parse(file_path, xml_parser)
    root = tree.getroot()
    thread = root.find('dxl:Thread', ns)
    metadata = thread.find('dxl:Metadata', ns)

    parse_mdp_header(file_path)

    intermediate_path1 = file_path + '.temp1'
    intermediate_path2 = file_path + '.temp2'
    convert_metadata(metadata, mdid)
    tree.write(intermediate_path1)
    fix_mdp_header(file_path, intermediate_path1, intermediate_path2)
    os.system('xmllint --format %s >%s' % (intermediate_path2, file_path))
    os.remove(intermediate_path1)
    os.remove(intermediate_path2)


def convert_metadata(metadata, mdid):
    scalar_ops = metadata.findall('dxl:GPDBScalarOp', ns)
    for scop in scalar_ops:
        op_mdid = scop.get('Mdid')
        if op_mdid == mdid:
            sys.stderr.write(
                "Warning, %s: scalar op %s already exists in file, exiting...\n" % (glob_mdp_name, mdid))
            exit(1)
    casts = metadata.findall('dxl:MDCast', ns)
    for md_cast in casts:
        op_mdid = md_cast.get('Mdid')
        if op_mdid == mdid:
            sys.stderr.write(
                "Warning, %s: cast %s already exists in file, exiting...\n" % (glob_mdp_name, mdid))
            exit(1)
    add_scalar_op(metadata, mdid)


def add_scalar_op(metadata, mdid):
    op_content = scalar_op_dict.get(mdid, None)

    if op_content is not None:
        # add a GPDBScalarOp
        new_op = et.Element('dxl:GPDBScalarOp')
        new_op.set("Mdid", mdid)
        new_op.set("Name", op_content[op_name_ix])
        new_op.set("ComparisonType", op_content[op_comp_type_ix])
        new_op.set("ReturnsNullOnNullInput", "true")

        tags =    [ "dxl:LeftType", "dxl:RightType",    "dxl:ResultType",    "dxl:OpFunc", "dxl:Commutator",    "dxl:InverseOp"]
        indexes = [ op_left_type_ix, op_right_type_ix,  op_result_type_ix,   op_func_ix,   op_commutator_ix,    op_inverse_op_ix ]

        for ix in range(len(tags)):
            sub_tag = et.Element(tags[ix])
            sub_tag.set("Mdid", op_content[indexes[ix]])
            new_op.append(sub_tag)
    else:
        cast_content = cast_dict.get(mdid, None)
        if cast_content is not None:
            # add an MDCast
            new_op = et.Element('dxl:MDCast')
            new_op.set("Mdid", mdid)
            new_op.set("Name", cast_content[cast_name_ix])
            new_op.set("BinaryCoercible", cast_content[cast_coercible_ix])
            new_op.set("SourceTypeId", cast_content[cast_source_ix])
            new_op.set("DestinationTypeId", cast_content[cast_dest_ix])
            new_op.set("CastFuncId", cast_content[cast_func_id_ix])
            new_op.set("CoercePathType", cast_content[cast_coerce_path_type_ix])
        else:
            sys.stderr.write(
                "Warning, %s: unable to fix lookup failure for mdid %s\n" % (glob_mdp_name, mdid))

    metadata.append(new_op)


def fix_mdp_header(input_path, temp_path, output_path):
    with open(temp_path, "r") as temp_file, open(output_path, "w") as output_file:
        # take all the lines up to <dxl:Thread from the original input file
        output_file.write(glob_header_to_thread)

        # then take all the remaining lines from the temp file
        temp_file_started = False
        temp_file.seek(0)
        for temp_line in temp_file:
            if re.search(r'<dxl:Thread ', temp_line):
                temp_file_started = True
            if temp_file_started:
                output_file.write(temp_line)
        temp_file.close()
        output_file.close()


def parse_mdp_header(input_path):
    global glob_header_to_thread, glob_xml_comment

    with open(input_path, "r") as input_file:
        # take all the lines up to <dxl:Thread from the original input file
        # and also save any XML-style comments <!-- blabla -->
        in_xml_comment = False
        for input_line in input_file:
            if re.search(r'<dxl:Thread ', input_line):
                return
            glob_header_to_thread += input_line
            if re.search(r'<!--', input_line):
                in_xml_comment = True
            if in_xml_comment:
                glob_xml_comment += input_line
            if in_xml_comment and re.search(r'-->', input_line):
                in_xml_comment = False


def main():
    global glob_mdp_name

    parser = argparse.ArgumentParser(description='Fix a lookup failure in an MDP')
    parser.add_argument('--mdid', help='mdid of the lookup failure')
    parser.add_argument('mdp_file', help='mdp file to update')

    args = parser.parse_args()
    if args.mdp_file is None:
        sys.stderr.write("Error, missing mandatory mdp file name argument\n")
        exit(1)

    glob_mdp_name = args.mdp_file
    convert_one_mdp(args.mdp_file, args.mdid)


if __name__ == '__main__':
    main()
