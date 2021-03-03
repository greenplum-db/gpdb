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

# other global variables
glob_mdp_name = ""
glob_header_to_thread = ""
glob_xml_comment = ""


def analyze_and_convert_metadata(metadata):
    for rel in metadata.iterfind('dxl:Relation', ns):
        part_col = rel.get('PartitionColumns')
        if part_col is not None:
            if rel.find('dxl:Partitions', ns) is None:
                sys.stderr.write(
                    "Warning, %s: file was not properly converted, please run the other script first, exiting...\n" % glob_mdp_name)
                exit(1)

            for index_info in list(rel.find('dxl:IndexInfoList', ns)):
                index_info_mdid = index_info.get('Mdid')
                for index in metadata.iterfind('dxl:Index', ns):
                    if index.get('Mdid') == index_info_mdid:
                        if index.find('dxl:Partitions', ns) is not None:
                            sys.stderr.write(
                                "Warning, %s: file is already converted, exiting...\n" % glob_mdp_name)
                            exit(1)
                        # found a partitioned relation that also has partitioned index(es)
                        convert_partitioned_index(metadata, rel, index)

def convert_partitioned_index(metadata, root_rel, root_index):
    rootrel_mdid = root_rel.get("Mdid")
    rootindex_mdid = root_index.get('Mdid')
    print(f'Found root_rel {rootrel_mdid}, root_index {rootindex_mdid}')
    partrel_mdids = []
    for part in list(root_rel.find('dxl:Partitions', ns)):
        partrel_mdid = part.get('Mdid')
        partrel_mdids.append(partrel_mdid)
    nparts = len(partrel_mdids)

    # remove the existing <dxl:PartConstraint> element(s)
    existing_constraint = root_index.find('dxl:PartConstraint', ns)
    if existing_constraint is not None:
        root_index.remove(existing_constraint)

    # make a new Partitions element
    partitions = et.Element('dxl:Partitions')
    for pix in range(1, nparts + 1):
        partindex_mdid = get_leaf_mdid(rootindex_mdid, pix)
        part = et.Element('dxl:Partition')
        part.set('Mdid', partindex_mdid)
        partitions.append(part)
        # update the corresponding part relation's index info list
        for rel in metadata.iterfind('dxl:Relation', ns):
            if rel.get('Mdid') == partrel_mdids[pix - 1]:
                index_info_list = rel.find('dxl:IndexInfoList', ns)
                for index_info in index_info_list:
                    index_info_mdid = index_info.get('Mdid')
                    if index_info_mdid == rootindex_mdid:
                        # replace with a new child index info
                        print(f'Updating child relation {partrel_mdids[pix - 1]} new index info {partindex_mdid}')
                        index_info_list.remove(index_info)
                        new_index_info = et.Element('dxl:IndexInfo')
                        new_index_info.set('Mdid', partindex_mdid)
                        new_index_info.set('IsPartial', 'false')
                        index_info_list.append(new_index_info)
                        break;
                break;
    root_index.append(partitions)

def get_leaf_mdid(root_mdid, part_num):
    ids = root_mdid.split('.')
    ids[1] = str(int(ids[1]) * 1000 + part_num)
    return '.'.join(ids)


def convert_one_mdp(input_path, output_path):
    et.register_namespace('dxl', 'http://greenplum.com/dxl/2010/12/')
    xml_parser = et.XMLParser(target=et.TreeBuilder(insert_comments=True))
    tree = et.parse(input_path, xml_parser)
    root = tree.getroot()
    thread = root.find('dxl:Thread', ns)
    if thread is not None:
        metadata = thread.find('dxl:Metadata', ns)
    else:
        # non-MDP file like md.xml
        metadata = root.find('dxl:Metadata', ns)
    if metadata is None:
        dynscans = root.findall('.//Metadata', ns)
        if len(dynscans) == 0:
            sys.stderr.write(
                "Warning, %s: Does not contain <dxl:Metadata> tag, exiting...\n" % glob_mdp_name)
        else:
            sys.stderr.write(
                "Warning, %s: <dxl:Metadata> tag is not in a supported place, exiting...\n" % glob_mdp_name)
        exit(1)
    parse_mdp_header(input_path)

    analyze_and_convert_metadata(metadata)

    if output_path is None:
        return

    intermediate_path1 = output_path + '.temp1'
    intermediate_path2 = output_path + '.temp2'
    tree.write(intermediate_path1)
    fix_mdp_header(input_path, intermediate_path1, intermediate_path2)
    os.system('xmllint --format %s >%s' % (intermediate_path2, output_path))
    os.remove(intermediate_path1)
    os.remove(intermediate_path2)

def fix_mdp_header(input_path, temp_path, output_path):
    with open(temp_path, "r") as temp_file, open(output_path, "w") as output_file:
        # take all the lines up to <dxl:Thread from the original input file
        if glob_header_to_thread is not None:
            output_file.write(glob_header_to_thread)
            temp_file_started = False
        else:
            temp_file_started = True

        # then take all the remaining lines from the temp file
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

    # if we reach here, the file did not have a dxl:Thread tag
    glob_header_to_thread = None


def main():
    global glob_mdp_name

    parser = argparse.ArgumentParser(description='Analyze and alter mdp for 7X partitioned index')
    parser.add_argument('--write', nargs='?', default='analyze__only', const='use__mdp__file', help='update the file in-place')
    parser.add_argument('mdp_file', nargs='?',
                        help='mdp file to read and optionally update, if an output file is specified')

    args = parser.parse_args()
    if args.mdp_file is None:
        sys.stderr.write("Error, missing mandatory mdp file name argument\n")
        exit(1)
    glob_mdp_name = args.mdp_file
    if args.write == "analyze__only":
        output_path = None
    elif args.write == "use__mdp__file":
        output_path = args.mdp_file
    else:
        output_path = args.write
    convert_one_mdp(args.mdp_file, output_path)


if __name__ == '__main__':
    main()
