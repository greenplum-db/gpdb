#!/bin/python
import argparse
import subprocess
import os
from multiprocessing import Pool

GPDB_ZONEDIR_PREFIX = "share/postgresql/timezone"
TZ_ZONEDIR_PREFIX = "usr/share/zoneinfo"
TZ_BINDIR_PREFIX = "usr/bin"
ZDUMP_BINARY = None

class TZFile(object):
    @property
    def name(self):
        return os.path.relpath(self._filename, self._prefix_path)

    def __init__(self, prefix_path, filename, output):
        self._prefix_path = prefix_path
        self._filename = filename
        self._output = output
        if GPDB_ZONEDIR_PREFIX in self._prefix_path:
            self._src = 'GPDB'
        else:
            self._src = 'IANA'

    def __eq__(self, other):
        return not self.__ne__(other)

    def __ne__(self, other):
        # Here we compare the zdump output of this tzfile against the zdump
        # output of another tzfile.
        self_output = subprocess.check_output([ZDUMP_BINARY, "-v", self._filename])
        self_output = self_output.replace(self._prefix_path, "")

        other_output = subprocess.check_output([ZDUMP_BINARY, "-v", other._filename])
        other_output = other_output.replace(other._prefix_path, "")

        self_header = self._output.replace(self._prefix_path, "")
        other_header = other._output.replace(other._prefix_path, "")
        if self_header != other_header:
            print "WARNING header mismtach: \n{src1} header is {header1} \n{src2} header is {header2}".format(src1=self._src, header1=self_header.strip(), src2=other._src, header2=other_header.strip())

        return self_output != other_output

def check_timezone_good(tz_tuple):
    if tz_tuple[0] != tz_tuple[1]:
        print "ERROR Mismatching timezone %s" % tz_tuple[0].name
        return False
    return True

def main():
    parser = argparse.ArgumentParser(description='Check timezone data.')
    parser.add_argument("gpdb_bin", help="gpdb install location")
    parser.add_argument("tz_bin", help="tz install location")
    args = parser.parse_args()

    global ZDUMP_BINARY
    ZDUMP_BINARY = os.path.join(args.tz_bin, TZ_BINDIR_PREFIX, "zdump")

    gpdb_files = get_timezone_files(os.path.join(args.gpdb_bin, GPDB_ZONEDIR_PREFIX))
    tz_files = get_timezone_files(os.path.join(args.tz_bin, TZ_ZONEDIR_PREFIX))

    has_mismatched_tzfiles = False

    parallel_list = []
    for tzname, tzfile in gpdb_files.items():
        if tzname not in tz_files:
            print "Missing tzfile %s in tz repo" % tzname
            has_mismatched_tzfiles = True
            del gpdb_files[tzname]
            continue

        # Remove the entry from tz_files so that at the end of this loop if
        # there are any remaining files we know that they don't exist in GDPB.
        parallel_list.append((gpdb_files[tzname], tz_files[tzname]))
        del tz_files[tzname]

    for tzname, tzfile in tz_files.items():
        print "Missing tzfile %s in gpdb repo" % tzname
        has_mismatched_tzfiles = True

    p = Pool(16)
    if not all(p.map(check_timezone_good, parallel_list)):
        has_mismatched_tzfiles = True

    if not has_mismatched_tzfiles:
        print "All timezones matched"

    return has_mismatched_tzfiles

def get_timezone_files(location):
    tz_files = {}
    for root, dirs, files in os.walk(location):
        for f in files:
            tzfile = get_tz_file_data(location, os.path.join(root, f))
            if tzfile is not None:
                tz_files[tzfile.name] = tzfile
    return tz_files

def get_tz_file_data(prefix_path, filename):
    output = subprocess.check_output(["file", filename])
    if "timezone data" in output:
        #print "found TZ file %s" % os.path.relpath(filename, prefix_path)
        return TZFile(prefix_path, filename, output)
    return None


if __name__ == '__main__':
    value = main()
    exit(value)
