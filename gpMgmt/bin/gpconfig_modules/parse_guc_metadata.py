import errno
import os
import re
import sys

# this finds all instances of a property name that is both surrounded with quotes and is adjacent
# to the metadata "GUC_DISALLOW_IN_FILE", where adjacent means not separated by a closing } bracket,
# assuming all the separate lines have been combined together into one string
GUCS_DISALLOWED_IN_FILE_REGEX = r"\"(\w+)\"[^}]+GUC_DISALLOW_IN_FILE"


class ParseGuc:
    """
    metadata about gucs, like GUC_DISALLOW_IN_FILE, is not available via
sql. Work around that by parsing the C code that defines this metadata, and store
relevant information in a file, to be read by gpconfig when called to
change a GUC.  The Makefile in gpMgmt/bin will use this parser to
 create a file during 'make install', used by gpconfig to validate that a given GUC can be changed
    """

    # hardcoded name for file looked up by gpconfig during its runtime
    DESTINATION_FILENAME = "gucs_disallowed_in_file.txt"
    DESTINATION_DIR = "share/greenplum"

    def __init__(self):
        self.disallowed_in_file_pattern = re.compile(GUCS_DISALLOWED_IN_FILE_REGEX)

    @staticmethod
    def get_source_c_file_paths():
        mypath = os.path.realpath(__file__)
        misc_dir = os.path.join(os.path.dirname(mypath), "../../../src/backend/utils/misc")
        return [os.path.join(misc_dir, "guc.c"), os.path.join(misc_dir, "guc_gp.c")]

    def parse_gucs(self, guc_path):
        with open(guc_path) as f:
            lines = f.readlines()
        combined = " ".join([line.strip() for line in lines])
        return self.disallowed_in_file_pattern.finditer(combined)

    def write_gucs_to_file(self, filepath):
        with open(filepath, 'w') as f:
            paths = self.get_source_c_file_paths()
            for guc_source in paths:
                hits = self.parse_gucs(guc_source)
                for match in hits:
                    title = match.groups()
                    f.write("%s\n" % title)

    def main(self):
        if len(sys.argv) < 2:
            print("usage: parse_guc_metadata <prefix path to installation directory>")
            sys.exit(1)
        prefix = sys.argv[1]
        dir_path = os.path.join(prefix, self.DESTINATION_DIR)
        _mkdir_p(dir_path, 0o755)
        output_file = os.path.join(dir_path, self.DESTINATION_FILENAME)
        self.write_gucs_to_file(output_file)


def _mkdir_p(path, mode):
    try:
        os.makedirs(path, mode)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise

# --------------------------------------------------------------------------
# Main
# --------------------------------------------------------------------------
if __name__ == '__main__':
    ParseGuc().main()
