#!/usr/bin/env python3
#
# setup.py
# This file specifies how a Python package is built and installed. This is
# required by setuptools and its descendants including cx_Freeze.
#
# The thought process:
# 1. Find a tool that can package all the dependencies of a Python script
# (cx_Freeze in this case),
# 2. Help it find all the dependencies of each our management util by
# setting `sys.path`, and
# 3. Modify config and Makefiles accordingly to use the packaging tool.
#
# Steps to build and install GPDB with standalone utils:
# 1. `cd` to the root directory of the GPDB source code.
# 2. `pip3 install --user -r python-developer-dependencies.txt`
# 3. `./configure --enable-standalone-utils <other_options_you_want>`
# 4. `make install`
# After installation completes, you can use the standalone utils in exactly
# the same way as before.
#
# NOTE:
# - After the utils are built into standalone binaries, __file__ will no longer
# be valid. Thus, we need to replace them with appropriate script names or
# paths.

import os
import sys
from cx_Freeze import setup, Executable

gpmgmt_dir = os.path.join(*os.path.split(os.path.abspath(__file__))[:-1])
sys.path = [
    os.path.join(gpmgmt_dir, "bin", "ext", "pygresql"),
    os.path.join(gpmgmt_dir, "bin", "ext"),
    os.path.join(gpmgmt_dir, "bin"),
] + sys.path

programs_buf = []
makefile = open("Makefile")
for line in makefile:
    if line.startswith("PROGRAMS=") or len(programs_buf) > 0:
        programs_buf.append(line)
        if "\\" not in line:
            break
makefile.close()

program_names = [
    name
    for line in programs_buf
    for name in line.replace("\\", "").split()
    if name != "PROGRAMS="
]


def is_python_program(name):
    prog = open(name)
    line = prog.readline()
    prog.close()
    return "python" in line


py_program_names = [name for name in program_names if is_python_program(name)]
print("building standalone management utils...", py_program_names)

setup(
    name="gpmgmt",
    version="0.1",
    executables=[
        Executable(name, target_name="__frozen__" + name) for name in py_program_names
    ],
)
