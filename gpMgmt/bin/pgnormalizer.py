#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import getopt

__copyright__ = "Copyright (c) 2017-Present Pivotal Software, Inc"


class PGNormalizer:
    def __init__(self, buffer_size, in_source, out_source):
        self.buffer_size = buffer_size
        self.in_source = in_source
        self.out_source = out_source

    def chunk_reader(self):
        while True:
            buf = self.in_source.read(self.buffer_size)

            if buf:
                yield buf
            else:
                return

    def write_out(self, options):
        last7 = ""

        try:
            while True:
                lines = next(self.chunk_reader())

                if options.raw_mode:
                    lines = lines.replace("\\", "\\\\")

                if options.column_delimiter and options.column_delimiter is not "\t":
                    lines = lines.replace("\t", "\\t")

                if options.line_delimiter and options.line_delimiter is not "\n":
                    lines = lines.replace("\n", "\\n")

                lines = last7 + lines

                if options.column_delimiter and options.column_delimiter is not "\t":
                    lines = lines.replace(options.column_delimiter, "\t")

                if options.line_delimiter and options.line_delimiter is not "\n":
                    lines = lines.replace(options.line_delimiter, "\n")
                else:
                    lines = lines.replace("\r\n", "\n")

                self.out_source.write(lines[:-7])
                last7 = lines[-7:]
        except StopIteration:
            self.out_source.write(last7)
            self.out_source.flush()
            return


class PGNOption:
    def __init__(self, sys_argv, options="ec:hl:r"):
        self.escape_mode = False
        self.raw_mode = False
        self.line_delimiter = ""
        self.column_delimiter = ""
        self.sys_argv = sys_argv
        self.options = options

        try:
            opts, args = getopt.getopt(self.sys_argv[1:], self.options)
        except getopt.GetoptError as err:
            raise PGNError(str(err))

        for o, a in opts:
            if o == "-h":
                print usage()
                raise PGNExit()
            elif o == "-e":
                self.escape_mode = True
            elif o == "-r":
                self.raw_mode = True
            elif o == "-l":
                if a.startswith("\\x"):
                    self.line_delimiter = a[2:].decode("hex")
                else:
                    self.line_delimiter = a.decode("unicode-escape")
            elif o == "-c":
                if a.startswith("\\x"):
                    self.column_delimiter = a[2:].decode("hex")
                else:
                    self.column_delimiter = a.decode("unicode-escape")
            else:
                assert False, "unhandled option"

        if len(self.column_delimiter) > 8 or len(self.line_delimiter) > 8:
            raise PGNError("The length of delimiter must be smaller than 8.")

        if not self.escape_mode and not self.raw_mode:
            raise PGNError("You must specify the mode, raw or escape.")

        if self.escape_mode and self.raw_mode:
            raise PGNError("You can only specify one mode.")

        if len(self.column_delimiter) > 1 and ("\t" in self.column_delimiter or "\n" in self.column_delimiter):
            raise PGNError("Column delimiters could not contain any TAB or NEWLINE character.")

        if len(self.line_delimiter) > 1 and ("\t" in self.line_delimiter or "\n" in self.line_delimiter):
            raise PGNError("Line delimiters could not contain any TAB or NEWLINE character.")


def usage():
    return "Usage: " + sys.argv[0] + " -e/r [-c column_delimiter] [-l column_delimiter]"


class PGNError(Exception):
    def __init__(self, msg):
        self.message = msg

    def __str__(self):
        return repr(self.message)


class PGNExit(Exception):
    pass


def main():
    try:
        options = PGNOption(sys.argv)
    except PGNError as err:
        sys.stderr.write(err.message + "\n\n" + usage())
        sys.exit(1)
    except PGNExit:
        sys.exit(0)

    pgn = PGNormalizer(64 * 1024 * 1024, sys.stdin, sys.stdout)

    pgn.write_out(options)

    return


if __name__ == "__main__":
    main()
