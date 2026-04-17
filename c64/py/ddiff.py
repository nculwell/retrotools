#!/usr/bin/python3

# Do a binary diff of two files, and print the result in a readable format.
#
# Usage: ddiff.py [options] <left-file> <right-file> [output-file]
#
# Arguments:
#   left-file    First file to compare
#   right-file   Second file to compare
#   output-file  Where to write the diff output (default: stdout)
#
# Options:
#   -a  Append ASCII text representation of each line (high bit masked off)
#   -c  Append C64 PETSCII text representation of each line
#   -b  Append base40-decoded text representation of each line (even offset)
#   -o  Append base40-decoded text representation starting at odd byte offset
#
# Output format:
#   Each differing chunk is shown as a pair of lines (left then right).
#   Bytes that differ are wrapped in parentheses.
#   Bytes absent from the shorter file are shown as --.
#   Groups of 8 and 16 bytes are separated by extra spaces.

import sys, re
import base40, c64text

class ArgExc(Exception):
    pass

# How many bytes to print in one line.
LINE_LENGTH = 32
OFFSET_WIDTH = 6

class Diff:

    def __init__(self, lft_file, rgt_file, **kwargs):
        # lft_file, rgt_file: open binary file objects to compare
        # kwargs: print_c64, print_base40, print_base40_odd, print_ascii (bool flags)
        #         line_length, offset_width (layout overrides)
        self.lft_file = lft_file
        self.rgt_file = rgt_file
        self.line_length = kwargs.get("line_length", LINE_LENGTH)
        self.offset_width = kwargs.get("offset_width", OFFSET_WIDTH)
        self.differences = bytearray(self.line_length)
        self.lft_line = bytearray(self.line_length)
        self.rgt_line = bytearray(self.line_length)
        self.print_c64 = kwargs.get("print_c64", None)
        self.print_base40 = kwargs.get("print_base40", None)
        self.print_base40_odd = kwargs.get("print_base40_odd", None)
        self.print_ascii = kwargs.get("print_ascii", None)

    def diff(self, diff_file):
        # Loop over both files line_length bytes at a time, printing only
        # chunks where the two files differ. Consecutive differing chunks are
        # separated by a dot-row connector; isolated chunks get a blank line.
        line_number = 1
        line_offset = 0
        prev_different = False
        diff_connector = "." * self.offset_width
        while True:
            lft_len = self.lft_file.readinto(self.lft_line)
            rgt_len = self.rgt_file.readinto(self.rgt_line)
            if lft_len == 0 and rgt_len == 0:
                break
            different = self._find_differences(lft_len, rgt_len)
            if different:
                if prev_different:
                    print(diff_connector, file=diff_file)
                else:
                    print(file=diff_file)
                self._print_differences(diff_file, line_offset, lft_len, rgt_len)
            prev_different = different
            line_number += 1
            line_offset += self.line_length

    def _find_differences(self, lft_len, rgt_len):
        # Populate self.differences[i] = 1 for each byte position that differs
        # (including positions present in only one file). Returns True if any
        # byte in this chunk differs.
        min_len = min((lft_len, rgt_len))
        max_len = max((lft_len, rgt_len))
        assert min_len > 0
        assert max_len <= self.line_length
        assert min_len <= max_len
        different = False
        for i in range(min_len):
            lb = self.lft_line[i]
            rb = self.rgt_line[i]
            if lb != rb:
                different = True
                self.differences[i] = 1
            else:
                self.differences[i] = 0
        if min_len < max_len:
            different = True
            for i in range(min_len, max_len):
                self.differences[i] = 1
        if max_len < self.line_length:
            for i in range(max_len, self.line_length):
                self.differences[i] = 0
        return different

    def _print_differences(  
        self,
        f,
        line_offset: int,
        lft_len: int,
        rgt_len: int
    ):
        # print both sides of one differing chunk
        self._print_line(f, line_offset, lft_len, self.lft_line)
        self._print_line(f, line_offset, rgt_len, self.rgt_line)

    def _print_line(
        self,
        f,
        line_offset: int,
        line_len: int,
        line: bytearray
    ):
        # print one side of a differing chunk with hex + optional text
        f.write(("%%0%dX:" % self.offset_width) % line_offset)

        for i in range(self.line_length):

            sep = " "
            if self.differences[i] == 1:
                if i == 0 or self.differences[i-1] == 0:
                    sep = "("
            else:
                if i > 0 and self.differences[i-1] == 1:
                    sep = ")"

            if sep == ")":
                f.write(sep)
            if i % 8 == 0:
                f.write(" ")
            if i % 16 == 0:
                f.write(" ")
            if sep != ")":
                f.write(sep)

            if i < line_len:
                f.write("%02X" % line[i])
            else:
                f.write("--")

        if self.differences[self.line_length-1] == 1:
            f.write(")")
        else:
            f.write(" ")

        if self.print_c64:
            f.write("  ")
            f.write(c64text.decode(line[:line_len]))

        if self.print_base40:
            f.write("  ")
            f.write(base40.base40_decode(line[:line_len]))

        if self.print_base40_odd:
            # TODO: Carry over last byte of previous line
            # so we can render the first character here.
            f.write("  ")
            f.write(base40.base40_decode(line[1:line_len-1]))

        if self.print_ascii:
            f.write("  ")
            f.write("".join([ chr(c&0x7F) if 0x20 <= (c&0x7F) < 0x7F else "~" for c in line[:line_len] ]))

        print(file=f) # end of the line

def parse_args(argv):
    # Parse sys.argv[1:] into (filenames dict, opts dict).
    # Raises ArgExc on invalid switches or wrong number of positional args.
    filenames = {}
    opts = {}
    args = []
    for a in argv:
        if a.startswith("-"):
            if a == "-a":
                opts["print_ascii"] = True
            elif a == "-c":
                opts["print_c64"] = True
            elif a == "-b":
                opts["print_base40"] = True
            elif a == "-o":
                opts["print_base40_odd"] = True
            else:
                raise ArgExc("Invalid switch: " + a)
        else:
            args.append(a)
    if len(args) < 2:
        raise ArgExc("Too few arguments: " + str(argv))
    if len(args) > 3:
        raise ArgExc("Too many arguments: " + str(argv))
    filenames["lft_filename"] = args[0]
    filenames["rgt_filename"] = args[1]
    if len(args) == 3:
        filenames["diff_filename"] = args[2]
    else:
        filenames["diff_filename"] = "-"
    return filenames, opts

def main():
    filenames, opts = parse_args(sys.argv[1:])
    lft_filename = filenames["lft_filename"]
    rgt_filename = filenames["rgt_filename"]
    diff_filename = filenames["diff_filename"]
    with open(lft_filename, "rb") as fl:
        with open(rgt_filename, "rb") as fr:
            with (sys.stdout if diff_filename == "-" else open(diff_filename, "w", encoding="ascii")) as fd:
                d = Diff(fl, fr, **opts)
                d.diff(fd)

if __name__ == "__main__":
    try:
        main()
    except ArgExc as e:
        print(e, file=sys.stderr)

